// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_ohos.h"

#include <native_buffer/native_buffer.h>

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
#include "components/viz/service/display_embedder/surface_control_api.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"

namespace viz {
namespace {

constexpr uint32_t kDebugBorderWidth = 4;
using Color4f = std::array<float, 4>;
constexpr Color4f kColorRed = {1.0f, 0.0f, 0.0f, 1.0f};
constexpr Color4f kColorRedTint = {1.0f, 0.0f, 0.0f, 0.3f};
constexpr Color4f kColorBlueTint = {0.0f, 0.0f, 1.0f, 0.3f};
constexpr Color4f kColorGreenTint = {0.0f, 1.0f, 0.0f, 0.3f};
constexpr Color4f kColorTransparent = {0.0f, 0.0f, 0.0f, 0.0f};
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

gfx::Size GetRotatedSize(const gfx::Size size,
                         gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
      return gfx::Size(size.height(), size.width());
    default:
      return size;
  }
}

int32_t FromGfxOverlayTransform(gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return OH_TRANSFORM_ROTATE_NONE;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return OH_TRANSFORM_ROTATE_90;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return OH_TRANSFORM_ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return OH_TRANSFORM_ROTATE_270;
    default:
      NOTREACHED();
  }
}

}  // namespace

class OutputPresenterOHOS::SurfaceControl {
 public:
  using ReleaseCallback =
      base::OnceCallback<void(base::ScopedFD release_fence_fd)>;

  explicit SurfaceControl(SurfaceControl* parent)
      : name_("delegate_child"), parent_(parent ? parent->surface_ : nullptr) {}
  explicit SurfaceControl(OHNativeWindow* window)
      : name_("delegate_container") {
    const auto& api = SurfaceControlAPI::GetInstance();
    surface_ = api.SurfaceControl_FromNativeWindow(window, name_.c_str());
  }

  ~SurfaceControl() {
    DCHECK(destroyed_);
    DCHECK(dirty_bits_.none());
    if (surface_) {
      const auto& api = SurfaceControlAPI::GetInstance();
      api.SurfaceControl_Release(surface_);
    }
  }

  void Destroy(OH_SurfaceTransaction* transaction) {
    DCHECK(!destroyed_);
    destroyed_ = true;
    const auto& api = SurfaceControlAPI::GetInstance();
    api.SurfaceTransaction_Reparent(transaction, surface_, nullptr);
    api.SurfaceTransaction_SetVisibility(
        transaction, surface_, OH_SURFACE_TRANSACTION_VISIBILITY_HIDE);
  }

  void SetSurface(SurfaceControl* surface) {
    DCHECK(!surface_);
    surface_ = surface->ReleaseSurface();

    DCHECK(surface->dirty_bits_.none());
    if (name_ != surface->name_) {
      dirty_bits_.set(kName);
    }
    if (z_order_ != surface->z_order_) {
      dirty_bits_.set(kZOrder);
    }
    if (bounds_ != surface->bounds_) {
      dirty_bits_.set(kBounds);
    }
    if (frame_ != surface->frame_) {
      dirty_bits_.set(kFrame);
    }
    if (frame_gravity_ != surface->frame_gravity_) {
      dirty_bits_.set(kFrameGravity);
    }
    if (scale_ != surface->scale_) {
      dirty_bits_.set(kScale);
    }
    if (visible_ != surface->visible_) {
      dirty_bits_.set(kVisible);
    }
    if (translate_ != surface->translate_) {
      dirty_bits_.set(kTranslate);
    }
    if (pivot_ != surface->pivot_) {
      dirty_bits_.set(kPivot);
    }
    if (border_width_ != surface->border_width_) {
      dirty_bits_.set(kBorderWidth);
    }
    if (border_color_ != surface->border_color_) {
      dirty_bits_.set(kBorderColor);
    }
    if (border_style_ != surface->border_style_) {
      dirty_bits_.set(kBorderStyle);
    }
    if (foreground_color_ != surface->foreground_color_) {
      dirty_bits_.set(kForegroundColor);
    }
    if (background_color_ != surface->background_color_) {
      dirty_bits_.set(kBackgroundColor);
    }
    if (alpha_ != surface->alpha_) {
      dirty_bits_.set(kAlpha);
    }
    if (buffer_transform_ != surface->buffer_transform_) {
      dirty_bits_.set(kBufferTransform);
    }
  }

