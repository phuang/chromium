// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OHOS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OHOS_H_

#include <optional>

#include "components/viz/service/display/overlay_processor_delegated_support.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"

namespace viz {

// This is an overlay processor implementation for OHOS.
class VIZ_SERVICE_EXPORT OverlayProcessorOHOS
    : public OverlayProcessorUsingStrategy {
 public:
  OverlayProcessorOHOS();
  ~OverlayProcessorOHOS() override;

  static std::optional<gfx::ColorSpace> GetOverrideColorSpace();

  // Override OverlayProcessorInterface:
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) override;
  bool IsOverlaySupported() const override;
  bool NeedsSurfaceDamageRectList() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& size) override;
  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;
  void CheckOverlaySupportImpl(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;
  bool SupportsFlipRotateTransform() const override;

 private:
  bool AttemptWithStrategies(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds);

  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size viewport_size_;
  gfx::RectF unassigned_damage_;
  DelegationStatus delegated_status_ = DelegationStatus::kCompositedOther;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OHOS_H_
