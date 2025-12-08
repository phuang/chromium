// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_android.h"

#include <array>
#include <bitset>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_plane_data.h"

namespace viz {
namespace {

using Color4f = std::array<float, 4>;
constexpr Color4f kColorTransparent = {0.0f, 0.0f, 0.0f, 0.0f};

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

}  // namespace

// Define all properties of a Surface.
#define SURFACE_PROPERTIES(X)                                 \
  X(bool, Visibility, visibility, true)                       \
  X(Color4f, Color, color, kColorTransparent)                 \
  X(float, BufferAlpha, buffer_alpha, 1.0f)                   \
  X(bool, Opaque, opaque, false)                              \
  X(gfx::Point, Position, position, {})                       \
  X(gfx::Rect, Crop, crop, {})                                \
  X(gfx::Vector2dF, Scale, scale, gfx::Vector2dF(1.0f, 1.0f)) \
  X(gfx::OverlayTransform, BufferTransform, buffer_transform, \
    gfx::OVERLAY_TRANSFORM_NONE)                              \
  X(int32_t, ZOrder, z_order, 0)

class OutputPresenterAndroid::Surface {
 public:
  using ReleaseCallback =
      base::OnceCallback<void(base::ScopedFD release_fence_fd)>;

  Surface(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
          Surface* parent)
      : task_runner_(std::move(task_runner)),
        parent_(parent ? parent->surface_ : nullptr) {}

  Surface(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
          ANativeWindow* window)
      : task_runner_(std::move(task_runner)) {
    surface_ = base::MakeRefCounted<gfx::SurfaceControl::Surface>(
        window, "delegate_container");
  }

  Surface(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
          gl::ScopedJavaSurfaceControl surface_control)
      : task_runner_(std::move(task_runner)) {
    surface_ = surface_control.MakeSurface();
  }

  ~Surface() {
    DCHECK(destroyed_);
    DCHECK(dirty_bits_.none());
  }

  void Destroy(gfx::SurfaceControl::Transaction* transaction) {
    DCHECK(!destroyed_);
    destroyed_ = true;
    transaction->SetParent(*surface_, nullptr);
    transaction->SetVisibility(*surface_, false);
  }

