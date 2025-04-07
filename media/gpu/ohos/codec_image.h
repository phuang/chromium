// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_CODEC_IMAGE_H_
#define MEDIA_GPU_OHOS_CODEC_IMAGE_H_

#include <cstdint>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/ohos/codec_output_buffer_renderer.h"

namespace media {
class MEDIA_GPU_EXPORT CodecImage
    : public gpu::StreamTextureSharedImageInterface,
      gpu::RefCountedLockHelperDrDc {
 public:
  using UnusedCB = base::OnceCallback<void(CodecImage*)>;

  CodecImage(const gfx::Size& coded_size,
             scoped_refptr<gpu::RefCountedLock> drdc_lock);

  CodecImage(const CodecImage&) = delete;
  CodecImage& operator=(const CodecImage&) = delete;

  void Initialize(
      std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
      bool is_texture_owner_backed);

  void AddUnusedCB(UnusedCB unused_cb);

  void NotifyUnused();
  void ReleaseResources() override;
  void UpdateAndBindTexImage() override;
  bool HasTextureOwner() const override;
  gpu::TextureBase* GetTextureBase() const override;
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
  bool RenderToOverlay() override;
  bool TextureOwnerBindsTextureOnUpdate() override;

  bool was_rendered_to_front_buffer() const {
    return output_buffer_renderer_
               ? output_buffer_renderer_->was_rendered_to_front_buffer()
               : false;
  }

  bool is_texture_owner_backed() const { return is_texture_owner_backed_; }

  scoped_refptr<gpu::NativeImageTextureOwner> texture_owner() const {
    return output_buffer_renderer_ ? output_buffer_renderer_->texture_owner()
                                   : nullptr;
  }

  bool RenderToFrontBuffer();
  bool RenderToTextureOwnerBackBuffer();
  virtual void ReleaseCodecBuffer();

  CodecOutputBuffer* get_codec_output_buffer_for_testing() const {
    return output_buffer_renderer_
               ? output_buffer_renderer_->get_codec_output_buffer_for_testing()
               : nullptr;
  }

 protected:
  ~CodecImage() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CodecImageTest, RenderAfterUnusedDoesntCrash);

  std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer_;

  bool RenderToTextureOwnerFrontBuffer();

  bool is_texture_owner_backed_ = false;

  gfx::Rect most_recent_bounds_;

  gfx::Size coded_size_;

  std::vector<UnusedCB> unused_cbs_;

  THREAD_CHECKER(gpu_main_thread_checker_);
};

class MEDIA_GPU_EXPORT CodecImageHolder
    : public base::RefCountedDeleteOnSequence<CodecImageHolder> {
 public:
  CodecImageHolder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   scoped_refptr<CodecImage> codec_image);

  CodecImageHolder(const CodecImageHolder&) = delete;
  CodecImageHolder& operator=(const CodecImageHolder&) = delete;

  CodecImage* codec_image_raw() const { return codec_image_.get(); }

 private:
  virtual ~CodecImageHolder();

  friend class base::RefCountedDeleteOnSequence<CodecImageHolder>;
  friend class base::DeleteHelper<CodecImageHolder>;

  scoped_refptr<CodecImage> codec_image_;
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_CODEC_IMAGE_H_
