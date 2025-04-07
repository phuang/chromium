// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/codec_wrapper.h"

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "base/trace_event/trace_event.h"

namespace media {
class CodecWrapperImpl : public base::RefCountedThreadSafe<CodecWrapperImpl> {
 public:
  CodecWrapperImpl(
      CodecSurfacePair codec_surface_pair,
      CodecWrapper::OutputReleasedCB output_buffer_release_cb,
      scoped_refptr<base::SequencedTaskRunner> release_task_runner);

  CodecWrapperImpl(const CodecWrapperImpl&) = delete;
  CodecWrapperImpl& operator=(const CodecWrapperImpl&) = delete;

  using DequeueStatus = CodecWrapper::DequeueStatus;
  using QueueStatus = CodecWrapper::QueueStatus;

  CodecSurfacePair TakeCodecSurfacePair();
  bool HasUnreleasedOutputBuffers() const;
  void DiscardOutputBuffers();
  bool IsFlushed() const;
  bool IsDraining() const;
  bool IsDrained() const;
  bool Flush();
  bool SetSurface(scoped_refptr<CodecSurfaceBundle> surface_bundle);
  scoped_refptr<CodecSurfaceBundle> SurfaceBundle();
  QueueStatus QueueInputBuffer(const DecoderBuffer& buffer);
  DequeueStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);
  bool ReleaseCodecOutputBuffer(int64_t id, bool render);

 private:
  enum class State {
    kError,
    kFlushed,
    kRunning,
    kDraining,
    kDrained,
  };

  friend base::RefCountedThreadSafe<CodecWrapperImpl>;
  ~CodecWrapperImpl();

  void DiscardOutputBuffers_Locked();

  mutable base::Lock lock_;

  State state_;

  scoped_refptr<CodecSurfaceBundle> surface_bundle_;

  std::unique_ptr<MediaCodecDecoderBridgeImpl> codec_;

  int64_t next_buffer_id_;

  base::flat_map<int64_t, int> buffer_ids_;

  absl::optional<int> owned_input_buffer_;

  gfx::Size size_;

  CodecWrapper::OutputReleasedCB output_buffer_release_cb_;

  bool elided_eos_pending_ = false;

  gfx::ColorSpace color_space_ = gfx::ColorSpace::CreateSRGB();

  scoped_refptr<base::SequencedTaskRunner> release_task_runner_;
};

CodecOutputBuffer::CodecOutputBuffer(scoped_refptr<CodecWrapperImpl> codec,
                                     int64_t id,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space)
    : codec_(std::move(codec)),
      id_(id),
      size_(size),
      color_space_(color_space) {}

CodecOutputBuffer::~CodecOutputBuffer() {
  if (!was_rendered_ && codec_) {
    codec_->ReleaseCodecOutputBuffer(id_, false);
  }
}

bool CodecOutputBuffer::ReleaseToSurface() {
  was_rendered_ = true;
  auto result = codec_->ReleaseCodecOutputBuffer(id_, true);
  return result;
}

CodecWrapperImpl::CodecWrapperImpl(
    CodecSurfacePair codec_surface_pair,
    CodecWrapper::OutputReleasedCB output_buffer_release_cb,
    scoped_refptr<base::SequencedTaskRunner> release_task_runner)
    : state_(State::kFlushed),
      surface_bundle_(std::move(codec_surface_pair.second)),
      codec_(std::move(codec_surface_pair.first)),
      next_buffer_id_(0),
      output_buffer_release_cb_(std::move(output_buffer_release_cb)),
      release_task_runner_(std::move(release_task_runner)) {
  DVLOG(2) << __func__;
}

CodecWrapperImpl::~CodecWrapperImpl() = default;

CodecSurfacePair CodecWrapperImpl::TakeCodecSurfacePair() {
  base::AutoLock l(lock_);
  if (!codec_) {
    return {nullptr, nullptr};
  }
  DiscardOutputBuffers_Locked();
  return {std::move(codec_), std::move(surface_bundle_)};
}

bool CodecWrapperImpl::IsFlushed() const {
  base::AutoLock l(lock_);
  return state_ == State::kFlushed;
}

bool CodecWrapperImpl::IsDraining() const {
  base::AutoLock l(lock_);
  return state_ == State::kDraining;
}

