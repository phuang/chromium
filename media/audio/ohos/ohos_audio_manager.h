// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OHOS_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_OHOS_AUDIO_MANAGER_H_

#include "media/audio/audio_manager_base.h"

namespace media {

// OHOS implementation of the AudioManager singleton.
class MEDIA_EXPORT OHOSAudioManager : public AudioManagerBase {
 public:
  OHOSAudioManager(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);
  ~OHOSAudioManager() override;

  OHOSAudioManager(const OHOSAudioManager&) = delete;
  OHOSAudioManager& operator=(const OHOSAudioManager&) = delete;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void ReleaseInputStream(AudioInputStream* stream) override;
  const char* GetName() override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  void ReleaseOutputStream(
      AudioOutputStream* stream) override;

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

  AudioParameters GetInputStreamParameters(
      const std::string& input_device_id) override;

  void SelectAudioDevice(const std::string& device_id, bool isInput);

};

}  // namespace media

#endif  // MEDIA_AUDIO_OHOS_AUDIO_MANAGER_H_
