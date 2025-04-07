// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_output_stream.h"

#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

OHOSAudioOutputStream::OHOSAudioOutputStream(OHOSAudioManager* manager,
                                             const AudioParameters& parameters)
    : manager_(manager),
      parameters_(parameters),
      ns_per_frame_(base::Time::kNanosecondsPerSecond /
                    static_cast<double>(parameters.sample_rate())),
      audio_bus_(AudioBus::Create(parameters)) {
  OH_AudioStream_Result ret = OH_AudioStreamBuilder_Create(
      &audio_stream_builder_, AUDIOSTREAM_TYPE_RENDERER);
  if (ret != AUDIOSTREAM_SUCCESS) {
    LOG(ERROR) << "AudioStreamBuilder create failed.";
  }

  sample_format_ = kSampleFormatS16;
  bytes_per_frame_ = parameters.GetBytesPerFrame(sample_format_);
  buffer_size_bytes_ = parameters.GetBytesPerBuffer(sample_format_);
}

OHOSAudioOutputStream::~OHOSAudioOutputStream() {
  // Close() must be called first.
  if (audio_renderer_ != nullptr) {
    OH_AudioRenderer_Release(audio_renderer_);
    audio_renderer_ = nullptr;
  }
  if (audio_stream_builder_ != nullptr) {
    OH_AudioStreamBuilder_Destroy(audio_stream_builder_);
    audio_stream_builder_ = nullptr;
  }
}

bool OHOSAudioOutputStream::Open() {
  if (!InitRender()) {
    return false;
  }
  return true;
}

void OHOSAudioOutputStream::Close() {
  Stop();
  manager_->ReleaseOutputStream(this);
}

static int32_t AudioRendererOnWriteData(OH_AudioRenderer* renderer,
                                        void* userData,
                                        void* buffer,
                                        int32_t length) {
  if (userData && buffer) {
    ((OHOSAudioOutputStream*)(userData))->PumpSamples(buffer, length);
  }
  return 0;
}

static int32_t AudioRendererOnError(OH_AudioRenderer* renderer,
                                    void* userData,
                                    OH_AudioStream_Result error) {
  if (userData) {
    ((OHOSAudioOutputStream*)(userData))->ReportError();
  }
  return 0;
}

static int32_t AudioRendererOnInterruptEvent(OH_AudioRenderer* renderer,
                                             void* userData,
                                             OH_AudioInterrupt_ForceType type,
                                             OH_AudioInterrupt_Hint hint) {
  LOG(ERROR) << "AudioRenderer on interrupt type:" << type << "hint:" << hint;
  return 0;
}

void OHOSAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(!callback_);
  DCHECK(reference_time_.is_null());
  callback_ = callback;
  if (!StartRender()) {
    LOG(ERROR) << "OHOSAudioOutputStream::StartRender failed";
  }
}

void OHOSAudioOutputStream::Stop() {
  if (!audio_renderer_) {
    return;
  }
  callback_ = nullptr;
  if (!reference_time_.is_null()) {
    reference_time_ = base::TimeTicks();
  }
  OH_AudioStream_Result ret = OH_AudioRenderer_Stop(audio_renderer_);
  if (ret != AUDIOSTREAM_SUCCESS) {
    ReportError();
  }
}

void OHOSAudioOutputStream::Refresh() {
  if (!audio_renderer_) {
    LOG(ERROR) << "OHOSAudioOutputStream::Refresh audio_renderer_ is null.";
    return;
  }
  OH_AudioRenderer_Stop(audio_renderer_);
  OH_AudioRenderer_Start(audio_renderer_);
}

