// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_FULL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_FULL_H_

#include "components/viz/service/display/overlay_processor_delegated_support.h"
#include "components/viz/service/display/overlay_processor_interface.h"

namespace viz {

// This is an overlay processor implementation for Android SurfaceControl.
class VIZ_SERVICE_EXPORT OverlayProcessorSurfaceControlFull final
    : public OverlayProcessorInterface {
 public:
  OverlayProcessorSurfaceControlFull();
  ~OverlayProcessorSurfaceControlFull() override;

  OverlayProcessorSurfaceControlFull(
      const OverlayProcessorSurfaceControlFull&) = delete;
  OverlayProcessorSurfaceControlFull& operator=(
      const OverlayProcessorSurfaceControlFull&) = delete;

  // Override OverlayProcessorInterface.
  bool DisableSplittingQuads() const override;

  bool IsOverlaySupported() const override;
  gfx::Rect GetAndResetOverlayDamage() override;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  bool NeedsSurfaceDamageRectList() const override;

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      std::optional<OverlayCandidate>& primary_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) override;

  gfx::RectF GetUnassignedDamage() const override;

 private:
  bool ProcessForOverlaysImpl(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      std::optional<OverlayCandidate>& primary_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

  // gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  // gfx::Size viewport_size_;
  bool overlays_allowed_ = true;
  gfx::Rect ca_overlay_damage_rect_;
  gfx::RectF unassigned_damage_;
  DelegationStatus delegated_status_ = DelegationStatus::kCompositedOther;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_FULL_H_
