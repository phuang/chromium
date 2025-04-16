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
  void OnOverlayReleased(ScopedOverlayAccess* access,
                         bool is_root,
                         base::ScopedFD release_fence_fd);

  OH_SurfaceControl* GetOrCreateSurfaceControl();

  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  gfx::Size size_;
  OH_SurfaceTransaction* transaction_ = nullptr;
  OH_SurfaceControl* root_surface_ = nullptr;

  base::circular_deque<OH_SurfaceControl*> overlay_surfaces_;
  // Unused surfaces
  base::circular_deque<OH_SurfaceControl*> avaliable_surfaces_;

  base::queue<SwapCompletionCallback> completion_callbacks_;
  base::queue<BufferPresentedCallback> presentation_callbacks_;

  base::WeakPtrFactory<OutputPresenterOHOS> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_OHOS_H_