void OHOSAudioOutputStream::SetInterruptMode(bool audioExclusive) {
  LOG(INFO) << "OHOSAudioOutputStream::SetInterruptMode audioExclusive: "
            << audioExclusive;
  if (!audio_renderer_) {
    LOG(ERROR)
        << "OHOSAudioOutputStream::SetInterruptMode audio_renderer_ is null.";
    return;
  }
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void OHOSAudioOutputStream::Flush() {}

void OHOSAudioOutputStream::SetVolume(double volume) {
  if (volume < 0.0 || volume > 1.0) {
    return;
  }
  volume_ = volume;
}

void OHOSAudioOutputStream::GetVolume(double* volume) {
  *volume = volume_;
}

bool OHOSAudioOutputStream::InitRender() {
  // set params
  OH_AudioStreamBuilder_SetSamplingRate(audio_stream_builder_,
                                        parameters_.sample_rate());
  OH_AudioStreamBuilder_SetChannelCount(audio_stream_builder_,
                                        parameters_.channels());
  OH_AudioStreamBuilder_SetLatencyMode(audio_stream_builder_,
                                       AUDIOSTREAM_LATENCY_MODE_NORMAL);
  OH_AudioStreamBuilder_SetFrameSizeInCallback(audio_stream_builder_,
                                               parameters_.frames_per_buffer());
  // set callback
  OH_AudioRenderer_Callbacks callbacks;
  callbacks.OH_AudioRenderer_OnWriteData = AudioRendererOnWriteData;
  callbacks.OH_AudioRenderer_OnError = AudioRendererOnError;
  callbacks.OH_AudioRenderer_OnInterruptEvent = AudioRendererOnInterruptEvent;
  OH_AudioStreamBuilder_SetRendererCallback(audio_stream_builder_, callbacks,
                                            this);
  OH_AudioStream_Result ret;
  // create audio render
  ret = OH_AudioStreamBuilder_GenerateRenderer(audio_stream_builder_,
                                               &audio_renderer_);
  if (ret != AUDIOSTREAM_SUCCESS) {
    LOG(ERROR) << "AudioStreamBuilder GenerateRenderer failed.";
    return false;
  }
  return true;
}

bool OHOSAudioOutputStream::StartRender() {
  OH_AudioStream_Result ret = OH_AudioRenderer_Start(audio_renderer_);
  if (ret != AUDIOSTREAM_SUCCESS) {
    if (!OH_AudioRenderer_Release(audio_renderer_)) {
      LOG(ERROR) << "ohos audio render release failed";
    }
    ReportError();
    return false;
  }
  return true;
}

void OHOSAudioOutputStream::ReportError() {
  LOG(ERROR) << "ohos audio render error happened";
  reference_time_ = base::TimeTicks();
  if (callback_) {
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }
}

base::TimeDelta OHOSAudioOutputStream::GetDelay(
    base::TimeTicks delay_timestamp) {
  // Get the time that a known audio frame was presented for playing.
  int64_t existing_frame_index;
  int64_t existing_frame_pts;
  OH_AudioStream_Result result =
      OH_AudioRenderer_GetTimestamp(audio_renderer_, CLOCK_MONOTONIC,
                                    &existing_frame_index, &existing_frame_pts);
  if (result != OH_AudioStream_Result::AUDIOSTREAM_SUCCESS) {
    LOG(ERROR) << "Failed to get audio latency, result: " << result;
    return base::TimeDelta();
  }

  // Calculate the number of frames between our known frame and the write index.
  int64_t frames;
  result = OH_AudioRenderer_GetFramesWritten(audio_renderer_, &frames);
  if (result != OH_AudioStream_Result::AUDIOSTREAM_SUCCESS) {
    LOG(ERROR) << "Failed to OH_AudioRenderer_GetFramesWritten, result: "
               << result;
    return base::TimeDelta();
  }
  const int64_t frame_index_delta = frames - existing_frame_index;

  // Calculate the time which the next frame will be presented.
  const base::TimeDelta next_frame_pts =
      base::Nanoseconds(existing_frame_pts + frame_index_delta * ns_per_frame_);

  // Calculate the latency between write time and presentation time. At startup
  // we may end up with negative values here.
  return std::max(base::TimeDelta(),
                  next_frame_pts - (delay_timestamp - base::TimeTicks()));
}

void OHOSAudioOutputStream::PumpSamples(void* buffer, int32_t length) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay = GetDelay(now);
  if (!callback_) {
    LOG(INFO) << "PumpSamples failed, callback_ is nullptr";
    return;
  }
  // Request more samples from |callback_|.
  int frames_filled =
      callback_->OnMoreData(delay, now, {}, audio_bus_.get(), false);
  DCHECK_EQ(frames_filled, audio_bus_->frames());
  audio_bus_->Scale(volume_);
  audio_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
      frames_filled, reinterpret_cast<int16_t*>(buffer));
  if (reference_time_.is_null()) {
    reference_time_ = now;
  }
}

}  // namespace media
