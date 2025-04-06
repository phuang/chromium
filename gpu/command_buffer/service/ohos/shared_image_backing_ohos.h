// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_OHOS_SHARED_IMAGE_BACKING_OHOS_H_
#define GPU_COMMAND_BUFFER_SERVICE_OHOS_SHARED_IMAGE_BACKING_OHOS_H_

#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

namespace gpu {

class SharedImageBackingOhos : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingOhos(const Mailbox& mailbox,
                         viz::SharedImageFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         std::string debug_label,
                         size_t estimated_size,
                         bool is_thread_safe);

  ~SharedImageBackingOhos() override;
  SharedImageBackingOhos(const SharedImageBackingOhos&) = delete;
  SharedImageBackingOhos& operator=(const SharedImageBackingOhos&) = delete;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_OHOS_SHARED_IMAGE_BACKING_OHOS_H_
