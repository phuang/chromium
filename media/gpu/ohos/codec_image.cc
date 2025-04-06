// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/codec_image.h"

#include <string>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

CodecImage::CodecImage(const gfx::Size& coded_size,
                       scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)), coded_size_(coded_size) {}

CodecImage::~CodecImage() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  NotifyUnused();
}

void CodecImage::Initialize(
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    bool is_texture_owner_backed) {
  DCHECK(output_buffer_renderer);
  output_buffer_renderer_ = std::move(output_buffer_renderer);
  is_texture_owner_backed_ = is_texture_owner_backed;
}

void CodecImage::AddUnusedCB(UnusedCB unused_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  unused_cbs_.push_back(std::move(unused_cb));
}

void CodecImage::NotifyUnused() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();

  ReleaseCodecBuffer();

  for (auto& cb : unused_cbs_)
    std::move(cb).Run(this);
  unused_cbs_.clear();
}

void CodecImage::NotifyOverlayPromotion(bool promotion,
                                        const gfx::Rect& bounds) {}

void CodecImage::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  ReleaseCodecBuffer();
}

void CodecImage::UpdateAndBindTexImage() {
  AssertAcquiredDrDcLock();
  RenderToTextureOwnerFrontBuffer();
}

bool CodecImage::HasTextureOwner() const {
  return !!texture_owner();
}

gpu::TextureBase* CodecImage::GetTextureBase() const {
  return texture_owner()->GetTextureBase();
}

bool CodecImage::RenderToFrontBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToFrontBuffer();
}

bool CodecImage::RenderToTextureOwnerBackBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;

  return output_buffer_renderer_->RenderToTextureOwnerBackBuffer();
}

bool CodecImage::RenderToTextureOwnerFrontBuffer() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToTextureOwnerFrontBuffer();
}

bool CodecImage::RenderToOverlay() {
  return false;
}

bool CodecImage::TextureOwnerBindsTextureOnUpdate() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->texture_owner()->binds_texture_on_update();
}

void CodecImage::ReleaseCodecBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  output_buffer_renderer_.reset();
}

CodecImageHolder::CodecImageHolder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<CodecImage> codec_image)
    : base::RefCountedDeleteOnSequence<CodecImageHolder>(
          std::move(task_runner)),
      codec_image_(std::move(codec_image)) {}

CodecImageHolder::~CodecImageHolder() = default;

}  // namespace media
