// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_ohos.h"

#include <native_buffer/native_buffer.h>
#include <surface_control.h>

#include <algorithm>
#include <bitset>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
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
namespace {

constexpr uint32_t kBorderWidth = 4;
constexpr std::array<float, 4> kColorRed = {1.0f, 0.0f, 0.0f, 1.0f};
constexpr std::array<float, 4> kColorRedTint = {1.0f, 0.0f, 0.0f, 0.3f};
constexpr std::array<float, 4> kColorBlueTint = {0.0f, 0.0f, 1.0f, 0.3f};
constexpr std::array<float, 4> kColorGreenTint = {0.0f, 1.0f, 0.0f, 0.3f};
constexpr std::array<float, 4> kColorTransparent = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kDepthGap = 50.0f;

bool IsDelegatedCompositingEnabled() {
  const char kEnableDelegatedCompositing[] = "enable-delegated-compositing";
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableDelegatedCompositing);
  return enabled;
}

bool IsDebugEnabled() {
  const char kEnableDelegatedCompositing[] =
      "enable-delegated-compositing-debugging";
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableDelegatedCompositing);
  return enabled;
}

}  // namespace
class OutputPresenterOHOS::SurfaceControl {
 public:
  using ReleaseCallback =
      base::OnceCallback<void(base::ScopedFD release_fence_fd)>;

  explicit SurfaceControl(SurfaceControl* parent) : parent_(parent->surface_) {}
  explicit SurfaceControl(OHNativeWindow* window) {
    surface_ = OH_SurfaceControl_FromNativeWindow(window, "root_surface");
  }

  ~SurfaceControl() {
    DCHECK(dirty_bits_.none());
    if (surface_) {
      OH_SurfaceControl_Release(surface_);
    }
  }

  void SetSurface(OH_SurfaceControl* surface) {
    DCHECK(!surface_);
    surface_ = surface;
    dirty_bits_.set();
  }

  OH_SurfaceControl* ReleaseSurface() {
    DCHECK(parent_);
    auto* surface = surface_;
    surface_ = nullptr;
    return surface;
  }

  void SetZOrder(int32_t z_order) { z_order_ = z_order; }

  void SetVisible(bool visible) {
    if (visible_ != visible) {
      visible_ = visible;
      dirty_bits_.set(kVisible);
    }
  }

  void SetBounds(const gfx::RectF& bounds) {
    if (bounds_ != bounds) {
      bounds_ = bounds;
      dirty_bits_.set(kBounds);
    }
  }

  void SetFrame(const gfx::RectF& frame) {
    if (frame_ != frame) {
      frame_ = frame;
      dirty_bits_.set(kFrame);
    }
  }

  void SetScale(float scale_x, float scale_y) {
    std::array<float, 2> scale = {scale_x, scale_y};
    if (scale_ != scale) {
      scale_ = scale;
      dirty_bits_.set(kScale);
    }
  }

  void SetTranslate(float translate_x, float translate_y, float translate_z) {
    std::array<float, 3> translate = {translate_x, translate_y, translate_z};
    if (translate_ != translate) {
      translate_ = translate;
      dirty_bits_.set(kTranslate);
    }
  }

  void SetPivot(float x, float y) {
    std::array<float, 2> pivot = {x, y};
    if (pivot_ != pivot) {
      pivot_ = pivot;
      dirty_bits_.set(kPivot);
    }
  }

  void SetBorderWidth(int32_t width) {
    if (border_width_ != width) {
      border_width_ = width;
      dirty_bits_.set(kBorderWidth);
    }
  }

  void SetBorderColor(const std::array<float, 4>& color) {
    if (border_color_ != color) {
      border_color_ = color;
      dirty_bits_.set(kBorderColor);
    }
  }

  void SetBorderStyle(int32_t style) {
    if (border_style_ != style) {
      border_style_ = style;
      dirty_bits_.set(kBorderStyle);
    }
  }

  void SetForegroundColor(const std::array<float, 4>& color) {
    if (foreground_color_ != color) {
      foreground_color_ = color;
      dirty_bits_.set(kForegroundColor);
    }
  }

