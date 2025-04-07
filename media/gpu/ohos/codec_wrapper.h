// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_OHOS_CODEC_WRAPPER_H_
#define MEDIA_GPU_OHOS_CODEC_WRAPPER_H_

#include <cstddef>
#include <cstdint>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/decoder_buffer.h"
#include "media/base/ohos/ohos_media_decoder_bridge_impl.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/ohos/codec_surface_bundle.h"

namespace media {
class CodecWrapper;
class CodecWrapperImpl;

using CodecSurfacePair = std::pair<std::unique_ptr<MediaCodecDecoderBridgeImpl>,
                                   scoped_refptr<CodecSurfaceBundle>>;

class MEDIA_GPU_EXPORT CodecOutputBuffer {
 public:
  CodecOutputBuffer(const CodecOutputBuffer&) = delete;
  CodecOutputBuffer& operator=(const CodecOutputBuffer&) = delete;

  ~CodecOutputBuffer();

  bool ReleaseToSurface();

  gfx::Size size() const { return size_; }

  void set_render_cb(base::OnceClosure render_cb) {
    render_cb_ = std::move(render_cb);
  }

  const gfx::ColorSpace& color_space() const { return color_space_; }

 private:
  friend class CodecWrapperImpl;
  CodecOutputBuffer(scoped_refptr<CodecWrapperImpl> codec,
                    int64_t id,
                    const gfx::Size& size,
                    const gfx::ColorSpace& color_space);

  scoped_refptr<CodecWrapperImpl> codec_;
  int64_t id_;
  bool was_rendered_ = false;
  gfx::Size size_;
  base::OnceClosure render_cb_;
  gfx::ColorSpace color_space_;
};

class MEDIA_GPU_EXPORT CodecWrapper {
 public:
  using OutputReleasedCB = base::RepeatingCallback<void(bool)>;
  CodecWrapper(CodecSurfacePair codec_surface_pair,
               OutputReleasedCB output_buffer_release_cb,
               scoped_refptr<base::SequencedTaskRunner> release_task_runner);

  CodecWrapper(const CodecWrapper&) = delete;
  CodecWrapper& operator=(const CodecWrapper&) = delete;

  ~CodecWrapper();

  CodecSurfacePair TakeCodecSurfacePair();

  bool IsFlushed() const;

  bool IsDraining() const;

  bool IsDrained() const;

  bool HasUnreleasedOutputBuffers() const;

  void DiscardOutputBuffers();

  bool Flush();

  bool SetSurface(scoped_refptr<CodecSurfaceBundle> surface_bundle);

  scoped_refptr<CodecSurfaceBundle> SurfaceBundle();

  enum class QueueStatus { kOk, kError, kTryAgainLater };
  QueueStatus QueueInputBuffer(const DecoderBuffer& buffer);

  enum class DequeueStatus { kOk, kError, kTryAgainLater };
  DequeueStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);

 private:
  scoped_refptr<CodecWrapperImpl> impl_;
};

}  // namespace media

#endif  // MEDIA_GPU_OHOS_CODEC_WRAPPER_H_
