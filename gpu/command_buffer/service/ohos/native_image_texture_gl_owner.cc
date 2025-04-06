// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ohos/native_image_texture_gl_owner.h"

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/abstract_texture_ohos.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/scoped_async_trace.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {

std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrentIfNeeded(
    gpu::NativeImageTextureOwner* texture_owner) {
  gl::GLContext* context = texture_owner->GetContext();
  if (context->IsCurrent(nullptr)) {
    return nullptr;
  }

  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      context, texture_owner->GetSurface());
  if (!context->IsCurrent(nullptr)) {
    LOG(ERROR) << "Failed to make context current in CodecImage. Subsequent "
                  "UpdateTexImage may fail.";
  }
  return scoped_current;
}
}  // namespace

NativeImageTextureGlOwner::NativeImageTextureGlOwner(
    std::unique_ptr<AbstractTextureOHOS> texture,
    scoped_refptr<SharedContextState> context_state)
    : NativeImageTextureOwner(true /*binds_texture_on_update */,
                              std::move(texture),
                              std::move(context_state)),
      native_image_(gl::OhosNativeImage::Create(GetTextureId())),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()) {
  DCHECK(context_);
  DCHECK(surface_);
}

NativeImageTextureGlOwner::~NativeImageTextureGlOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ReleaseResources();
}

void NativeImageTextureGlOwner::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  native_image_ = nullptr;
}

void NativeImageTextureGlOwner::SetFrameAvailableCallback(
    const base::RepeatingClosure& frame_available_cb) {
  DCHECK(!is_frame_available_callback_set_);

  is_frame_available_callback_set_ = true;
  native_image_->SetFrameAvailableCallback(frame_available_cb);
}

void NativeImageTextureGlOwner::RunWhenBufferIsAvailable(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void NativeImageTextureGlOwner::UpdateNativeImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (native_image_) {
    auto scoped_make_current = MakeCurrentIfNeeded(this);
    if (scoped_make_current && !scoped_make_current->IsContextCurrent()) {
      return;
    }

    ScopedRestoreTextureBinding scoped_restore_texture;
    native_image_->UpdateNativeImage();
  }
}

void NativeImageTextureGlOwner::EnsureNativeImageBound(GLuint service_id) {
  DCHECK_EQ(service_id, GetTextureId());
}

void NativeImageTextureGlOwner::ReleaseNativeImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (native_image_) {
    native_image_->ReleaseNativeImage();
  }
}

gl::GLContext* NativeImageTextureGlOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* NativeImageTextureGlOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

void* NativeImageTextureGlOwner::AquireOhosNativeWindow() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (native_image_ != nullptr) {
    return native_image_->AquireOhosNativeWindow();
  } else {
    return nullptr;
  }
}

bool NativeImageTextureGlOwner::GetCodedSizeAndVisibleRect(
    gfx::Size rotated_visible_size,
    gfx::Size* coded_size,
    gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);

  if (!native_image_) {
    *visible_rect = gfx::Rect();
    *coded_size = gfx::Size();
    return false;
  }

  float mtx[16];
  native_image_->GetTransformMatrix(mtx);

  bool result =
      DecomposeTransform(mtx, rotated_visible_size, coded_size, visible_rect);

  constexpr gfx::Rect kMaxRect(16536, 16536);
  gfx::Rect coded_rect(*coded_size);

  if (!result || !coded_rect.Contains(*visible_rect) ||
      !kMaxRect.Contains(coded_rect)) {
    gfx::Size coded_size_for_debug = *coded_size;
    gfx::Rect visible_rect_for_debug = *visible_rect;

    *coded_size = rotated_visible_size;
    *visible_rect = gfx::Rect(rotated_visible_size);

    base::debug::Alias(mtx);
    base::debug::Alias(&coded_size_for_debug);
    base::debug::Alias(&visible_rect_for_debug);

    LOG(ERROR) << "Wrong matrix decomposition: coded: "
               << coded_size_for_debug.ToString()
               << "visible: " << visible_rect_for_debug.ToString()
               << "matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
               << ", " << mtx[5] << ", " << mtx[12] << ", " << mtx[13];

    base::debug::DumpWithoutCrashing();
  }

  return true;
}

// static
bool NativeImageTextureGlOwner::DecomposeTransform(
    float mtx[16],
    gfx::Size rotated_visible_size,
    gfx::Size* coded_size,
    gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);

  if (rotated_visible_size.width() < 4 || rotated_visible_size.height() < 4) {
    *coded_size = rotated_visible_size;
    *visible_rect = gfx::Rect(rotated_visible_size);

    return true;
  }

  float sx;
  float sy;
  *visible_rect = gfx::Rect();

  if (mtx[0]) {
    LOG_IF(DFATAL, mtx[1] || mtx[4] || !mtx[5])
        << "Invalid matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
        << ", " << mtx[5];

    sx = mtx[0];
    sy = mtx[5];

    visible_rect->set_size(rotated_visible_size);
  } else {
    LOG_IF(DFATAL, !mtx[1] || !mtx[4] || mtx[5])
        << "Invalid matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
        << ", " << mtx[5];

    sx = mtx[4];
    sy = mtx[1];

    visible_rect->set_width(rotated_visible_size.height());
    visible_rect->set_height(rotated_visible_size.width());
  }

  float tx = sx > 0 ? mtx[12] : (sx + mtx[12]);
  float ty = sy > 0 ? mtx[13] : (sy + mtx[13]);

  sx = std::abs(sx);
  sy = std::abs(sy);
  if (!sx || !sy) {
    return false;
  }

  *coded_size = visible_rect->size();

  const float possible_shrinks_amounts[] = {1.0f, 0.5f, 0.0f};

  for (float shrink_amount : possible_shrinks_amounts) {
    if (sx < 1.0f) {
      coded_size->set_width(
          std::round((visible_rect->width() - 2.0f * shrink_amount) / sx));
      visible_rect->set_x(std::round(tx * coded_size->width() - shrink_amount));
    }
    if (sy < 1.0f) {
      coded_size->set_height(
          std::round((visible_rect->height() - 2.0f * shrink_amount) / sy));
      visible_rect->set_y(
          std::round(ty * coded_size->height() - shrink_amount));
    }

    if (visible_rect->x() >= 0 && visible_rect->y() >= 0) {
      break;
    }
  }
  return true;
}

}  // namespace gpu