  void SetAlpha(float alpha) {
    if (alpha_ != alpha) {
      alpha_ = alpha;
      dirty_bits_.set(kAlpha);
    }
  }

  void SetBuffer(OH_NativeBuffer* buffer,
                 base::ScopedFD fence_fd,
                 const gfx::Rect& damage_rect,
                 ReleaseCallback release_callback) {
    DCHECK(!release_callback_);
    DCHECK_NE(buffer_, buffer);
    buffer_ = buffer;
    fence_fd_ = std::move(fence_fd);
    damage_rect_ = damage_rect;
    release_callback_ = std::move(release_callback);
    dirty_bits_.set(kBuffer);
  }

  void Sync(OH_SurfaceTransaction* transaction) {
    if (!surface_) {
      surface_ = OH_SurfaceControl_Create("child_surface");
      OH_SurfaceTransaction_Reparent(transaction, surface_, parent_);
      OH_SurfaceTransaction_SetVisibility(
          transaction, surface_, OH_SURFACE_TRANSACTION_VISIBILITY_SHOW);
    }

    // Always set zorder
    OH_SurfaceTransaction_SetZOrder(transaction, surface_, z_order_);

    if (dirty_bits_.test(kBounds)) {
      OH_SurfaceTransaction_SetBounds(transaction, surface_, bounds_.x(),
                                      bounds_.y(), bounds_.width(),
                                      bounds_.height());
    }

    if (dirty_bits_.test(kFrame)) {
      OH_SurfaceTransaction_SetFrame(transaction, surface_, frame_.x(),
                                     frame_.y(), frame_.width(),
                                     frame_.height());
    }

    if (dirty_bits_.test(kScale)) {
      OH_SurfaceTransaction_SetScale(transaction, surface_, scale_[0],
                                     scale_[1], 1.0);
    }
    if (dirty_bits_.test(kTranslate)) {
      OH_SurfaceTransaction_SetTranslate(transaction, surface_, translate_[0],
                                         translate_[1], translate_[2]);
    }

    if (dirty_bits_.test(kPivot)) {
      OH_SurfaceTransaction_SetPivot(transaction, surface_, pivot_[0],
                                     pivot_[1]);
    }
    if (dirty_bits_.test(kBorderWidth)) {
      OH_SurfaceTransaction_SetBorderWidth(transaction, surface_, border_width_,
                                           border_width_, border_width_,
                                           border_width_);
    }
    if (dirty_bits_.test(kBorderColor)) {
      OH_SurfaceTransaction_SetBorderColor(transaction, surface_,
                                           border_color_[0], border_color_[1],
                                           border_color_[2], border_color_[3]);
    }
    if (dirty_bits_.test(kBorderStyle)) {
      OH_SurfaceTransaction_SetBorderStyle(transaction, surface_, border_style_,
                                           border_style_, border_style_,
                                           border_style_);
    }
    if (dirty_bits_.test(kForegroundColor)) {
      OH_SurfaceTransaction_SetForegroundColor(
          transaction, surface_, foreground_color_[0], foreground_color_[1],
          foreground_color_[2], foreground_color_[3]);
    }
    if (dirty_bits_.test(kAlpha)) {
      OH_SurfaceTransaction_SetBufferAlpha(transaction, surface_, alpha_);
    }

    if (dirty_bits_.test(kBuffer)) {
      auto context =
          release_callback_
              ? std::make_unique<ReleaseCallback>(std::move(release_callback_))
              : std::unique_ptr<ReleaseCallback>();
      OH_SurfaceTransaction_SetBuffer(
          transaction, surface_, buffer_, fence_fd_.release(),
          context.release(), [](void* context, int32_t release_fence_fd) {
            std::unique_ptr<ReleaseCallback> callback(
                reinterpret_cast<ReleaseCallback*>(context));
            if (callback) {
              std::move(*callback).Run(base::ScopedFD(release_fence_fd));
            }
          });
      OH_Rect rect = {damage_rect_.x(), damage_rect_.y(), damage_rect_.width(),
                      damage_rect_.height()};
      OH_SurfaceTransaction_SetDamageRegion(transaction, surface_, &rect, 1);
    }
    dirty_bits_.reset();
  }

