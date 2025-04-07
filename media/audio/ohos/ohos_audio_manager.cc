// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_manager.h"

#include <stdlib.h>
#include "base/command_line.h"
#include "base/logging.h"
#include "media/audio/ohos/ohos_audio_input_stream.h"
#include "media/audio/ohos/ohos_audio_output_stream.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"

namespace media {

constexpr int kDefaultSampleRate = 48000;
constexpr int kMinimumOutputBufferSize = 2048;
constexpr int kMinimumInputBufferSize = 960;
const char AUDIO_DEFAULT_DEVICE_ID[] = "default";
const char AUDIO_DEFAULT_DEVICE_NAME[] = "(default)";
static const char AUDIO_MANAGER_NAME[] = "OHOS";

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<OHOSAudioManager>(std::move(audio_thread),
                                            audio_log_factory);
}

OHOSAudioManager::OHOSAudioManager(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

OHOSAudioManager::~OHOSAudioManager() = default;

// Implementation of AudioManager.
bool OHOSAudioManager::HasAudioOutputDevices() {
  return true;
}

bool OHOSAudioManager::HasAudioInputDevices() {
  return true;
}

void OHOSAudioManager::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  AudioDeviceName device_name;
  device_name.unique_id = std::string(AUDIO_DEFAULT_DEVICE_ID);
  device_name.device_name = std::string(AUDIO_DEFAULT_DEVICE_NAME);
  device_names->push_front(device_name);
}

void OHOSAudioManager::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  AudioDeviceName device_name;
  device_name.unique_id = std::string(AUDIO_DEFAULT_DEVICE_ID);
  device_name.device_name = std::string(AUDIO_DEFAULT_DEVICE_NAME);
  device_names->push_front(device_name);
}

const char* OHOSAudioManager::GetName() {
  return AUDIO_MANAGER_NAME;
}

// Implementation of AudioManagerBase.
AudioOutputStream* OHOSAudioManager::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* OHOSAudioManager::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  SelectAudioDevice(device_id, false);
  return new OHOSAudioOutputStream(this, params);
}

AudioInputStream* OHOSAudioManager::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* OHOSAudioManager::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  SelectAudioDevice(device_id, true);
  return new OHOSAudioInputStream(this, params);
}

AudioParameters OHOSAudioManager::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  SelectAudioDevice(output_device_id, false);
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_STEREO>(),
      kDefaultSampleRate, kMinimumOutputBufferSize);
}

AudioParameters OHOSAudioManager::GetInputStreamParameters(
    const std::string& input_device_id) {
  AudioParameters params =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_STEREO>(),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  params.set_effects(AudioParameters::ECHO_CANCELLER |
                     AudioParameters::NOISE_SUPPRESSION |
                     AudioParameters::AUTOMATIC_GAIN_CONTROL);
  return params;
}

void OHOSAudioManager::ReleaseInputStream(AudioInputStream* stream) {
  AudioManagerBase::ReleaseInputStream(stream);
}

void OHOSAudioManager::ReleaseOutputStream(AudioOutputStream* stream) {
  AudioManagerBase::ReleaseOutputStream(stream);
  stream = nullptr;
}

void OHOSAudioManager::SelectAudioDevice(const std::string& device_id,
                                         bool isInput) {}

}  // namespace media