  OH_SurfaceControl* ReleaseSurface() {
    DCHECK(parent_);
    auto* surface = surface_;
    surface_ = nullptr;
    return surface;
  }

  void SetZOrder(int32_t z_order) {
    if (z_order_ != z_order) {
      z_order_ = z_order;
      dirty_bits_.set(kZOrder);
    }
  }

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

  void SetFrameGravity(int32_t frame_gravity) {
    if (frame_gravity_ != frame_gravity) {
      frame_gravity_ = frame_gravity;
      dirty_bits_.set(kFrameGravity);
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

  void SetBackgroundColor(const std::array<float, 4>& color) {
    if (background_color_ != color) {
      background_color_ = color;
      dirty_bits_.set(kBackgroundColor);
    }
  }

  void SetAlpha(float alpha) {
    if (alpha_ != alpha) {
      alpha_ = alpha;
      dirty_bits_.set(kAlpha);
    }
  }

  void SetName(const std::string& name) {
    if (name_ != name) {
      name_ = name;
      dirty_bits_.set(kName);
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

  void SetBufferTransform(int32_t transform) {
    if (buffer_transform_ != transform) {
      buffer_transform_ = transform;
      dirty_bits_.set(kBufferTransform);
    }
  }

  void Sync(OH_SurfaceTransaction* transaction) {
    DCHECK(!destroyed_);
    const auto& api = SurfaceControlAPI::GetInstance();

    if (!surface_) {
      surface_ = api.SurfaceControl_Create(name_.c_str());
      api.SurfaceTransaction_SetHardwareEnableHint(transaction, surface_, true);
      api.SurfaceTransaction_Reparent(transaction, surface_, parent_);
      api.SurfaceTransaction_SetVisibility(
          transaction, surface_,
          visible_ ? OH_SURFACE_TRANSACTION_VISIBILITY_SHOW
                   : OH_SURFACE_TRANSACTION_VISIBILITY_HIDE);
      api.SurfaceTransaction_SetZOrder(transaction, surface_, z_order_);
      dirty_bits_.reset(kVisible);
      dirty_bits_.reset(kName);
      dirty_bits_.reset(kZOrder);
    }

    if (dirty_bits_.test(kName)) {
      api.SurfaceTransaction_SetName(transaction, surface_, name_.c_str());
    }
    if (dirty_bits_.test(kVisible)) {
      api.SurfaceTransaction_SetVisibility(
          transaction, surface_,
          visible_ ? OH_SURFACE_TRANSACTION_VISIBILITY_SHOW
                   : OH_SURFACE_TRANSACTION_VISIBILITY_HIDE);
    }
    if (dirty_bits_.test(kZOrder)) {
      api.SurfaceTransaction_SetZOrder(transaction, surface_, z_order_);
    }
    if (dirty_bits_.test(kBounds)) {
      api.SurfaceTransaction_SetBounds(transaction, surface_, bounds_.x(),
                                       bounds_.y(), bounds_.width(),
                                       bounds_.height());
    }

    if (dirty_bits_.test(kFrame)) {
      api.SurfaceTransaction_SetFrame(transaction, surface_, frame_.x(),
                                      frame_.y(), frame_.width(),
                                      frame_.height());
    }
    if (dirty_bits_.test(kFrameGravity)) {
      api.SurfaceTransaction_SetFrameGravity(transaction, surface_,
                                             frame_gravity_);
    }
    if (dirty_bits_.test(kScale)) {
      api.SurfaceTransaction_SetScale(transaction, surface_, scale_[0],
                                      scale_[1], 1.0);
    }
    if (dirty_bits_.test(kTranslate)) {
      api.SurfaceTransaction_SetTranslate(transaction, surface_, translate_[0],
                                          translate_[1], translate_[2]);
    }
    if (dirty_bits_.test(kPivot)) {
      api.SurfaceTransaction_SetPivot(transaction, surface_, pivot_[0],
                                      pivot_[1]);
    }
    if (dirty_bits_.test(kBorderWidth)) {
      api.SurfaceTransaction_SetBorderWidth(transaction, surface_,
                                            border_width_, border_width_,
                                            border_width_, border_width_);
    }
    if (dirty_bits_.test(kBorderColor)) {
      api.SurfaceTransaction_SetBorderColor(transaction, surface_,
                                            border_color_[0], border_color_[1],
                                            border_color_[2], border_color_[3]);
    }
    if (dirty_bits_.test(kBorderStyle)) {
      api.SurfaceTransaction_SetBorderStyle(transaction, surface_,
                                            border_style_, border_style_,
                                            border_style_, border_style_);
    }
    if (dirty_bits_.test(kForegroundColor)) {
      api.SurfaceTransaction_SetForegroundColor(
          transaction, surface_, foreground_color_[0], foreground_color_[1],
          foreground_color_[2], foreground_color_[3]);
    }
    if (dirty_bits_.test(kBackgroundColor)) {
      api.SurfaceTransaction_SetBackgroundColor(
          transaction, surface_, background_color_[0], background_color_[1],
          background_color_[2], background_color_[3]);
    }
    if (dirty_bits_.test(kAlpha)) {
      api.SurfaceTransaction_SetBufferAlpha(transaction, surface_, alpha_);
    }
    if (dirty_bits_.test(kBufferTransform)) {
      api.SurfaceTransaction_SetBufferTransform(transaction, surface_,
                                                buffer_transform_);
    }

    if (dirty_bits_.test(kBuffer)) {
      std::unique_ptr<ReleaseCallback> callback_ptr;
      if (release_callback_) {
        callback_ptr =
            std::make_unique<ReleaseCallback>(std::move(release_callback_));
      }
      api.SurfaceTransaction_SetBuffer(
          transaction, surface_, buffer_, fence_fd_.release(),
          callback_ptr.release(), [](void* data, int32_t release_fence_fd) {
            std::unique_ptr<ReleaseCallback> callback_ptr(
                reinterpret_cast<ReleaseCallback*>(data));
            if (callback_ptr) {
              std::move(*callback_ptr).Run(base::ScopedFD(release_fence_fd));
            }
          });
      OH_Rect rect = {damage_rect_.x(), damage_rect_.y(), damage_rect_.width(),
                      damage_rect_.height()};
      api.SurfaceTransaction_SetDamageRegion(transaction, surface_, &rect, 1);
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
    kName,
    kVisible,
    kBounds,
    kFrame,
    kFrameGravity,
    kScale,
    kTranslate,
    kPivot,
    kBuffer,
    kBufferTransform,
    kBorderWidth,
    kBorderColor,
    kBorderStyle,
    kForegroundColor,
    kBackgroundColor,
    kAlpha,
    kZOrder,
    kCount,
  };
  std::bitset<kCount> dirty_bits_;

  std::string name_;
  OH_SurfaceControl* parent_ = nullptr;
  OH_SurfaceControl* surface_ = nullptr;
  bool destroyed_ = false;
  int32_t z_order_ = -1;
  bool visible_ = true;
  gfx::RectF bounds_;
  gfx::RectF frame_;
  int32_t frame_gravity_ = OH_FRAME_GRAVITY_RESIZE;

  std::array<float, 2> scale_ = {1.0, 1.0};
  std::array<float, 3> translate_ = {0.0, 0.0, 0.0};
  std::array<float, 2> pivot_ = {0.5, 0.5};
  Color4f border_color_ = {0.0, 0.0, 0.0, 0.0};
  int32_t border_width_ = 0;
  int32_t border_style_ = OH_SURFACE_TRANSACTION_BORDER_STYLE_SOLID;
  Color4f foreground_color_ = kColorTransparent;
  Color4f background_color_ = kColorTransparent;
  float alpha_ = 1.0f;

  OH_NativeBuffer* buffer_ = nullptr;
  int32_t buffer_transform_ = OH_TRANSFORM_ROTATE_NONE;
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
      transaction_(
          SurfaceControlAPI::GetInstance().SurfaceTransaction_Create()) {
  root_surface_ = std::make_unique<SurfaceControl>(
      reinterpret_cast<OHNativeWindow*>(deps->GetSurfaceHandle()));
  auto callback = base::BindRepeating(&OutputPresenterOHOS::OnComplete,
                                      weak_factory_.GetWeakPtr());

  callback = base::BindPostTask(task_runner_, std::move(callback));
  on_complete_callback_ = std::make_unique<OnCompleteCallback>(callback);
  SurfaceControlAPI::GetInstance().SurfaceTransaction_SetOnComplete(
      transaction_, on_complete_callback_.get(),
      [](void* context, uint64_t timestamp) {
        auto* callback = reinterpret_cast<OnCompleteCallback*>(context);
        callback->Run(timestamp);
      });
}

OutputPresenterOHOS::~OutputPresenterOHOS() {
  const auto& api = SurfaceControlAPI::GetInstance();
  api.SurfaceTransaction_SetOnComplete(transaction_, nullptr, nullptr);
  api.SurfaceTransaction_Delete(transaction_);
}

void OutputPresenterOHOS::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities->supports_post_sub_buffer = true;
  capabilities->supports_surfaceless = true;

  capabilities->number_of_buffers = 5;
  capabilities->supports_dynamic_frame_buffer_allocation = true;
  capabilities->renderer_allocates_images = true;

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

  int32_t first_surface_z_order =
      pending_frame_.empty() ? 0 : pending_frame_.front()->z_order();

  // Remove reused item (nullptr) from previous_frame_.
  auto it =
      std::remove(previous_frame_.begin(), previous_frame_.end(), nullptr);
  previous_frame_.erase(it, previous_frame_.end());

  for (auto& surface : pending_frame_) {
    // Try reuse OH_SurfaceControl from previous frame.
    if (!surface->has_surface() && !previous_frame_.empty() &&
        surface->buffer()) {
      surface->SetSurface(previous_frame_.front().get());
      previous_frame_.pop_front();
    }
    if (first_surface_z_order < 0) {
      // Make sure z order is non-negative.
      surface->SetZOrder(surface->z_order() - first_surface_z_order);
    }
    // surface->SetTranslate(0.0f, 0.0f, depth);
    surface->Sync(transaction_);
  }

  // Clear all unused surface from previous frame.
  for (auto& surface : previous_frame_) {
    surface->Destroy(transaction_);
    surface.reset();
  }
  previous_frame_.clear();

  const auto& api = SurfaceControlAPI::GetInstance();
  api.SurfaceTransaction_Commit(transaction_);
  std::swap(previous_frame_, pending_frame_);

  completion_callbacks_.emplace_back(std::move(completion_callback));
  root_overlay_access_ = nullptr;
  presentation_callbacks_.emplace_back(std::move(presentation_callback));
}

void OutputPresenterOHOS::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane,
    ScopedOverlayAccess* access) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const bool is_root = overlay_plane.is_root_render_pass;

  DCHECK_EQ(overlay_plane.is_solid_color, access == nullptr)
      << "access and is_solid_color don't match";

  // display_rect which is in the display (ArkWeb area) coordinate.
  const gfx::RectF& display_rect = overlay_plane.display_rect;

  gfx::RectF bounds;
  gfx::RectF frame;
  float x_scale = 1.0;
  float y_scale = 1.0;

  if (is_root) {
    CHECK(!root_overlay_access_);
    root_overlay_access_ = access;
    bounds = display_rect;
    frame = gfx::RectF(GetRotatedSize(size_, transform_));
  } else {
    if (overlay_plane.is_solid_color) {
      // For solid color quad.
      bounds = display_rect;
      frame = gfx::RectF(display_rect.size());
    } else {
      // TODO: figure out how to use overlay_plane.clip_rect
      // For texture quad.

      // Buffer size in pixels
      const gfx::SizeF buffer_size(overlay_plane.resource_size_in_pixels);

      // Caculate source rect based on the uv_rect which coordinates are between
      // [0.0, 1.0].
      gfx::RectF src_rect = gfx::ScaleRect(overlay_plane.uv_rect, buffer_size);

      // When the video is being scrolled offscreen DisplayCompositor will crop
      // it to only visible portion and adjust uv_rect accordingly. When the
      // video is smaller than the surface is can lead to the crop rect being
      // less than a pixel in size. This adjusts the crop rect size to at least
      // 1 pixel as we want to stretch last visible pixel line/column in this
      // case. Note: We will do it even if crop_rect width/height is exact 0.0f.
      // In reality this should never happen and there is no way to display
      // video with empty crop rect, so display compositor should not request
      // this.
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
      // Make sure src_rect is insize of the buffer.
      src_rect.Intersect(gfx::RectF(buffer_size));
      DCHECK_GT(src_rect.width(), 0);
      DCHECK_GT(src_rect.height(), 0);

      // Caculate scales based on src_rect (on the buffer in pixels) and
      // display_rect (the final rect on screen)
      x_scale = display_rect.width() / src_rect.width();
      y_scale = display_rect.height() / src_rect.height();

      bounds = gfx::RectF(display_rect.origin(), src_rect.size());
      frame = gfx::RectF(
          display_rect.origin() - src_rect.origin().OffsetFromOrigin(),
          buffer_size);
    }
  }

  OH_NativeBuffer* const buffer =
      access ? access->GetOHNativeBuffer() : nullptr;
  SurfaceControl* surface = nullptr;

  if (buffer) {
    // For plane with buffer.
    DCHECK(!overlay_plane.is_solid_color);

    // Find surface with same buffer in previous frame first.
    auto it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                           [&](const std::unique_ptr<SurfaceControl>& surface) {
                             return surface && surface->buffer() == buffer;
                           });
    if (it == previous_frame_.end()) {
      // Find surface with same display rect in previous frame.
      it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                        [&](const std::unique_ptr<SurfaceControl>& surface) {
                          return surface && surface->bounds() == bounds;
                        });
    }

    if (it != previous_frame_.end()) {
      // Found a surface with same buffer or bounds
      pending_frame_.push_back(std::move(*it));
    } else {
      DCHECK(root_surface_);
      // Cannot find a surface with same buffer or bounds, so create a new one.
      pending_frame_.emplace_back(
          std::make_unique<SurfaceControl>(root_surface_.get()));
    }

    surface = pending_frame_.back().get();

    if (surface->buffer() != buffer) {
      base::ScopedFD fence_fd = access->TakeAcquireFence().Release();
      auto callback = base::BindOnce(&OutputPresenterOHOS::OnOverlayReleased,
                                     weak_factory_.GetWeakPtr(),
                                     base::UnsafeDangling(access), is_root);

      // Make sure OnOverlayReleased() is called on GPU main thread.
      callback = base::BindPostTask(task_runner_, std::move(callback));

      // We need track buffer which are being used by RS. For root surface,
      // the buffer is considered being released when OnComplete is called.
      if (!is_root) {
        access->InUseByWindowServerInc();
      }

      auto damage_rect = overlay_plane.damage_rect.IsEmpty()
                             ? gfx::Rect(overlay_plane.resource_size_in_pixels)
                             : ToEnclosingRect(overlay_plane.damage_rect);
      surface->SetBuffer(buffer, std::move(fence_fd), damage_rect,
                         std::move(callback));
    }
  } else {
    // For a solid plane.
    DCHECK(overlay_plane.is_solid_color);

    // Find a surface with the same bounds.
    auto it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                           [&](const std::unique_ptr<SurfaceControl>& surface) {
                             if (!surface) {
                               return false;
                             }
                             if (surface->buffer()) {
                               return false;
                             }
                             return surface->bounds() == bounds;
                           });

    // If cannot find a surface with the same bounds, try to find a surface for
    // any solid color plane.
    if (it == previous_frame_.end()) {
      it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                        [&](const std::unique_ptr<SurfaceControl>& surface) {
                          return surface && surface->buffer() == nullptr;
                        });
    }