  bool has_surface() const { return surface_ != nullptr; }
  int32_t z_order() const { return z_order_; }
  bool is_visible() const { return visible_; }
  const gfx::RectF bounds() const { return bounds_; }
  const gfx::RectF frame() const { return frame_; }
  OH_NativeBuffer* buffer() const { return buffer_; }

 private:
  enum DirtyBits {
    kVisible,
    kBounds,
    kFrame,
    kScale,
    kTranslate,
    kPivot,
    kBuffer,
    kBorderWidth,
    kBorderColor,
    kBorderStyle,
    kForegroundColor,
    kAlpha,
    kCount,
  };
  std::bitset<kCount> dirty_bits_;

  OH_SurfaceControl* parent_ = nullptr;
  OH_SurfaceControl* surface_ = nullptr;
  int32_t z_order_ = -1;
  bool visible_ = true;
  gfx::RectF bounds_;
  gfx::RectF frame_;
  std::array<float, 2> scale_ = {1.0, 1.0};
  std::array<float, 3> translate_ = {0.0, 0.0, 0.0};
  std::array<float, 2> pivot_ = {0.5, 0.5};
  std::array<float, 4> border_color_ = {0.0, 0.0, 0.0, 0.0};
  int32_t border_width_ = 0;
  int32_t border_style_ = OH_SURFACE_TRANSACTION_BORDER_STYLE_SOLID;
  std::array<float, 4> foreground_color_ = {0.0, 0.0, 0.0, 0.0};
  float alpha_ = 1.0f;

  OH_NativeBuffer* buffer_ = nullptr;
  base::ScopedFD fence_fd_;
  gfx::Rect damage_rect_;
  ReleaseCallback release_callback_;
};

// static
std::unique_ptr<OutputPresenterOHOS> OutputPresenterOHOS::Create(
    SkiaOutputSurfaceDependency* deps) {
  if (!IsDelegatedCompositingEnabled()) {
    return {};
  }

  return std::make_unique<OutputPresenterOHOS>(deps);
}

OutputPresenterOHOS::OutputPresenterOHOS(SkiaOutputSurfaceDependency* deps)
    : dependency_(deps),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      transaction_(OH_SurfaceTransaction_Create()) {
  root_surface_ = std::make_unique<SurfaceControl>(
      reinterpret_cast<OHNativeWindow*>(deps->GetSurfaceHandle()));
  auto callback = base::BindRepeating(&OutputPresenterOHOS::OnComplete,
                                      weak_factory_.GetWeakPtr());

  callback = base::BindPostTask(task_runner_, std::move(callback));
  on_complete_callback_ = std::make_unique<OnCompleteCallback>(callback);
  OH_SurfaceTransaction_SetOnComplete(
      transaction_, on_complete_callback_.get(),
      [](void* context, uint64_t timestamp) {
        auto* callback = reinterpret_cast<OnCompleteCallback*>(context);
        callback->Run(timestamp);
      });
}

