// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_capturer_source.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_glitch_info.h"

namespace media {

AudioCapturerReadCallback::AudioCapturerReadCallback(
    const OnReadDataCallback& readDataCallback)
    : readDataCallback_(readDataCallback) {}

AudioCapturerReadCallback::~AudioCapturerReadCallback() {}

void AudioCapturerReadCallback::OnReadData(size_t length) {
  if (!readDataCallback_.is_null()) {
    readDataCallback_.Run();
  }
}

OHOSAudioCapturerSource::OHOSAudioCapturerSource(
    scoped_refptr<base::SingleThreadTaskRunner> capturer_task_runner)
    : capturer_task_runner_(capturer_task_runner) {}

OHOSAudioCapturerSource::~OHOSAudioCapturerSource() {}

void OHOSAudioCapturerSource::Initialize(
    const AudioParameters& params,
    AudioCapturerSource::CaptureCallback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  params_ = params;
  callback_ = callback;

  if (params_.format() != AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    ReportError("Only AUDIO_PCM_LOW_LATENCY format is supported");
    return;
  }
  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSAudioCapturerSource::InitializeOnCapturerThread,
                     this));
}

void OHOSAudioCapturerSource::Start() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_);
  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSAudioCapturerSource::StartOnCapturerThread, this));
}

void OHOSAudioCapturerSource::Stop() {
  if (!main_task_runner_) {
    return;
  }
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock lock(callback_lock_);
    if (callback_) {
      callback_ = nullptr;
    }
  }

  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSAudioCapturerSource::StopOnCapturerThread, this));
}

int32_t AudioCapturerOnReadData(OH_AudioCapturer* capturer,
                                void* userData,
                                void* buffer,
                                int32_t bufferLen) {
  if (userData && buffer) {
    ((OHOSAudioCapturerSource*)(userData))->ReadData(buffer, bufferLen);
  }
  return 0;
}

void OHOSAudioCapturerSource::InitializeOnCapturerThread() {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());
  // create builder
  OH_AudioStreamBuilder_Create(&audio_stream_builder_,
                               AUDIOSTREAM_TYPE_CAPTURER);
  // set params
  OH_AudioStreamBuilder_SetSamplingRate(audio_stream_builder_,
                                        params_.sample_rate());
  OH_AudioStreamBuilder_SetChannelCount(audio_stream_builder_,
                                        params_.channels());
  // set callbacks
  OH_AudioCapturer_Callbacks callbacks;
  callbacks.OH_AudioCapturer_OnReadData = AudioCapturerOnReadData;
  OH_AudioStreamBuilder_SetCapturerCallback(audio_stream_builder_, callbacks,
                                            this);
  // create audio capturer
  OH_AudioStream_Result ret =
      OH_AudioStreamBuilder_GenerateCapturer(
          audio_stream_builder_, &audio_capturer_);
  if (ret != AUDIOSTREAM_SUCCESS) {
    ReportError("Generate OHOS audio capturer failed");
  }
}

void OHOSAudioCapturerSource::StartOnCapturerThread() {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());
  if (OH_AudioCapturer_Start(audio_capturer_) != AUDIOSTREAM_SUCCESS) {
    ReportError("Start OHOS audio capturer failed");
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSAudioCapturerSource::NotifyCaptureStarted, this));
}

void OHOSAudioCapturerSource::StopOnCapturerThread() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (OH_AudioCapturer_Stop(audio_capturer_) != AUDIOSTREAM_SUCCESS) {
    ReportError("Stop OHOS audio capturer failed");
  }
  OH_AudioCapturer_Release(audio_capturer_);
  OH_AudioStreamBuilder_Destroy(audio_stream_builder_);
}

void OHOSAudioCapturerSource::ReadData(void* buffer, int32_t length) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSAudioCapturerSource::ReadDataOnCapturerThread, this,
                     buffer, length));
}

void OHOSAudioCapturerSource::ReadDataOnCapturerThread(void* buffer,
                                                       int32_t length) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(callback_lock_);
  uint32_t frameCount = length / params_.GetBytesPerFrame(kSampleFormatS16);
  auto audio_bus = AudioBus::Create(params_.channels(), frameCount);
  int frame_count_threshold = 2 * params_.sample_rate() / 100;
  if (static_cast<int>(frameCount) > frame_count_threshold) {
    LOG(ERROR) << "audioBus cannot handle input audio data more than 20ms. "
                  "frameCount: "
               << static_cast<int>(frameCount);
    return;
  }
  audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(
      reinterpret_cast<const int16_t*>(buffer), static_cast<int>(frameCount));
  if (callback_) {
    const AudioInputBuffer* inputBuffer =
        reinterpret_cast<const AudioInputBuffer*>(buffer);
    AudioGlitchInfo glitch_info{
        .duration = base::Microseconds(inputBuffer->params.glitch_duration_us),
        .count = inputBuffer->params.glitch_count};

    callback_->Capture(audio_bus.get(), base::TimeTicks::Now(), glitch_info, 1.0);
  }
}

void OHOSAudioCapturerSource::SetVolume(double volume) {
  NOTREACHED();
}

void OHOSAudioCapturerSource::SetAutomaticGainControl(bool enable) {
  NOTREACHED();
}

void OHOSAudioCapturerSource::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  NOTREACHED();
}

void OHOSAudioCapturerSource::NotifyCaptureError(const std::string& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!callback_) {
    return;
  }
  callback_->OnCaptureError(AudioCapturerSource::ErrorCode::kUnknown, message);
}

void OHOSAudioCapturerSource::NotifyCaptureStarted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (callback_) {
    callback_->OnCaptureStarted();
  }
}

void OHOSAudioCapturerSource::ReportError(const std::string& message) {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OHOSAudioCapturerSource::NotifyCaptureError,
                                this, message));
}
}  // namespace media
