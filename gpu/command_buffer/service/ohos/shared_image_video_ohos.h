// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_OHOS_SHARED_IMAGE_VIDEO_OHOS_H_
#define GPU_COMMAND_BUFFER_SERVICE_OHOS_SHARED_IMAGE_VIDEO_OHOS_H_

#include <memory>

#include "gpu/command_buffer/service/ohos/shared_image_backing_ohos.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gpu {
struct Mailbox;
class AbstractTextureOHOS;
class RefCountedLock;
class StreamTextureSharedImageInterface;
class SharedContextState;
class NativeImageTextureOwner;

class GPU_GLES2_EXPORT SharedImageVideoOhos : public SharedImageBackingOhos {
 public:
  static std::unique_ptr<SharedImageVideoOhos> Create(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> context_state);

  ~SharedImageVideoOhos() override;

  SharedImageVideoOhos(const SharedImageVideoOhos&) = delete;
  SharedImageVideoOhos& operator=(const SharedImageVideoOhos&) = delete;

  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

 protected:
  SharedImageVideoOhos(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const gfx::ColorSpace color_space,
                       GrSurfaceOrigin surface_origin,
                       SkAlphaType alpha_type,
                       bool is_thread_safe);

  std::unique_ptr<AbstractTextureOHOS> GenAbstractTexture(
      const bool passthrough);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_OHOS_SHARED_IMAGE_VIDEO_OHOS_H_