bool CodecWrapperImpl::IsDrained() const {
  base::AutoLock l(lock_);
  return state_ == State::kDrained;
}

bool CodecWrapperImpl::HasUnreleasedOutputBuffers() const {
  base::AutoLock l(lock_);
  return !buffer_ids_.empty();
}

void CodecWrapperImpl::DiscardOutputBuffers() {
  DVLOG(3) << __func__;
  base::AutoLock l(lock_);
  DiscardOutputBuffers_Locked();
}

void CodecWrapperImpl::DiscardOutputBuffers_Locked() {
  DVLOG(3) << __func__;
  lock_.AssertAcquired();
  for (auto& kv : buffer_ids_) {
    codec_->ReleaseOutputBuffer(kv.second, false);
  }
  buffer_ids_.clear();
}

bool CodecWrapperImpl::Flush() {
  DVLOG(3) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  // Dequeued buffers are invalidated by flushing.
  buffer_ids_.clear();
  owned_input_buffer_.reset();
  auto status = codec_->FlushBridgeDecoder();
  if (status == DecoderAdapterCode::DECODER_ERROR) {
    state_ = State::kError;
    return false;
  }
  state_ = State::kFlushed;
  elided_eos_pending_ = false;
  return true;
}

CodecWrapperImpl::QueueStatus CodecWrapperImpl::QueueInputBuffer(
    const DecoderBuffer& buffer) {
  TRACE_EVENT0("media", "CodecWrapperImpl::QueueInputBuffer");
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);
  DVLOG(3) << "QueueInputBuffer timedelta = "
           << base::TimeDelta().InMicroseconds();

  if (buffer.end_of_stream()) {
    if (state_ == State::kFlushed || state_ == State::kDrained) {
      elided_eos_pending_ = true;
    } else {
      auto res = codec_->QueueInputBufferEOS();
      if (res == DecoderAdapterCode::DECODER_RETRY) {
        return QueueStatus::kTryAgainLater;
      }
    }

    state_ = State::kDraining;
    return QueueStatus::kOk;
  }
  DecoderAdapterCode status;

  status = codec_->QueueInputBuffer(buffer.data(), buffer.size(),
                                    buffer.timestamp().ToInternalValue());
  switch (status) {
    case DecoderAdapterCode::DECODER_OK:
      state_ = State::kRunning;
      return QueueStatus::kOk;
    case DecoderAdapterCode::DECODER_ERROR:
      state_ = State::kError;
      return QueueStatus::kError;
    case DecoderAdapterCode::DECODER_RETRY:
      return QueueStatus::kTryAgainLater;
    default:
      NOTREACHED();
      return QueueStatus::kError;
  }
}

CodecWrapperImpl::DequeueStatus CodecWrapperImpl::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  TRACE_EVENT0("media", "CodecWrapperImpl::DequeueOutputBuffer");
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);
  DCHECK(!*codec_buffer);

  if (elided_eos_pending_) {
    elided_eos_pending_ = false;
    state_ = State::kDrained;
    if (end_of_stream) {
      *end_of_stream = true;
    }
    return DequeueStatus::kOk;
  }

  for (int attempt = 0; attempt < 3; ++attempt) {
    uint32_t index = 0;
    bool eos = false;
    auto status = codec_->DequeueOutputBuffer(presentation_time, index, eos);
    switch (status) {
      case DecoderAdapterCode::DECODER_OK: {
        if (eos) {
          state_ = State::kDrained;
          codec_->ReleaseOutputBuffer(index, false);
          if (end_of_stream) {
            *end_of_stream = true;
          }
          return DequeueStatus::kOk;
        }

        int64_t buffer_id = next_buffer_id_++;
        buffer_ids_[buffer_id] = index;

        DecoderFormat format;
        auto result = codec_->GetOutputFormatBridgeDecoder(format);
        DVLOG(3) << "CodecWrapperImpl::DequeueOutputBuffer "
                      "des width: "
                   << format.width << ", height: " << format.height;
        DVLOG(3) << "CodecWrapperImpl::DequeueOutputBuffer "
                      "src width: "
                   << codec_->GetConfigWidth()
                   << ", height: " << codec_->GetConfigHeight();
        if (result == DecoderAdapterCode::DECODER_OK) {
          size_ = gfx::Size(format.width, format.height);
        } else {
          LOG(ERROR) << "CodecWrapperImpl::GetOutputFormatBridgeDecoder "
                        "failed.";
          size_ =
              gfx::Size(codec_->GetConfigWidth(), codec_->GetConfigHeight());
        }

        *codec_buffer = base::WrapUnique(
            new CodecOutputBuffer(this, buffer_id, size_, color_space_));
        return DequeueStatus::kOk;
      }
      case DecoderAdapterCode::DECODER_RETRY: {
        return DequeueStatus::kTryAgainLater;
      }
      case DecoderAdapterCode::DECODER_ERROR: {
        state_ = State::kError;
        return DequeueStatus::kError;
      }
    }
  }

  state_ = State::kError;
  return DequeueStatus::kError;
}

