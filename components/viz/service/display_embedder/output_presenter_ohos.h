// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_OHOS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_OHOS_H_

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"

typedef struct OH_SurfaceControl OH_SurfaceControl;
typedef struct OH_SurfaceTransaction OH_SurfaceTransaction;
typedef struct OH_NativeBuffer OH_NativeBuffer;

namespace {
class SequencedTaskRunner;
}

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT OutputPresenterOHOS : public OutputPresenter {
 public:
  static std::unique_ptr<OutputPresenterOHOS> Create(
      SkiaOutputSurfaceDependency* deps);

  explicit OutputPresenterOHOS(SkiaOutputSurfaceDependency* deps);
  ~OutputPresenterOHOS() override;

  // OutputPresenter implementation:
  void InitializeCapabilities(OutputSurface::Capabilities* capabilities) final;
  bool Reshape(const ReshapeParams& params) final;
  void Present(SwapCompletionCallback completion_callback,
               BufferPresentedCallback presentation_callback,
               gfx::FrameData data) final;
  void ScheduleOverlayPlane(
      const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
      ScopedOverlayAccess* access) final;

 private:
  class SurfaceControl;

  void OnComplete(uint64_t timestamp);
  void OnOverlayReleased(MayBeDangling<ScopedOverlayAccess> access,
                         bool is_root,
                         base::ScopedFD release_fence_fd);

  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  OH_SurfaceTransaction* const transaction_ ;

  gfx::Size size_;
  gfx::OverlayTransform transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // The root surface.
  std::unique_ptr<SurfaceControl> root_surface_;
  // The surfaces will be used in the next submitted frame.
  std::deque<std::unique_ptr<SurfaceControl>> pending_frame_;
  // The surfaces are used in the previouse submitted frame.
  std::deque<std::unique_ptr<SurfaceControl>> previous_frame_;
  // The surfaces are not used in the previouse submitted frame, they can be
  // reused.
  std::deque<std::unique_ptr<SurfaceControl>> avaliable_surfaces_;

  ScopedOverlayAccess* root_overlay_access_ = nullptr;

  base::circular_deque<SwapCompletionCallback> completion_callbacks_;
  base::circular_deque<BufferPresentedCallback> presentation_callbacks_;

  // on complete callback for OH_SurfaceTransaction.
  using OnCompleteCallback = base::RepeatingCallback<void(uint64_t timestamp)>;
  std::unique_ptr<OnCompleteCallback> on_complete_callback_;

  base::WeakPtrFactory<OutputPresenterOHOS> weak_factory_{this};

  THREAD_CHECKER(thread_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_OHOS_H_
