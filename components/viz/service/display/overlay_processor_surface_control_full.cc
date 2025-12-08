// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_surface_control_full.h"

#include "components/viz/service/display/overlay_candidate_factory.h"

namespace viz {
namespace {

gfx::RectF GetPrimaryPlaneDisplayRect(
    const std::optional<OverlayCandidate>& primary_plane) {
  return primary_plane.has_value() ? primary_plane->display_rect : gfx::RectF();
}

}  // namespace

OverlayProcessorSurfaceControlFull::OverlayProcessorSurfaceControlFull() = default;

OverlayProcessorSurfaceControlFull::~OverlayProcessorSurfaceControlFull() =
    default;

bool OverlayProcessorSurfaceControlFull::DisableSplittingQuads() const {
  return false;
}

bool OverlayProcessorSurfaceControlFull::IsOverlaySupported() const {
  return true;
}

gfx::Rect OverlayProcessorSurfaceControlFull::GetAndResetOverlayDamage() {
  gfx::Rect result = ca_overlay_damage_rect_;
  ca_overlay_damage_rect_ = gfx::Rect();
  return result;
}

bool OverlayProcessorSurfaceControlFull::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorSurfaceControlFull::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkM44& output_color_matrix,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    std::optional<OverlayCandidate>& primary_plane,
    CandidateList* overlay_candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  auto* render_pass = render_passes->back().get();
  bool result = ProcessForOverlaysImpl(
      resource_provider, render_pass, output_color_matrix, render_pass_filters,
      render_pass_backdrop_filters, surface_damage_rect_list, primary_plane,
      overlay_candidates, damage_rect, content_bounds);
  if (result) {
    // Set |ca_overlay_damage_rect_| to be everything, so that the next
    // composite that we draw to the output surface will do a full re-draw.
    ca_overlay_damage_rect_ = render_pass->output_rect;

    // Everything in |render_pass->quad_list| has been moved over to
    // |candidates|. Ideally we would clear |render_pass->quad_list|, but some
    // RenderPass overlays still point into that list. So instead, to avoid
    // drawing the root RenderPass, we set |damage_rect| to be empty.
    *damage_rect = gfx::Rect();
  } else {
    render_pass->has_transparent_background |= !primary_plane->is_opaque;
    overlay_candidates->push_back(std::move(primary_plane).value());
    primary_plane.reset();
  }
}

gfx::RectF OverlayProcessorSurfaceControlFull::GetUnassignedDamage() const {
  return unassigned_damage_;
}

bool OverlayProcessorSurfaceControlFull::ProcessForOverlaysImpl(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPass* render_pass,
    const SkM44& output_color_matrix,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    std::optional<OverlayCandidate>& primary_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  auto* quad_list = &render_pass->quad_list;
  if (!overlays_allowed_) {
    delegated_status_ = DelegationStatus::kCompositedFeatureDisabled;
    return false;
  }

  constexpr size_t kTooManyQuads = 64;
  if (quad_list->size() >= kTooManyQuads) {
    delegated_status_ = DelegationStatus::kCompositedTooManyQuads;
    return false;
  }

  if (!render_pass_backdrop_filters.empty()) {
    delegated_status_ = DelegationStatus::kCompositedBackdropFilter;
    return false;
  }

  if (!render_pass->copy_requests.empty()) {
    delegated_status_ = DelegationStatus::kCompositedCopyRequest;
    return false;
  }

  static const OverlayCandidateFactory::OverlayContext context = [] {
    OverlayCandidateFactory::OverlayContext context;
    context.is_delegated_context = true;
    context.disable_wire_size_optimization = false;
    context.supports_clip_rect = true;
    context.supports_out_of_window_clip_rect = true;
    context.supports_arbitrary_transform = false;
    context.supports_rounded_display_masks = false;
    context.supports_mask_filter = false;
    context.transform_and_clip_rpdq = false;
    context.supports_flip_rotate_transform = false;
    return context;
  }();

  OverlayCandidateFactory candidate_factory(
      render_pass, resource_provider, &surface_damage_rect_list,
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane),
      &render_pass_filters, context);

  unassigned_damage_ = gfx::RectF(candidate_factory.GetUnassignedDamage());

  candidates->reserve(quad_list->size());
  int num_quads_skipped = 0;

  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    if (auto result = TryPromoteDrawQuadForDelegation(candidate_factory, *it);
        result.has_value()) {
      if (const auto& candidate = result.value(); candidate.has_value()) {
        candidates->push_back(candidate.value());
      } else {
        // This quad can be intentionally skipped.
        num_quads_skipped++;
      }
    } else {
      delegated_status_ = result.error();
    }
  }

  if (candidates->empty() ||
      (candidates->size() + num_quads_skipped) != quad_list->size()) {
    candidates->clear();
    return false;
  }

  int curr_plane_order = candidates->size();
  for (auto&& each : *candidates) {
    each.plane_z_order = curr_plane_order--;
  }

  // We cannot erase the quads that were handled as overlays because raw
  // pointers of the aggregate draw quads were placed in the |rpdq| member of
  // the |OverlayCandidate|. As keeping with the pattern in
  // overlay_processor_mac we will also set the damage to empty on the
  // successful promotion of all quads.
  delegated_status_ = DelegationStatus::kFullDelegation;
  return true;
}

}  // namespace viz