  void SetSurface(std::unique_ptr<Surface> surface) {
    DCHECK(!surface_);
    task_runner_ = std::move(surface->task_runner_);
    surface_ = surface->ReleaseSurface();
    DCHECK(surface->dirty_bits_.none());
#define IMPL_CHECK_DIRTY_PROPERTY(type, name, member, default_value) \
  if (member##_ != surface->member##_) {                             \
    dirty_bits_.set(k##name);                                        \
  }
    SURFACE_PROPERTIES(IMPL_CHECK_DIRTY_PROPERTY)
#undef IMPL_CHECK_DIRTY_PROPERTY
  }

  scoped_refptr<gfx::SurfaceControl::Surface> ReleaseSurface() {
    DCHECK(parent_);
    auto surface = surface_;
    surface_ = nullptr;
    destroyed_ = true;
    return surface;
  }

#define DECLARE_SETTER_AND_GETTER(type, name, member, default_value) \
  void Set##name(type value) {                                       \
    if (member##_ != value) {                                        \
      member##_ = value;                                             \
      dirty_bits_.set(k##name);                                      \
    }                                                                \
  }                                                                  \
  type member() const { return member##_; }
  SURFACE_PROPERTIES(DECLARE_SETTER_AND_GETTER)
#undef DECLARE_SETTER_AND_GETTER

  void SetBuffer(base::android::ScopedHardwareBufferHandle buffer,
                 base::ScopedFD fence_fd,
                 const gfx::Rect& damage_rect,
                 ReleaseCallback release_callback) {
    DCHECK(!release_callback_);
    DCHECK(release_callback);
    DCHECK_NE(buffer_.get(), buffer.get());
    buffer_ = std::move(buffer);
    fence_fd_ = std::move(fence_fd);
    damage_rect_ = damage_rect;
    release_callback_ = std::move(release_callback);
    dirty_bits_.set(kBuffer);
  }

  void Sync(gfx::SurfaceControl::Transaction* transaction) {
    DCHECK(!destroyed_);
    CreateSurfaceIfNeeded();

#define IMPL_SYNC_PROPERTY(type, name, member, default_value) \
  if (dirty_bits_.test(k##name)) {                            \
    transaction->Set##name(*surface_, member##_);             \
  }
    SURFACE_PROPERTIES(IMPL_SYNC_PROPERTY)
#undef IMPL_SYNC_PROPERTY

    if (dirty_bits_.test(kBuffer) && buffer_.is_valid()) {
      DCHECK(release_callback_);
      transaction->SetBufferWithRelease(
          *surface_, buffer_.get(), std::move(fence_fd_),
          std::move(release_callback_), task_runner_);
    }
    dirty_bits_.reset();
  }

  bool has_surface() const { return surface_ != nullptr; }
  bool has_buffer() const { return buffer_.is_valid(); }
  const base::android::ScopedHardwareBufferHandle& buffer() const {
    return buffer_;
  }

 private:
  void CreateSurfaceIfNeeded() {
    if (!surface_) {
      surface_ = base::MakeRefCounted<gfx::SurfaceControl::Surface>(
          *parent_, "delegate_child");
      dirty_bits_.set();
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<gfx::SurfaceControl::Surface> surface_;
  scoped_refptr<gfx::SurfaceControl::Surface> parent_;
  bool destroyed_ = false;

  enum DirtyBits {
#define DECLARE_DIRTY_BIT(type, name, member, default_value) k##name,
    SURFACE_PROPERTIES(DECLARE_DIRTY_BIT)
#undef DECLARE_DIRTY_BIT
        kBuffer,
    kCount,
  };
  std::bitset<kCount> dirty_bits_ = {~(1ull << kBuffer)};

#define DECLARE_MEMBER_VARIABLE(type, name, member, default_value) \
  type member##_ = default_value;
  SURFACE_PROPERTIES(DECLARE_MEMBER_VARIABLE)
#undef DECLARE_MEMBER_VARIABLE

  base::android::ScopedHardwareBufferHandle buffer_;
  base::ScopedFD fence_fd_;
  gfx::Rect damage_rect_;
  ReleaseCallback release_callback_;
};

class OutputPresenterAndroid::ScopedBufferReleaseData {
 public:
  ScopedBufferReleaseData(
      base::raw_ptr<ScopedOverlayAccess> access,
      std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
          buffer_fence_sync,
      bool is_root)
      : access_(access),
        buffer_fence_sync_(std::move(buffer_fence_sync)),
        is_root_(is_root) {
    if (access_) {
      access_->InUseByWindowServerInc();
    }
  }

  ~ScopedBufferReleaseData() { reset(); }

  ScopedBufferReleaseData(ScopedBufferReleaseData&& other) {
    reset();
    access_ = other.access_;
    other.access_ = nullptr;
    buffer_fence_sync_ = std::move(other.buffer_fence_sync_);
    is_root_ = other.is_root_;
  }

  ScopedBufferReleaseData(const ScopedBufferReleaseData&) = delete;
  ScopedBufferReleaseData& operator=(const ScopedBufferReleaseData&) = delete;

  void reset() {
    if (access_) {
      access_->InUseByWindowServerDec();
      access_ = nullptr;
    }
    buffer_fence_sync_.reset();
  }

  ScopedOverlayAccess* access() { return access_.get(); }
  base::android::ScopedHardwareBufferFenceSync* buffer_fence_sync() {
    return buffer_fence_sync_.get();
  }
  bool is_root() const { return is_root_; }

 private:
  base::raw_ptr<ScopedOverlayAccess> access_;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      buffer_fence_sync_;
  bool is_root_;
};

OutputPresenterAndroid::FrameData::FrameData(
    base::raw_ptr<ScopedOverlayAccess> access,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback)
    : access(access),
      completion_callback(std::move(completion_callback)),
      presentation_callback(std::move(presentation_callback)) {}

OutputPresenterAndroid::FrameData::~FrameData() = default;
OutputPresenterAndroid::FrameData::FrameData(FrameData&& other) = default;
OutputPresenterAndroid::FrameData& OutputPresenterAndroid::FrameData::operator=(
    FrameData&& other) = default;

// static
std::unique_ptr<OutputPresenterAndroid> OutputPresenterAndroid::Create(
    SkiaOutputSurfaceDependency* deps) {
  if (!features::IsFullDelegatedCompositingEnabled()) {
    return {};
  }

  if (!gfx::SurfaceControl::IsSupported()) {
    return {};
  }

  auto surface_handle = deps->GetSurfaceHandle();
  auto surface_record =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(surface_handle);
  if (!surface_record.can_be_used_with_surface_control) {
    return {};
  }

  if (std::holds_alternative<gl::ScopedJavaSurface>(
          surface_record.surface_variant)) {
    auto scoped_java_surface = std::get<gl::ScopedJavaSurface>(
        std::move(surface_record.surface_variant));
    return std::make_unique<OutputPresenterAndroid>(
        deps, gl::ScopedANativeWindow(scoped_java_surface));
  }

  if (std::holds_alternative<gl::ScopedJavaSurfaceControl>(
          surface_record.surface_variant)) {
    auto scoped_java_surface_control = std::get<gl::ScopedJavaSurfaceControl>(
        std::move(surface_record.surface_variant));
    return std::make_unique<OutputPresenterAndroid>(
        deps, std::move(scoped_java_surface_control));
  }

  return {};
}

OutputPresenterAndroid::OutputPresenterAndroid(
    SkiaOutputSurfaceDependency* deps,
    gl::ScopedANativeWindow window)
    : dependency_(deps),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  root_surface_ =
      std::make_unique<Surface>(task_runner_, window.a_native_window());
}

OutputPresenterAndroid::OutputPresenterAndroid(
    SkiaOutputSurfaceDependency* deps,
    gl::ScopedJavaSurfaceControl surface_control)
    : dependency_(deps),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  root_surface_ =
      std::make_unique<Surface>(task_runner_, std::move(surface_control));
}

OutputPresenterAndroid::~OutputPresenterAndroid() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void OutputPresenterAndroid::InitializeCapabilities(
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

bool OutputPresenterAndroid::Reshape(const ReshapeParams& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  size_ = params.GfxSize();
  root_surface_->SetCrop(gfx::Rect(size_));
  root_surface_->SetPosition(gfx::Point(0, 0));
  root_surface_->SetScale(gfx::Vector2dF(1.0f, 1.0f));
  return true;
}

void OutputPresenterAndroid::Present(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback,
    gfx::FrameData data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  gfx::SurfaceControl::Transaction transaction;
  root_surface_->Sync(&transaction);

  // sort pending frame by z order
  std::sort(
      pending_frame_.begin(), pending_frame_.end(),
      [](const std::unique_ptr<Surface>& a, const std::unique_ptr<Surface>& b) {
        return a->z_order() < b->z_order();
      });

  int32_t first_surface_z_order =
      pending_frame_.empty() ? 0 : pending_frame_.front()->z_order();

  // Remove reused item (nullptr) from previous_frame_.
  auto it =
      std::remove(previous_frame_.begin(), previous_frame_.end(), nullptr);
  previous_frame_.erase(it, previous_frame_.end());

  for (auto& surface : pending_frame_) {
    // Try reusing Surface from previous frame.
    if (!surface->has_surface() && !previous_frame_.empty() &&
        surface->has_buffer()) {
      surface->SetSurface(std::move(previous_frame_.front()));
      previous_frame_.pop_front();
    }
    if (first_surface_z_order < 0) {
      // Make sure z order is non-negative.
      surface->SetZOrder(surface->z_order() - first_surface_z_order);
    }
    surface->Sync(&transaction);
  }

  // Clear all unused surface from previous frame.
  for (auto& surface : previous_frame_) {
    surface->Destroy(&transaction);
    surface.reset();
  }
  previous_frame_.clear();

  transaction.SetOnCommitCb(base::BindOnce(&OutputPresenterAndroid::OnCommit,
                                           weak_factory_.GetWeakPtr()),
                            task_runner_);
  transaction.SetOnCompleteCb(
      base::BindOnce(&OutputPresenterAndroid::OnComplete,
                     weak_factory_.GetWeakPtr()),
      task_runner_);

  // If there is a pending transaction, queue this transaction until an ack is
  // received.
  if (has_pending_transaction_ack_) {
    pending_transaction_queue_.emplace_back(std::move(transaction));
  } else {
    transaction.Apply();
    has_pending_transaction_ack_ = true;
  }

  std::swap(previous_frame_, pending_frame_);

  pending_commit_frame_data_queue_.emplace_back(
      std::move(root_overlay_access_), std::move(completion_callback),
      std::move(presentation_callback));
}

void OutputPresenterAndroid::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane,
    ScopedOverlayAccess* access) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const bool is_root = overlay_plane.is_root_render_pass;

  DCHECK_EQ(overlay_plane.is_solid_color, access == nullptr)
      << "access and is_solid_color don't match";

  // display_rect which is in the display (ArkWeb area) coordinate.
  const gfx::RectF& display_rect = overlay_plane.display_rect;
  gfx::Rect dst_rect = gfx::ToEnclosingRect(display_rect);
  gfx::Rect crop_rect;
  gfx::Point position;
  gfx::Vector2dF scale = {1.0f, 1.0f};

  if (is_root) {
    DCHECK(!root_overlay_access_);
    root_overlay_access_ = access;
    crop_rect = gfx::Rect(GetRotatedSize(size_, transform_));
  } else {
    if (overlay_plane.is_solid_color) {
      // For solid color quad.
      crop_rect = gfx::Rect(dst_rect.size());
      position = dst_rect.origin();
      scale = gfx::Vector2dF(1.0f, 1.0f);
    } else {
      // TODO: figure out how to use overlay_plane.clip_rect
      // For texture quad.

      // Buffer size in pixels
      const gfx::Size& buffer_size = overlay_plane.resource_size_in_pixels;

      // Caculate source rect based on the uv_rect which coordinates are between
      // [0.0, 1.0].
      crop_rect = gfx::ToEnclosingRect(
          gfx::ScaleRect(overlay_plane.uv_rect, gfx::SizeF(buffer_size)));

      // When the video is being scrolled offscreen DisplayCompositor will crop
      // it to only visible portion and adjust uv_rect accordingly. When the
      // video is smaller than the surface is can lead to the crop rect being
      // less than a pixel in size. This adjusts the crop rect size to at least
      // 1 pixel as we want to stretch last visible pixel line/column in this
      // case. Note: We will do it even if crop_rect width/height is exact 0.0f.
      // In reality this should never happen and there is no way to display
      // video with empty crop rect, so display compositor should not request
      // this.
      if (crop_rect.width() == 0) {
        crop_rect.set_width(1);
        if (crop_rect.right() > buffer_size.width()) {
          crop_rect.set_x(buffer_size.width() - 1);
        }
      }
      if (crop_rect.height() == 0) {
        crop_rect.set_height(1);
        if (crop_rect.bottom() > buffer_size.height()) {
          crop_rect.set_y(buffer_size.height() - 1);
        }
      }
      // Make sure crop_rect is insize of the buffer.
      crop_rect.Intersect(gfx::Rect(buffer_size));
      DCHECK_GT(crop_rect.width(), 0)
          << " crop_rect: " << crop_rect.ToString()
          << ", buffer_size: " << buffer_size.ToString();
      DCHECK_GT(crop_rect.height(), 0)
          << " crop_rect: " << crop_rect.ToString()
          << ", buffer_size: " << buffer_size.ToString();

      // Calculate position and scale.
      scale = gfx::Vector2dF(static_cast<float>(dst_rect.width()) /
                                 static_cast<float>(crop_rect.width()),
                             static_cast<float>(dst_rect.height()) /
                                 static_cast<float>(crop_rect.height()));
      position = dst_rect.origin() - gfx::Vector2d(crop_rect.x() * scale.x(),
                                                   crop_rect.y() * scale.y());
    }
  }

  auto buffer_fence_sync =
      access ? access->GetAHardwareBufferFenceSync() : nullptr;
  Surface* surface = nullptr;

  if (buffer_fence_sync) {
    auto buffer = buffer_fence_sync->TakeBuffer();
    // For plane with buffer.
    DCHECK(!overlay_plane.is_solid_color);

    // Find surface with same buffer in previous frame first.
    auto it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                           [&](const std::unique_ptr<Surface>& surface) {
                             return surface &&
                                    surface->buffer().get() == buffer.get();
                           });
    if (it == previous_frame_.end()) {
      // Find surface with same display rect in previous frame.
      it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                        [&](const std::unique_ptr<Surface>& surface) {
                          return surface && surface->position() == position &&
                                 surface->scale() == scale &&
                                 surface->crop() == crop_rect;
                        });
    }

    if (it != previous_frame_.end()) {
      // Found a surface with same buffer or bounds
      pending_frame_.push_back(std::move(*it));
    } else {
      DCHECK(root_surface_);
      // Cannot find a surface with same buffer or bounds, so create a new one.
      pending_frame_.emplace_back(
          std::make_unique<Surface>(task_runner_, root_surface_.get()));
    }

    surface = pending_frame_.back().get();

    if (surface->buffer().get() != buffer.get()) {
      auto available_fence = buffer_fence_sync->TakeAvailableFence();
      ScopedBufferReleaseData data(access, std::move(buffer_fence_sync),
                                   is_root);
      auto callback =
          base::BindOnce(&OutputPresenterAndroid::OnBufferReleased,
                         weak_factory_.GetWeakPtr(), std::move(data));

      // We need track buffer which are being used by RS. For root surface,
      // the buffer is considered being released when OnComplete is called.
      auto damage_rect = overlay_plane.damage_rect.IsEmpty()
                             ? gfx::Rect(overlay_plane.resource_size_in_pixels)
                             : ToEnclosingRect(overlay_plane.damage_rect);
      surface->SetBuffer(std::move(buffer), std::move(available_fence),
                         damage_rect, std::move(callback));
    }
  } else {
    // For a solid plane.
    DCHECK(overlay_plane.is_solid_color);

    // Find a surface with the same bounds.
    auto it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                           [&](const std::unique_ptr<Surface>& surface) {
                             if (!surface) {
                               return false;
                             }
                             if (surface->has_buffer()) {
                               return false;
                             }
                             return surface->position() == position &&
                                    surface->scale() == scale &&
                                    surface->crop() == crop_rect;
                           });