bool CodecWrapperImpl::SetSurface(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  base::AutoLock l(lock_);
  DCHECK(surface_bundle);
  DCHECK(codec_ && state_ != State::kError);

  if (codec_->SetBridgeOutputSurface(surface_bundle->GetOHOSNativeWindow()) ==
      DecoderAdapterCode::DECODER_ERROR) {
    state_ = State::kError;
    return false;
  }
  surface_bundle_ = std::move(surface_bundle);
  return true;
}

scoped_refptr<CodecSurfaceBundle> CodecWrapperImpl::SurfaceBundle() {
  base::AutoLock l(lock_);
  return surface_bundle_;
}

bool CodecWrapperImpl::ReleaseCodecOutputBuffer(int64_t id, bool render) {
  if (!render && release_task_runner_ &&
      !release_task_runner_->RunsTasksInCurrentSequence()) {
    release_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&CodecWrapperImpl::ReleaseCodecOutputBuffer),
            this, id, render));
    return true;
  }

  base::AutoLock l(lock_);
  if (!codec_ || state_ == State::kError) {
    return false;
  }

  auto buffer_it = buffer_ids_.find(id);
  bool valid = buffer_it != buffer_ids_.end();
  DVLOG(3) << __func__ << " id=" << id << " render=" << render
           << " valid=" << valid;
  if (!valid) {
    return false;
  }

  int index = buffer_it->second;
  codec_->ReleaseOutputBuffer(index, render);
  buffer_ids_.erase(buffer_it);
  return true;
}

CodecWrapper::CodecWrapper(
    CodecSurfacePair codec_surface_pair,
    OutputReleasedCB output_buffer_release_cb,
    scoped_refptr<base::SequencedTaskRunner> release_task_runner)
    : impl_(new CodecWrapperImpl(std::move(codec_surface_pair),
                                 std::move(output_buffer_release_cb),
                                 std::move(release_task_runner))) {}

CodecWrapper::~CodecWrapper() {
  DCHECK(!impl_->TakeCodecSurfacePair().first);
}

CodecSurfacePair CodecWrapper::TakeCodecSurfacePair() {
  return impl_->TakeCodecSurfacePair();
}

bool CodecWrapper::HasUnreleasedOutputBuffers() const {
  return impl_->HasUnreleasedOutputBuffers();
}

void CodecWrapper::DiscardOutputBuffers() {
  impl_->DiscardOutputBuffers();
}

bool CodecWrapper::IsFlushed() const {
  return impl_->IsFlushed();
}

bool CodecWrapper::IsDraining() const {
  return impl_->IsDraining();
}

bool CodecWrapper::IsDrained() const {
  return impl_->IsDrained();
}

bool CodecWrapper::Flush() {
  return impl_->Flush();
}

CodecWrapper::QueueStatus CodecWrapper::QueueInputBuffer(
    const DecoderBuffer& buffer) {
  return impl_->QueueInputBuffer(buffer);
}

CodecWrapper::DequeueStatus CodecWrapper::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  return impl_->DequeueOutputBuffer(presentation_time, end_of_stream,
                                    codec_buffer);
}

bool CodecWrapper::SetSurface(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  return impl_->SetSurface(std::move(surface_bundle));
}

scoped_refptr<CodecSurfaceBundle> CodecWrapper::SurfaceBundle() {
  return impl_->SurfaceBundle();
}

}  // namespace media
