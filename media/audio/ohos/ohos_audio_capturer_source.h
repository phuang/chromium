// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_OHOS_AUDIO_OHOS_AUDIO_CAPTURER_SOURCE_H_
#define MEDIA_OHOS_AUDIO_OHOS_AUDIO_CAPTURER_SOURCE_H_

#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/media_export.h"
#include "ohaudio/native_audiocapturer.h"
#include "ohaudio/native_audiostreambuilder.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

using OnReadDataCallback = base::RepeatingCallback<void(void)>;

constexpr int kMaxNumOfBuffer = 2;

class AudioCapturerReadCallback {
 public:
  AudioCapturerReadCallback(const OnReadDataCallback& readDataCallback);

  ~AudioCapturerReadCallback();

  void OnReadData(size_t length);

 private:
  OnReadDataCallback readDataCallback_;
};

class MEDIA_EXPORT OHOSAudioCapturerSource final : public AudioCapturerSource {
 public:
  OHOSAudioCapturerSource(
      scoped_refptr<base::SingleThreadTaskRunner> capturer_task_runner);
  OHOSAudioCapturerSource(const OHOSAudioCapturerSource&) = delete;
  OHOSAudioCapturerSource& operator=(const OHOSAudioCapturerSource&) = delete;

  // AudioCaptureSource implementation.
  void Initialize(const AudioParameters& params,
                  CaptureCallback* callback) override;
  void Start() override;
  void Stop() override;
  void SetVolume(double volume) override;
  void SetAutomaticGainControl(bool enable) override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;
  void ReadData(void* buffer, int32_t length);

 private:
  ~OHOSAudioCapturerSource() override;

  // Called in Initialize(), create a OHOS audio capturer.
  void InitializeOnCapturerThread();

  // Called in Start(), start OHOS audio capturer.
  void StartOnCapturerThread();

  // Called in Stop(); stop OHOS audio capturer.
  void StopOnCapturerThread();

  // Called in ReadData() when the OH_AudioCapturer_OnReadData callback is
  // triggered. |buffer| is the data obtained from the microphone, and |length|
  // is the buffer length.
  void ReadDataOnCapturerThread(void* buffer, int32_t length);

  void NotifyCaptureError(const std::string& error);

  void NotifyCaptureStarted();

  // reports an error to |callback_|.
  void ReportError(const std::string& message);

  OH_AudioCapturer* audio_capturer_ = nullptr;

  OH_AudioStreamBuilder* audio_stream_builder_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> capturer_task_runner_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  AudioParameters params_;

  CaptureCallback* callback_ = nullptr;

  std::shared_ptr<AudioCapturerReadCallback> audioCapturerReadCallback_ =
      nullptr;

  base::Lock callback_lock_;
};

}  // namespace media

#endif  // MEDIA_OHOS_AUDIO_OHOS_AUDIO_CAPTURER_SOURCE_H_