    // If cannot find a surface with the same bounds, try to find a surface for
    // any solid color plane.
    if (it == previous_frame_.end()) {
      it = std::find_if(previous_frame_.begin(), previous_frame_.end(),
                        [&](const std::unique_ptr<Surface>& surface) {
                          return surface && !surface->has_buffer();
                        });
    }

    if (it != previous_frame_.end()) {
      pending_frame_.push_back(std::move(*it));
    } else {
      DCHECK(root_surface_);
      pending_frame_.emplace_back(
          std::make_unique<Surface>(task_runner_, root_surface_.get()));
    }

    surface = pending_frame_.back().get();
  }

  // Make sure surface is visible.
  surface->SetVisibility(true);
  surface->SetZOrder(overlay_plane.plane_z_order);
  surface->SetBufferAlpha(overlay_plane.opacity);
  surface->SetOpaque(overlay_plane.is_opaque);

  // Only root buffer is always in hardware physical direction(portrait), so it
  // need to be rotated to the current screen logic rotation direction.
  auto transform = is_root ? transform_ : gfx::OVERLAY_TRANSFORM_NONE;
  surface->SetBufferTransform(transform);
  surface->SetPosition(position);
  surface->SetCrop(crop_rect);
  surface->SetScale(scale);

  // Set the surface's background color.
  if (overlay_plane.color.has_value()) {
    surface->SetColor({overlay_plane.color->fR, overlay_plane.color->fG,
                       overlay_plane.color->fB, overlay_plane.color->fA});
  } else {
    surface->SetColor(kColorTransparent);
  }
}

