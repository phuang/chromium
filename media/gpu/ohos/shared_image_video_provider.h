// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_OHOS_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include "base/functional/callback.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/ohos/codec_image.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class CommandBufferStub;
class SharedContextState;
struct SyncToken;
}  // namespace gpu

namespace media {

class MEDIA_GPU_EXPORT SharedImageVideoProvider {
 public:
  using GetStubCB = base::RepeatingCallback<gpu::CommandBufferStub*()>;
  using GpuInitCB =
      base::OnceCallback<void(scoped_refptr<gpu::SharedContextState>)>;

  struct ImageSpec {
    ImageSpec();
    ImageSpec(const gfx::Size& coded_size, uint64_t generation_id);
    ImageSpec(const ImageSpec&);
    ~ImageSpec();

    gfx::Size coded_size;

    gfx::ColorSpace color_space;

    uint64_t generation_id = 0;

    bool operator==(const ImageSpec&) const;
    bool operator!=(const ImageSpec&) const;
  };

  using ReleaseCB = base::OnceCallback<void(const gpu::SyncToken&)>;

  struct ImageRecord {
    ImageRecord();
    ImageRecord(ImageRecord&&);

    ImageRecord(const ImageRecord&) = delete;
    ImageRecord& operator=(const ImageRecord&) = delete;

    ~ImageRecord();

    gpu::Mailbox mailbox;

    // ClientSharedImage for the current shared image.
    scoped_refptr<gpu::ClientSharedImage> shared_image;

    ReleaseCB release_cb;

    scoped_refptr<CodecImageHolder> codec_image_holder;

    bool is_vulkan = false;
  };

  SharedImageVideoProvider() = default;

  SharedImageVideoProvider(const SharedImageVideoProvider&) = delete;
  SharedImageVideoProvider& operator=(const SharedImageVideoProvider&) = delete;

  virtual ~SharedImageVideoProvider() = default;

  using ImageReadyCB = base::OnceCallback<void(ImageRecord)>;

  virtual void Initialize(GpuInitCB gpu_init_cb) = 0;

  virtual void RequestImage(ImageReadyCB cb, const ImageSpec& spec) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_SHARED_IMAGE_VIDEO_PROVIDER_H_
