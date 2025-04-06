// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/codec_surface_bundle.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/ohos/ohos_media_decoder_bridge_impl.h"

namespace media {

CodecSurfaceBundle::CodecSurfaceBundle()
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunner::GetCurrentDefault()) {}

CodecSurfaceBundle::CodecSurfaceBundle(
    scoped_refptr<gpu::NativeImageTextureOwner> texture_owner,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      codec_buffer_wait_coordinator_(
          base::MakeRefCounted<CodecBufferWaitCoordinator>(
              std::move(texture_owner),
              std::move(drdc_lock))),
      ohos_native_window_(codec_buffer_wait_coordinator_->texture_owner()
                              ->AquireOhosNativeWindow()) {}

CodecSurfaceBundle::~CodecSurfaceBundle() {
  if (!codec_buffer_wait_coordinator_)
    return;

  auto task_runner =
      codec_buffer_wait_coordinator_->texture_owner()->task_runner();
  if (task_runner->RunsTasksInCurrentSequence()) {
    codec_buffer_wait_coordinator_->texture_owner()->ReleaseNativeImage();
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&gpu::NativeImageTextureOwner::ReleaseNativeImage,
                       codec_buffer_wait_coordinator_->texture_owner()));
  }
}

void* CodecSurfaceBundle::GetOHOSNativeWindow() const {
  return ohos_native_window_;
}

}  // namespace media
