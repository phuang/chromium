// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_ohos.h"

#include <memory>
#include <optional>

// #include "base/android/build_info.h"
#include "base/feature_list.h"
#include "cc/base/math_util.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
// #include "ui/gfx/android/android_surface_control_compat.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {
namespace {

BASE_FEATURE(kOHOSSingleOnTOp,
             "OHOSSingleOnTOp",
             base::FEATURE_ENABLED_BY_DEFAULT);

gfx::RectF ClipFromOrigin(gfx::RectF input) {
  if (input.x() < 0.f) {
    input.set_width(input.width() + input.x());
    input.set_x(0.f);
  }

  if (input.y() < 0) {
    input.set_height(input.height() + input.y());
    input.set_y(0.f);
  }

  return input;
}

gfx::RectF GetPrimaryPlaneDisplayRect(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane) {
  return primary_plane ? primary_plane->display_rect : gfx::RectF();
}

}  // namespace

OverlayProcessorOHOS::OverlayProcessorOHOS() {
  // Android webview never sets |frame_sequence_number_| for the overlay
  // processor. Android Chrome does set this variable because it does call draw.
  // However, it also may not update this variable when displaying an overlay.
  // Therefore, our damage tracking for overlays is incorrect and we must ignore
  // the thresholding of prioritization.

  // TODO(crbug.com/40236858): We should take issue into account when trying to
  // find a replacement for number-of-scanouts.
  prioritization_config_.changing_threshold = false;
  prioritization_config_.damage_rate_threshold = false;

  // strategies_.push_back(std::make_unique<OverlayStrategyFullDelegation>(this));
  // strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
  // strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
  //     this,
  //     OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  // strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
}

OverlayProcessorOHOS::~OverlayProcessorOHOS() = default;

void OverlayProcessorOHOS::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkM44& output_color_matrix,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  DCHECK(candidates->empty());
  bool success = false;
  // #if !BUILDFLAG(IS_APPLE)
  //   RecordFDUsageUMA();
  // #endif

  DebugLogBeforeDelegation(*damage_rect, surface_damage_rect_list);

  success = AttemptWithStrategies(
      output_color_matrix, render_pass_filters, render_pass_backdrop_filters,
      resource_provider, render_passes, &surface_damage_rect_list,
      output_surface_plane, candidates, content_bounds);

  DCHECK(candidates->empty() || success);

  if (success) {
    overlay_damage_rect_ = *damage_rect;
    // Save all the damage for the case when we fail delegation.
    previous_frame_overlay_rect_.Union(*damage_rect);
    // All quads handled. Primary plane damage is zero.
    *damage_rect = gfx::Rect();
  } else {
    overlay_damage_rect_ = previous_frame_overlay_rect_;
    // Add in all the damage from all fully delegated frames.
    damage_rect->Union(previous_frame_overlay_rect_);
    previous_frame_overlay_rect_ = gfx::Rect();
    // This is only relevant when delegating.
    unassigned_damage_ = gfx::RectF();
  }

  DebugLogAfterDelegation(delegated_status_, *candidates, *damage_rect);
}

bool OverlayProcessorOHOS::IsOverlaySupported() const {
  return true;
}

bool OverlayProcessorOHOS::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorOHOS::CheckOverlaySupportImpl(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates) {
  DCHECK(!candidates->empty());

  for (auto& candidate : *candidates) {
    if (auto override_color_space = GetOverrideColorSpace()) {
      candidate.color_space = override_color_space.value();
      candidate.hdr_metadata = gfx::HDRMetadata();
    }

    // Check if the ColorSpace is supported
    // if (!gfx::OHOS::SupportsColorSpace(candidate.color_space)) {
    //   candidate.overlay_handled = false;
    //   return;
    // }

    // Aggregator adds `display_transform_` to all quads, which is then added to
    // `candidate.transform` here. `display_transform_` only applies to content
    // on the main plane so it needs to be removed candidate it its own plane.
    gfx::OverlayTransform candidate_overlay_transform = OverlayTransformsConcat(
        absl::get<gfx::OverlayTransform>(candidate.transform),
        InvertOverlayTransform(display_transform_));
    // Note the transform below using `candidate_overlay_transform` to compute
    // clipped and normalized `uv_rect` is only tested with NONE and
    // FLIP_VERTICAL.
    if (candidate_overlay_transform != gfx::OVERLAY_TRANSFORM_NONE &&
        candidate_overlay_transform != gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL) {
      candidate.overlay_handled = false;
      return;
    }
    candidate.transform = candidate_overlay_transform;

    gfx::RectF orig_display_rect = candidate.display_rect;
    gfx::RectF display_rect = orig_display_rect;
    if (candidate.clip_rect) {
      display_rect.Intersect(gfx::RectF(*candidate.clip_rect));
    }
    // The framework doesn't support display rects positioned at a negative
    // offset.
    display_rect = ClipFromOrigin(display_rect);
    if (display_rect.IsEmpty()) {
      candidate.overlay_handled = false;
      return;
    }

    // The display rect above includes the |display_transform_| while the rects
    // sent to the platform API need to be in the logical screen space.
    const gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
        gfx::InvertOverlayTransform(display_transform_),
        gfx::SizeF(viewport_size_));
    orig_display_rect = display_inverse.MapRect(orig_display_rect);
    display_rect = display_inverse.MapRect(display_rect);

    candidate.unclipped_display_rect = orig_display_rect;
    candidate.unclipped_uv_rect = candidate.uv_rect;

    candidate.display_rect = gfx::RectF(gfx::ToEnclosingRect(display_rect));

    // Transform `uv_rect` to display space, then clip, then transform back.
    candidate.uv_rect = gfx::OverlayTransformToTransform(
                            candidate_overlay_transform, gfx::SizeF(1, 1))
                            .MapRect(candidate.uv_rect);
    candidate.uv_rect = cc::MathUtil::ScaleRectProportional(
        candidate.uv_rect, orig_display_rect, candidate.display_rect);
    candidate.uv_rect =
        gfx::OverlayTransformToTransform(
            gfx::InvertOverlayTransform(candidate_overlay_transform),
            gfx::SizeF(1, 1))
            .MapRect(candidate.uv_rect);
    candidate.overlay_handled = true;
  }
}