    if (it != previous_frame_.end()) {
      pending_frame_.push_back(std::move(*it));
    } else {
      DCHECK(root_surface_);
      pending_frame_.emplace_back(
          std::make_unique<SurfaceControl>(root_surface_.get()));
    }

    surface = pending_frame_.back().get();
  }

  // Make sure surface is visible.
  surface->SetVisible(true);
  surface->SetBounds(bounds);
  surface->SetFrame(frame);
  surface->SetScale(x_scale, y_scale);
  surface->SetZOrder(overlay_plane.plane_z_order);
  surface->SetAlpha(overlay_plane.opacity);

  // Only root buffer is always in hardware physical direction(portrait), so it
  // need to be rotated to the current screen logic rotation direction.
  auto transform =
      is_root ? FromGfxOverlayTransform(transform_) : OH_TRANSFORM_ROTATE_NONE;

  surface->SetBufferTransform(transform);

  // Set surface's background color.
  if (overlay_plane.color.has_value()) {
    surface->SetBackgroundColor(
        {overlay_plane.color->fR, overlay_plane.color->fG,
         overlay_plane.color->fB, overlay_plane.color->fA});
  }

  if (IsDebugEnabled()) {
    surface->SetBorderStyle(OH_SURFACE_TRANSACTION_BORDER_STYLE_SOLID);
    surface->SetBorderWidth(kDebugBorderWidth);
    surface->SetBorderColor(kColorRed);
    if (!overlay_plane.is_solid_color) {
      if (overlay_plane.is_opaque) {
        surface->SetForegroundColor(kColorRedTint);
      } else {
        surface->SetForegroundColor(kColorGreenTint);
      }
    }
  }

