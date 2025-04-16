// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_ohos.h"

#include <native_buffer/native_buffer.h>
#include <surface_control.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"

namespace viz {

// static
std::unique_ptr<OutputPresenterOHOS> OutputPresenterOHOS::Create(
    SkiaOutputSurfaceDependency* deps) {
  // LOG(INFO) << "OutputPresenterOHOS::Create()";
  if (!base::FeatureList::IsEnabled(
          features::kUseSkiaOutputDeviceBufferQueue)) {
    return {};
  }

  return std::make_unique<OutputPresenterOHOS>(deps);
}

OutputPresenterOHOS::OutputPresenterOHOS(SkiaOutputSurfaceDependency* deps)
    : dependency_(deps),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // LOG(ERROR) << "EEE deps->GetSurfaceHandle()=" << deps->GetSurfaceHandle();
  root_surface_ = OH_SurfaceControl_FromNativeWindow(
      reinterpret_cast<OHNativeWindow*>(deps->GetSurfaceHandle()),
      "root_surface");
  CHECK(root_surface_);
  transaction_ = OH_SurfaceTransaction_Create();
}

OutputPresenterOHOS::~OutputPresenterOHOS() {
  OH_SurfaceTransaction_Delete(transaction_);
  if (root_surface_) {
    OH_SurfaceControl_Release(root_surface_);
  }
}

void OutputPresenterOHOS::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities->supports_post_sub_buffer = true;
  capabilities->supports_surfaceless = true;

  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kRGBA_8888_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kRGBA_8888_SkColorType;
}

bool OutputPresenterOHOS::Reshape(const ReshapeParams& params) {
  // LOG(ERROR) << "EEEE Reshape() size=" << params.GfxSize().ToString();
  size_ = params.GfxSize();
  OH_Rect bounds = {0, 0, size_.width(), size_.height()};
  OH_SurfaceTransaction_SetCrop(transaction_, root_surface_, &bounds);
  return true;
}

void OutputPresenterOHOS::Present(SwapCompletionCallback completion_callback,
                                  BufferPresentedCallback presentation_callback,
                                  gfx::FrameData data) {
  // Hide unused surfaces in this frame.
  for (auto* surface : avaliable_surfaces_) {
    OH_SurfaceTransaction_SetVisibility(transaction_, surface,
                                        OH_SURFACE_TRANSACTION_VISIBILITY_HIDE);
  }

  OH_SurfaceTransaction_Commit(transaction_);

  for (auto* surface : overlay_surfaces_) {
    avaliable_surfaces_.push_back(surface);
  }
  overlay_surfaces_.clear();

  completion_callbacks_.emplace(std::move(completion_callback));
  presentation_callbacks_.emplace(std::move(presentation_callback));
}

void OutputPresenterOHOS::OnOverlayReleased(ScopedOverlayAccess* access,
                                            bool is_root,
                                            base::ScopedFD release_fence_fd) {
  CHECK(!completion_callbacks_.empty());
  CHECK(!presentation_callbacks_.empty());

  if (release_fence_fd.is_valid()) {
    gfx::GpuFenceHandle fence_handle;
    fence_handle.Adopt(std::move(release_fence_fd));
    // LOG(ERROR) << "EEEE access->SetReleaseFence(std::move(fence_handle));
    // before";
    access->SetReleaseFence(std::move(fence_handle));
    // LOG(ERROR) << "EEEE access->SetReleaseFence(std::move(fence_handle));
    // after";
  }

  if (is_root) {
    // Trigger completion and presentation callbacks when the overlay is
    // released
    // TODO: figure out a better way to trigger those callbacks.
    std::move(completion_callbacks_.front())
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
    std::move(presentation_callbacks_.front())
        .Run(gfx::PresentationFeedback(base::TimeTicks::Now(),
                                       base::TimeDelta(),
                                       /*flags=*/0));
    completion_callbacks_.pop();
    presentation_callbacks_.pop();
  }
}

void OutputPresenterOHOS::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
    ScopedOverlayAccess* access) {
  bool is_root = overlay_plane_candidate.is_root_render_pass;
  OH_NativeBuffer* buffer = access->GetOHNativeBuffer();
  CHECK(buffer);

  base::ScopedFD fence_fd = access->TakeAcquireFence().Release();

  auto callback = base::BindOnce(&OutputPresenterOHOS::OnOverlayReleased,
                                 weak_factory_.GetWeakPtr(), access, is_root);

  // Make sure OnOverlayReleased() is called on GPU main thread.
  callback = base::BindPostTask(task_runner_, std::move(callback));

  using Context = base::OnceCallback<void(base::ScopedFD)>;
  auto context = std::make_unique<Context>(std::move(callback));

  auto* surface = is_root ? root_surface_ : GetOrCreateSurfaceControl();

  OH_SurfaceTransaction_SetBuffer(
      transaction_, surface, buffer, fence_fd.release(), context.release(),
      [](void* data, int release_fence_fd) {
        std::unique_ptr<Context> context(reinterpret_cast<Context*>(data));
        std::move(*context).Run(base::ScopedFD(release_fence_fd));
      });

  auto enclosing_rect = ToEnclosingRect(overlay_plane_candidate.damage_rect);
  OH_Rect damage_rect = {enclosing_rect.x(), enclosing_rect.y(),
                         enclosing_rect.width(), enclosing_rect.height()};
  OH_SurfaceTransaction_SetDamageRegion(transaction_, surface, &damage_rect, 1);

  if (!is_root) {
    const auto& display_rect = overlay_plane_candidate.display_rect;
    OH_SurfaceTransaction_SetPosition(transaction_, surface, display_rect.x(),
                                      display_rect.y());
    OH_SurfaceTransaction_SetFrame(transaction_, surface, 0, 0,
                                   display_rect.width(), display_rect.height());

    overlay_surfaces_.push_back(surface);
  }
}

OH_SurfaceControl* OutputPresenterOHOS::GetOrCreateSurfaceControl() {
  OH_SurfaceControl* surface = nullptr;
  if (avaliable_surfaces_.empty()) {
    surface = OH_SurfaceControl_Create("child_surface");
    OH_SurfaceTransaction_Reparent(transaction_, surface, root_surface_);
  } else {
    surface = avaliable_surfaces_.front();
    avaliable_surfaces_.pop_front();
    OH_SurfaceTransaction_SetVisibility(transaction_, surface,
                                        OH_SURFACE_TRANSACTION_VISIBILITY_SHOW);
  }
  return surface;
}

}  // namespace viz
