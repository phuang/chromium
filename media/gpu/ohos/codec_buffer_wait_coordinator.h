// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_CODEC_BUFFER_WAIT_COORDINATOR_H_
#define MEDIA_GPU_OHOS_CODEC_BUFFER_WAIT_COORDINATOR_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "media/base/tuneable.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

struct FrameAvailableEvent;

class MEDIA_GPU_EXPORT CodecBufferWaitCoordinator
    : public base::RefCountedThreadSafe<CodecBufferWaitCoordinator>,
      public gpu::RefCountedLockHelperDrDc {
 public:
  explicit CodecBufferWaitCoordinator(
      scoped_refptr<gpu::NativeImageTextureOwner> texture_owner,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  CodecBufferWaitCoordinator(const CodecBufferWaitCoordinator&) = delete;
  CodecBufferWaitCoordinator& operator=(const CodecBufferWaitCoordinator&) =
      delete;

  scoped_refptr<gpu::NativeImageTextureOwner> texture_owner() const {
    DCHECK(texture_owner_);
    return texture_owner_;
  }

  virtual void SetReleaseTimeToNow();

  virtual bool IsExpectingFrameAvailable();

  virtual void WaitForFrameAvailable();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

 protected:
  virtual ~CodecBufferWaitCoordinator();

 private:
  friend class base::RefCountedThreadSafe<CodecBufferWaitCoordinator>;

  scoped_refptr<gpu::NativeImageTextureOwner> texture_owner_;

  base::TimeTicks release_time_;
  scoped_refptr<FrameAvailableEvent> frame_available_event_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  Tuneable<base::TimeDelta> max_wait_ = {
      "MediaCodecOutputBufferMaxWaitTime", base::Milliseconds(0),
      base::Milliseconds(5), base::Milliseconds(20)};
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_CODEC_BUFFER_WAIT_COORDINATOR_H_
