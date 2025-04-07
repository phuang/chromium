// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OHOS_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_OHOS_AUDIO_INPUT_STREAM_H_

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/ohos/ohos_audio_capturer_source.h"
#include "media/audio/ohos/ohos_audio_manager.h"
#include "media/base/audio_parameters.h"

namespace media {

class OHOSAudioManager;

class OHOSAudioInputStream : public AudioInputStream {
 public:
  // Caller must ensure that manager outlives the stream.
  OHOSAudioInputStream(OHOSAudioManager* manager,
                       const AudioParameters& parameters);

  OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  class CaptureCallbackAdapter;

  ~OHOSAudioInputStream() override;

  OHOSAudioManager* const manager_;

  AudioParameters parameters_;

  std::unique_ptr<CaptureCallbackAdapter> callback_adapter_;

  scoped_refptr<OHOSAudioCapturerSource> capturer_source_;

  double volume_ = 1.0;

  bool automatic_gain_control_ = false;
};

}  // namespace media

#endif  // MEDIA_AUDIO_OHOS_AUDIO_INPUT_STREAM_H_