// TODO it is for debugging, remove it before landing the change.
#if 0
  std::stringstream name;
  name << (is_root ? "delegate_root" : "delegate_child");
  if (buffer) {
    name << "_buffer";
  }
  if (overlay_plane.is_solid_color) {
    name << "_color";
  }
  if (overlay_plane.color.has_value()) {
    name << "_RGBA(" << overlay_plane.color->fR << ","
         << overlay_plane.color->fG << "," << overlay_plane.color->fB << ","
         << overlay_plane.color->fA << ")";
  }
  surface->SetName(name.str());
#endif
}

void OutputPresenterOHOS::OnComplete(uint64_t timestamp) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!completion_callbacks_.empty());
  DCHECK(!presentation_callbacks_.empty());
  gfx::SwapCompletionResult swap_result(gfx::SwapResult::SWAP_ACK);
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::TimeDelta(),
                                     /*flags=*/0);
  std::move(completion_callbacks_.front()).Run(std::move(swap_result));
  completion_callbacks_.pop_front();
  std::move(presentation_callbacks_.front()).Run(std::move(feedback));
  presentation_callbacks_.pop_front();
}

void OutputPresenterOHOS::OnOverlayReleased(
    MayBeDangling<ScopedOverlayAccess> access,
    bool is_root,
    base::ScopedFD release_fence_fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Below code will crash for root overlay.
  // TODO(penghuang): figure out how to handle it properly
  // Probably it causes the artifacts on screen.
  if (release_fence_fd.is_valid()) {
    gfx::GpuFenceHandle fence_handle;
    fence_handle.Adopt(std::move(release_fence_fd));
    access->SetReleaseFence(std::move(fence_handle));
  }

  // Decrease in use count, when count is 0, viz will return the buffer to
  // render, so the buffer can be reused or released.
  access->InUseByWindowServerDec();
}

}  // namespace viz
