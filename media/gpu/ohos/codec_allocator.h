// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_CODEC_ALLOCATOR_H_
#define MEDIA_GPU_OHOS_CODEC_ALLOCATOR_H_

#include <cstddef>
#include <string>

#include <memory>

#include "base/functional/callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/ohos/ohos_media_decoder_bridge_impl.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class TickClock;
}

namespace media {
class MEDIA_GPU_EXPORT CodecAllocator {
 public:
  static CodecAllocator* GetInstance(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  CodecAllocator(const CodecAllocator&) = delete;
  CodecAllocator& operator=(const CodecAllocator&) = delete;

  using CodecFactoryCB =
      base::RepeatingCallback<std::unique_ptr<MediaCodecDecoderBridgeImpl>(
          const VideoBridgeCodecConfig& config)>;

  using CodecCreatedCB =
      base::OnceCallback<void(std::unique_ptr<MediaCodecDecoderBridgeImpl>)>;
  virtual void CreateMediaCodecAsync(
      CodecCreatedCB codec_created_cb,
      std::unique_ptr<VideoBridgeCodecConfig> codec_config);

  virtual void ReleaseMediaCodec(
      std::unique_ptr<MediaCodecDecoderBridgeImpl> codec,
      base::OnceClosure codec_released_cb);

 protected:
  friend class base::NoDestructor<CodecAllocator>;

  CodecAllocator(CodecFactoryCB factory_cb,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);
  virtual ~CodecAllocator();

 private:
  friend class CodecAllocatorTest;

  void OnCodecCreated(base::TimeTicks start_time,
                      CodecCreatedCB codec_created_cb,
                      std::unique_ptr<MediaCodecDecoderBridgeImpl> codec);

  void OnCodecReleased(base::TimeTicks start_time,
                       base::OnceClosure codec_released_cb);

  base::SequencedTaskRunner* SelectCodecTaskRunner();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  const CodecFactoryCB factory_cb_;

  raw_ptr<const base::TickClock> tick_clock_;

  scoped_refptr<base::SequencedTaskRunner> primary_task_runner_;
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_CODEC_ALLOCATOR_H_
