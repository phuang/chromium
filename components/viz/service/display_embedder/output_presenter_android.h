// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_ANDROID_H_

#include <deque>
#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface_control.h"

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT OutputPresenterAndroid final : public OutputPresenter {
 public:
  static std::unique_ptr<OutputPresenterAndroid> Create(
      SkiaOutputSurfaceDependency* deps);

  OutputPresenterAndroid(SkiaOutputSurfaceDependency* deps,
                         gl::ScopedANativeWindow window);
  OutputPresenterAndroid(SkiaOutputSurfaceDependency* deps,
                         gl::ScopedJavaSurfaceControl surface_control);
  ~OutputPresenterAndroid() override;

  // OutputPresenter implementation:
  void InitializeCapabilities(
      OutputSurface::Capabilities* capabilities) override;
  bool Reshape(const ReshapeParams& params) override;
  void Present(SwapCompletionCallback completion_callback,
               BufferPresentedCallback presentation_callback,
               gfx::FrameData data) override;
  void ScheduleOverlayPlane(
      const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
      ScopedOverlayAccess* access) override;

 private:
  class Surface;

  // Called when the transaction is committed.
  void OnCommit();

  // Called when the transaction is completed, i.e. all buffers are presented.
  void OnComplete(gfx::SurfaceControl::TransactionStats stats);

  // Called when a buffer is released by SurfaceFlinger.
  class ScopedBufferReleaseData;
  void OnBufferReleased(ScopedBufferReleaseData data,
                        base::ScopedFD release_fence_fd);
  void TriggerPendingCompletionCallbacks();

  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::circular_deque<gfx::SurfaceControl::Transaction>
      pending_transaction_queue_;
  bool has_pending_transaction_ack_ = false;

  gfx::Size size_;
  gfx::OverlayTransform transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // The root surface.
  std::unique_ptr<Surface> root_surface_;
  // The surfaces will be used in the next submitted frame.
  std::deque<std::unique_ptr<Surface>> pending_frame_;
  // The surfaces are used in the previouse submitted frame.
  std::deque<std::unique_ptr<Surface>> previous_frame_;
  // The surfaces are not used in the previouse submitted frame, they can be
  // reused.
  base::raw_ptr<ScopedOverlayAccess> root_overlay_access_;

  struct FrameData {
    FrameData(base::raw_ptr<ScopedOverlayAccess> access,
              SwapCompletionCallback completion_callback,
              BufferPresentedCallback presentation_callback);
    ~FrameData();
    FrameData(FrameData&& other);
    FrameData& operator=(FrameData&& other);

    bool completed = false;
    base::raw_ptr<ScopedOverlayAccess> access;
    SwapCompletionCallback completion_callback;
    BufferPresentedCallback presentation_callback;
  };
  base::circular_deque<FrameData> pending_commit_frame_data_queue_;
  base::circular_deque<FrameData> pending_complete_frame_data_queue_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<OutputPresenterAndroid> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_ANDROID_H_