void OverlayProcessorOHOS::AdjustOutputSurfaceOverlay(
    std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane->has_value()) {
    // No output surface plane to adjust.
    return;
  }
  // For surface control, we should always have a valid |output_surface_plane|
  // here.
  CHECK(output_surface_plane);
  CHECK(output_surface_plane->has_value());
  // DCHECK(output_surface_plane && output_surface_plane->has_value());

  OutputSurfaceOverlayPlane& plane = output_surface_plane->value();
  // DCHECK(gfx::OHOS::SupportsColorSpace(plane.color_space))
  //     << "The main overlay must only use color space supported by the "
  //        "device";

  DCHECK_EQ(plane.transform, gfx::OVERLAY_TRANSFORM_NONE);
  DCHECK(plane.display_rect == ClipFromOrigin(plane.display_rect));

  plane.transform = display_transform_;
  const gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
      gfx::InvertOverlayTransform(display_transform_),
      gfx::SizeF(viewport_size_));
  plane.display_rect = display_inverse.MapRect(plane.display_rect);
  plane.display_rect = gfx::RectF(gfx::ToEnclosingRect(plane.display_rect));

  // Call the base class implementation.
  OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
      output_surface_plane);
}

gfx::Rect OverlayProcessorOHOS::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& candidate) const {
  // Should only be called after ProcessForOverlays on handled candidates.
  DCHECK(candidate.overlay_handled);
  // We transform the candidate's display rect to the logical screen space (used
  // by the ui when preparing the frame) that the OHOS expects it to
  // be in. So in order to provide a damage rect which maps to the
  // OutputSurface's main plane, we need to undo that transformation. But only
  // if the overlay is in handled state, since the modification above is only
  // applied when we mark the overlay as handled.
  gfx::Size viewport_size_pre_display_transform(viewport_size_.height(),
                                                viewport_size_.width());
  auto transform = gfx::OverlayTransformToTransform(
      display_transform_, gfx::SizeF(viewport_size_pre_display_transform));
  return transform.MapRect(gfx::ToEnclosingRect(candidate.display_rect));
}

bool OverlayProcessorOHOS::SupportsFlipRotateTransform() const {
  return true;
}

void OverlayProcessorOHOS::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

void OverlayProcessorOHOS::SetViewportSize(const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

std::optional<gfx::ColorSpace> OverlayProcessorOHOS::GetOverrideColorSpace() {
  // // Historically, android media was hardcoding color space to srgb and it
  // // wasn't possible to overlay with arbitrary colorspace on pre-S devices,
  // so
  // // we keep old behaviour there.
  // static bool is_older_than_s =
  //     base::android::BuildInfo::GetInstance()->sdk_int() <
  //     base::android::SdkVersion::SDK_VERSION_S;
  // if (is_older_than_s) {
  //   return gfx::ColorSpace::CreateSRGB();
  // }

  return std::nullopt;
}

bool OverlayProcessorOHOS::AttemptWithStrategies(
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    const DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  auto* render_pass = render_pass_list->back().get();
  QuadList* quad_list = &render_pass->quad_list;

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.supports_clip_rect = false;
  context.supports_out_of_window_clip_rect = true;
  context.supports_arbitrary_transform = false;
  context.supports_mask_filter = false;
  context.transform_and_clip_rpdq = false;
  context.supports_flip_rotate_transform = false;

  OverlayCandidateFactory candidate_factory = OverlayCandidateFactory(
      render_pass, resource_provider, surface_damage_rect_list,
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane),
      &render_pass_filters, context);

  unassigned_damage_ = gfx::RectF(candidate_factory.GetUnassignedDamage());

  candidates->reserve(quad_list->size());

  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    if (auto result = TryPromoteDrawQuadForDelegation(candidate_factory, *it);
        result.has_value()) {
      if (const auto& candidate = result.value(); candidate.has_value()) {
        candidates->push_back(candidate.value());
      }
    } else {
      candidates->clear();
      delegated_status_ = result.error();
      return false;
    }
  }

  int curr_plane_order = candidates->size();
  for (auto&& each : *candidates) {
    each.plane_z_order = curr_plane_order--;
  }

  // Check for support.
  this->CheckOverlaySupport(nullptr, candidates);

  for (auto&& each : *candidates) {
    if (!each.overlay_handled) {
      candidates->clear();
      delegated_status_ = DelegationStatus::kCompositedCheckOverlayFail;
      // DBG_DRAW_RECT("delegated.handled.failed", each.display_rect);
      // DBG_LOG("delegated.handled.failed", "Handled failed %s",
      //         each.display_rect.ToString().c_str());
      return false;
    }
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