OutputPresenterOHOS::~OutputPresenterOHOS() {
  OH_SurfaceTransaction_SetOnComplete(transaction_, nullptr, nullptr);
  OH_SurfaceTransaction_Delete(transaction_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  size_ = params.GfxSize();
  root_surface_->SetBounds(gfx::RectF(size_));
  root_surface_->SetFrame(gfx::RectF(size_));
  return true;
}

void OutputPresenterOHOS::Present(SwapCompletionCallback completion_callback,
                                  BufferPresentedCallback presentation_callback,
                                  gfx::FrameData data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  root_surface_->Sync(transaction_);

  // sort pending frame by z order
  std::sort(pending_frame_.begin(), pending_frame_.end(),
            [](const std::unique_ptr<SurfaceControl>& a,
               const std::unique_ptr<SurfaceControl>& b) {
              return a->z_order() < b->z_order();
            });
  // LOG(ERROR) << "EEEE pending_frame_.size() = " << pending_frame_.size();
  // LOG(ERROR) << "EEEE pending_frame_.front()->z_order() = "
  //            << pending_frame_.front()->z_order();

  for (auto& surface : pending_frame_) {
    // Try reuse OH_SurfaceControl from previous frame.
    if (!surface->has_surface() && !previous_frame_.empty()) {
      surface->SetSurface(previous_frame_.front()->ReleaseSurface());
      previous_frame_.pop_front();
    }
    // surface->SetTranslate(0.0f, 0.0f, depth);
    surface->Sync(transaction_);
  }

  // Clear all unused surface from previous frame.
  while (!previous_frame_.empty()) {
    auto surface = std::move(previous_frame_.front());
    DCHECK(surface->has_surface());
    previous_frame_.pop_front();
    surface->SetVisible(false);
    surface->Sync(transaction_);
    // Reusing SurfaceControl cause junk
    // avaliable_surfaces_.push_back(std::move(surface));
    surface.reset();
  }

  OH_SurfaceTransaction_Commit(transaction_);

  previous_frame_ = std::move(pending_frame_);

  completion_callbacks_.emplace(std::move(completion_callback));
  presentation_callbacks_.emplace(std::move(presentation_callback));
}

void OutputPresenterOHOS::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane,
    ScopedOverlayAccess* access) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const bool is_root = overlay_plane.is_root_render_pass;
  OH_NativeBuffer* buffer = access ? access->GetOHNativeBuffer() : nullptr;

  // if (is_root) {
  //   LOG(ERROR) << "EEEE Schedule Root OverlayPlane buffer=" << buffer;
  // } else {

  // }
  // LOG(ERROR) << "EEEE Schedule OverlayPlane overlay_plane.is_solid_color="
  //            << overlay_plane.is_solid_color;
  // LOG(ERROR) << "EEEE Schedule OverlayPlane overlay_plane.is_opaque = "
  //            << overlay_plane.is_opaque;
  if (buffer == nullptr) {
    // TODO: need to set null buffer to surface
    return;
  }

  SurfaceControl* surface = nullptr;
  bool buffer_changed = false;

  // Find surface with same buffer in previous frame first.
  auto it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                         [&](const std::unique_ptr<SurfaceControl>& surface) {
                           return surface->buffer() == buffer;
                         });
  buffer_changed = (it == previous_frame_.end());
  if (buffer_changed) {
    // Find surface with same display rect in previous frame.
    it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                      [&](const std::unique_ptr<SurfaceControl>& surface) {
                        return surface->bounds() == overlay_plane.display_rect;
                      });
  }

  if (it != previous_frame_.end()) {
    // Reuse surface from previous frame.
    surface = it->get();
    pending_frame_.push_back(std::move(*it));
    previous_frame_.erase(it);
  } else {
    if (!avaliable_surfaces_.empty()) {
      // Reuse a surface from avaliable_surfaces_.
      pending_frame_.push_back(std::move(avaliable_surfaces_.front()));
      pending_frame_.back()->SetVisible(true);
      avaliable_surfaces_.pop_front();
    } else {
      pending_frame_.push_back(
          std::make_unique<SurfaceControl>(root_surface_.get()));
    }
  }
  surface = pending_frame_.back().get();

  if (buffer_changed) {
    base::ScopedFD fence_fd = access->TakeAcquireFence().Release();

    auto callback = base::BindOnce(&OutputPresenterOHOS::OnOverlayReleased,
                                   weak_factory_.GetWeakPtr(),
                                   base::UnsafeDangling(access), is_root);

    // Make sure OnOverlayReleased() is called on GPU main thread.
    callback = base::BindPostTask(task_runner_, std::move(callback));

    if (!is_root) {
      access->InUseByWindowServerInc();
    }
    surface->SetBuffer(buffer, std::move(fence_fd),
                       ToEnclosingRect(overlay_plane.damage_rect),
                       std::move(callback));
    surface->SetAlpha(overlay_plane.opacity);
  }

  if (IsDebugEnabled()) {
    surface->SetBorderWidth(kBorderWidth);
    surface->SetBorderColor(kColorRed);
    surface->SetBorderStyle(OH_SURFACE_TRANSACTION_BORDER_STYLE_SOLID);
    if (overlay_plane.is_opaque) {
      surface->SetForegroundColor(kColorRedTint);
    } else {
      surface->SetForegroundColor(kColorGreenTint);
    }
  }

  if (is_root) {
    surface->SetBounds(gfx::RectF(size_));
    surface->SetFrame(gfx::RectF(size_));
    surface->SetZOrder(0);
  } else {
    // TODO: figure outt how to use overlay_plane.clip_rect

    gfx::SizeF buffer_size(overlay_plane.resource_size_in_pixels);

    // source rect which is in the source buffer coordinate
    gfx::RectF src_rect = gfx::ScaleRect(overlay_plane.uv_rect, buffer_size);

    // display rect which is in the display coordinate.
    const gfx::RectF& display_rect = overlay_plane.display_rect;

    // When the video is being scrolled offscreen DisplayCompositor will crop it
    // to only visible portion and adjust uv_rect accordingly. When the video
    // is smaller than the surface is can lead to the crop rect being less than
    // a pixel in size. This adjusts the crop rect size to at least 1 pixel as
    // we want to stretch last visible pixel line/column in this case.
    // Note: We will do it even if crop_rect width/height is exact 0.0f. In
    // reality this should never happen and there is no way to display video
    // with empty crop rect, so display compositor should not request this.
    if (src_rect.width() == 0) {
      src_rect.set_width(1);
      if (src_rect.right() > buffer_size.width()) {
        src_rect.set_x(buffer_size.width() - 1);
      }
    }
    if (src_rect.height() == 0) {
      src_rect.set_height(1);
      if (src_rect.bottom() > buffer_size.height()) {
        src_rect.set_y(buffer_size.height() - 1);
      }
    }

    src_rect.Intersect(gfx::RectF(buffer_size));
    DCHECK(src_rect.width() > 0);
    DCHECK(src_rect.height() > 0);

    float scale_x = display_rect.width() / src_rect.width();
    float scale_y = display_rect.height() / src_rect.height();
    surface->SetPivot(0, 0);
    surface->SetScale(scale_x, scale_y);

    gfx::RectF bounds_rect = gfx::RectF(display_rect.origin(), src_rect.size());

    gfx::RectF frame_rect = gfx::RectF(display_rect.origin(), buffer_size);
    frame_rect.Offset(-src_rect.x(), -src_rect.y());

    surface->SetBounds(bounds_rect);
    surface->SetFrame(frame_rect);

    CHECK_GT(overlay_plane.plane_z_order, 0);
    surface->SetZOrder(overlay_plane.plane_z_order);
  }
}

void OutputPresenterOHOS::OnComplete(uint64_t timestamp) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(!completion_callbacks_.empty());
  DCHECK(!presentation_callbacks_.empty());

  gfx::SwapCompletionResult swap_result(gfx::SwapResult::SWAP_ACK);
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::TimeDelta(),
                                     /*flags=*/0);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callbacks_.front()),
                                std::move(swap_result)));

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(presentation_callbacks_.front()),
                                std::move(feedback)));

  completion_callbacks_.pop();
  presentation_callbacks_.pop();
}

void OutputPresenterOHOS::OnOverlayReleased(
    MayBeDangling<ScopedOverlayAccess> access,
    bool is_root,
    base::ScopedFD release_fence_fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (release_fence_fd.is_valid() && !is_root) {
    gfx::GpuFenceHandle fence_handle;
    fence_handle.Adopt(std::move(release_fence_fd));
    access->SetReleaseFence(std::move(fence_handle));
  }
  if (!is_root) {
    access->InUseByWindowServerDec();
  }
}

}  // namespace viz
