// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ohos/shared_image_video_ohos.h"

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_ohos.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "gpu/command_buffer/service/ohos/shared_image_video_ohos_native_image.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

SharedImageVideoOhos::SharedImageVideoOhos(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    bool is_thread_safe)
    : SharedImageBackingOhos(
          mailbox,
          viz::SinglePlaneFormat::kRGBA_8888,
          size,
          color_space,
          surface_origin,
          alpha_type,
          (SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE),
          {},
          viz::SinglePlaneFormat::kRGBA_8888.EstimatedSizeInBytes(size),
          is_thread_safe) {}

SharedImageVideoOhos::~SharedImageVideoOhos() {}

// Static.
std::unique_ptr<SharedImageVideoOhos> SharedImageVideoOhos::Create(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state) {
  return std::make_unique<SharedImageVideoOhosNativeImage>(
      mailbox, size, color_space, surface_origin, alpha_type,
      std::move(stream_texture_sii), std::move(context_state));
}

std::unique_ptr<AbstractTextureOHOS>
SharedImageVideoOhos::GenAbstractTexture(const bool passthrough) {
  if (passthrough) {
    return AbstractTextureOHOS::CreateForPassthrough(size());
  } else {
    return AbstractTextureOHOS::CreateForValidating(size());
  }
}

SharedImageBackingType SharedImageVideoOhos::GetType() const {
  return SharedImageBackingType::kVideo;
}

gfx::Rect SharedImageVideoOhos::ClearedRect() const {
  return gfx::Rect(size());
}

void SharedImageVideoOhos::SetClearedRect(const gfx::Rect& cleared_rect) {}

void SharedImageVideoOhos::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

}  // namespace gpu
