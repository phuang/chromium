// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_VIDEO_FRAME_FACTORY_H_
#define MEDIA_GPU_OHOS_VIDEO_FRAME_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_decoder.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class CodecOutputBuffer;
class CodecSurfaceBundle;
class VideoFrame;

class MEDIA_GPU_EXPORT VideoFrameFactory {
 public:
  using InitCB = base::RepeatingCallback<void(
      scoped_refptr<gpu::NativeImageTextureOwner>)>;
  using OnceOutputCB = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;

  VideoFrameFactory() = default;
  virtual ~VideoFrameFactory() = default;

  virtual void Initialize(InitCB init_cb) = 0;

  virtual void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) = 0;

  virtual void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      OnceOutputCB output_cb) = 0;

  virtual void RunAfterPendingVideoFrames(base::OnceClosure closure) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_VIDEO_FRAME_FACTORY_H_