void OutputPresenterAndroid::OnCommit() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto current_frame_data = std::move(pending_commit_frame_data_queue_.front());
  pending_commit_frame_data_queue_.pop_front();

  // If the root buffer is not changed, we set previous access to null, so the
  // completion callback of previouse frame can be triggered without waiting for
  // the buffer is released by system composor.
  if (!pending_complete_frame_data_queue_.empty() &&
      pending_complete_frame_data_queue_.back().access ==
          current_frame_data.access) {
    pending_complete_frame_data_queue_.back().access = nullptr;
  }

  pending_complete_frame_data_queue_.push_back(std::move(current_frame_data));

  // Apply a transaction which is pending on the commit event of the previous
  // frame.
  if (!pending_transaction_queue_.empty()) {
    auto transactoin = std::move(pending_transaction_queue_.front());
    pending_transaction_queue_.pop_front();
    transactoin.Apply();
  } else {
    has_pending_transaction_ack_ = false;
  }
}

void OutputPresenterAndroid::OnComplete(
    gfx::SurfaceControl::TransactionStats stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(!pending_complete_frame_data_queue_.empty());
  auto& data = pending_complete_frame_data_queue_.front();
  data.completed = true;

  TriggerPendingCompletionCallbacks();
}

void OutputPresenterAndroid::OnBufferReleased(ScopedBufferReleaseData data,
                                              base::ScopedFD release_fence_fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Below code will crash for root overlay.
  // TODO(penghuang): figure out how to handle it properly
  // Probably it causes the artifacts on screen.
  if (release_fence_fd.is_valid()) {
    data.buffer_fence_sync()->SetReadFence(std::move(release_fence_fd));
  }

  if (data.is_root()) {
    // A buffer for the root surface is released, we can check and trigger
    // compltion callback for that frame.
    TriggerPendingCompletionCallbacks();
  }
}

void OutputPresenterAndroid::TriggerPendingCompletionCallbacks() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  while (!pending_complete_frame_data_queue_.empty()) {
    auto& data = pending_complete_frame_data_queue_.front();
    if (!data.completed) {
      break;
    }
    if (data.access && data.access->IsInUseByWindowServer()) {
      break;
    }
    data.access = nullptr;
    std::move(data.completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
    std::move(data.presentation_callback)
        .Run(gfx::PresentationFeedback(base::TimeTicks::Now(),
                                       base::TimeDelta(),
                                       /*flags=*/0));
    pending_complete_frame_data_queue_.pop_front();
  }
}

}  // namespace viz
