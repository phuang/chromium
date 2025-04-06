// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_DIRECT_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_OHOS_DIRECT_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/ohos/codec_image.h"
#include "media/gpu/ohos/shared_image_video_provider.h"
#include "media/gpu/ohos/video_frame_factory.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class GpuSharedImageVideoFactory;

class MEDIA_GPU_EXPORT DirectSharedImageVideoProvider
    : public SharedImageVideoProvider,
      public gpu::RefCountedLockHelperDrDc {
 public:
  DirectSharedImageVideoProvider(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetStubCB get_stub_cb,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  DirectSharedImageVideoProvider(const DirectSharedImageVideoProvider&) =
      delete;
  DirectSharedImageVideoProvider& operator=(
      const DirectSharedImageVideoProvider&) = delete;

  ~DirectSharedImageVideoProvider() override;

  void Initialize(GpuInitCB get_stub_cb) override;
  void RequestImage(ImageReadyCB cb, const ImageSpec& spec) override;

 private:
  base::SequenceBound<GpuSharedImageVideoFactory> gpu_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

class GpuSharedImageVideoFactory
    : public gpu::CommandBufferStub::DestructionObserver {
 public:
  explicit GpuSharedImageVideoFactory(
      SharedImageVideoProvider::GetStubCB get_stub_cb);

  GpuSharedImageVideoFactory(const GpuSharedImageVideoFactory&) = delete;
  GpuSharedImageVideoFactory& operator=(const GpuSharedImageVideoFactory&) =
      delete;

  ~GpuSharedImageVideoFactory() override;

  void Initialize(SharedImageVideoProvider::GpuInitCB init_cb);

  using FactoryImageReadyCB = SharedImageVideoProvider::ImageReadyCB;

  void CreateImage(FactoryImageReadyCB cb,
                   const SharedImageVideoProvider::ImageSpec& spec,
                   scoped_refptr<gpu::RefCountedLock> drdc_lock);

 private:
  bool CreateImageInternal(const SharedImageVideoProvider::ImageSpec& spec,
                           gpu::Mailbox mailbox,
                           scoped_refptr<CodecImage> image,
                           scoped_refptr<gpu::RefCountedLock>);

  void OnWillDestroyStub(bool have_context) override;

  raw_ptr<gpu::CommandBufferStub> stub_ = nullptr;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<GpuSharedImageVideoFactory> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_DIRECT_SHARED_IMAGE_VIDEO_PROVIDER_H_
