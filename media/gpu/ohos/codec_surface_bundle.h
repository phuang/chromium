// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_CODEC_SURFACE_BUNDLE_H_
#define MEDIA_GPU_OHOS_CODEC_SURFACE_BUNDLE_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/ohos/codec_buffer_wait_coordinator.h"

namespace gpu {
class RefCountedLock;
}  // namespace gpu

namespace media {
class MEDIA_GPU_EXPORT CodecSurfaceBundle
    : public base::RefCountedDeleteOnSequence<CodecSurfaceBundle> {
 public:
  CodecSurfaceBundle();
  explicit CodecSurfaceBundle(
      scoped_refptr<gpu::NativeImageTextureOwner> texture_owner,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  CodecSurfaceBundle(const CodecSurfaceBundle&) = delete;
  CodecSurfaceBundle& operator=(const CodecSurfaceBundle&) = delete;

  void* GetOHOSNativeWindow() const;

  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator()
      const {
    return codec_buffer_wait_coordinator_;
  }

 private:
  ~CodecSurfaceBundle();
  friend class base::RefCountedDeleteOnSequence<CodecSurfaceBundle>;
  friend class base::DeleteHelper<CodecSurfaceBundle>;

  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;

  void* ohos_native_window_;

  base::WeakPtrFactory<CodecSurfaceBundle> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_CODEC_SURFACE_BUNDLE_H_
