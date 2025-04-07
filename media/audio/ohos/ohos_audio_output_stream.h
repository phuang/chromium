// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OHOS_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_OHOS_AUDIO_OUTPUT_STREAM_H_

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/timer/timer.h"
#include "media/audio/ohos/ohos_audio_manager.h"
#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostreambuilder.h"

namespace media {

class OHOSAudioManager;

class OHOSAudioOutputStream : public AudioOutputStream {
 public:
  OHOSAudioOutputStream(const OHOSAudioOutputStream&) = delete;
  OHOSAudioOutputStream& operator=(const OHOSAudioOutputStream&) = delete;

  // Caller must ensure that manager outlives the stream.
  OHOSAudioOutputStream(OHOSAudioManager* manager,
                        const AudioParameters& parameters);

  // AudioOutputStream interface.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;
  void SetInterruptMode(bool audioExclusive);
  void Refresh();

  // Requests data from AudioSourceCallback
  void PumpSamples(void* buffer, int32_t length);

  // Resets internal state and reports an error to |callback_|.
  void ReportError();

 private:
  ~OHOSAudioOutputStream() override;

  base::TimeDelta GetDelay(base::TimeTicks delay_timestamp);

  bool InitRender();

  bool StartRender();

  OHOSAudioManager* manager_;

  AudioParameters parameters_;

  // Constant used for calculating latency. Amount of nanoseconds per frame.
  const double ns_per_frame_;

  // |audio_bus_| is used only in PumpSamples(). It is kept here to avoid
  // reallocating the memory every time.
  std::unique_ptr<AudioBus> audio_bus_;

  AudioSourceCallback* callback_ = nullptr;

  double volume_ = 1.0;

  base::TimeTicks reference_time_;

  int bytes_per_frame_;

  size_t buffer_size_bytes_;

  SampleFormat sample_format_;

  OH_AudioRenderer* audio_renderer_ = nullptr;

  OH_AudioStreamBuilder* audio_stream_builder_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_OHOS_AUDIO_OUTPUT_STREAM_H_
