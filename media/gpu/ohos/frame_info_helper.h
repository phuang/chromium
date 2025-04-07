// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_FRAME_INFO_HELPER_H_
#define MEDIA_GPU_OHOS_FRAME_INFO_HELPER_H_

#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/ohos/shared_image_video_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace media {
class CodecOutputBufferRenderer;

class MEDIA_GPU_EXPORT FrameInfoHelper {
 public:
  struct FrameInfo {
    FrameInfo();
    ~FrameInfo();

    FrameInfo(FrameInfo&&);
    FrameInfo(const FrameInfo&);
    FrameInfo& operator=(const FrameInfo&);

    gfx::Size coded_size;
    gfx::Rect visible_rect;
    absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  };

  using FrameInfoReadyCB =
      base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                              FrameInfo)>;

  static std::unique_ptr<FrameInfoHelper> Create(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      SharedImageVideoProvider::GetStubCB get_stub_cb,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  virtual ~FrameInfoHelper() = default;

  virtual void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      FrameInfoReadyCB callback) = 0;

 protected:
  FrameInfoHelper() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_FRAME_INFO_HELPER_H_
