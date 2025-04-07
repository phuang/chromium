// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_OHOS_VIDEO_CAPTURE_DEVICE_OHOS_H_
#define MEDIA_CAPTURE_VIDEO_OHOS_VIDEO_CAPTURE_DEVICE_OHOS_H_

#include <stdint.h>

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace base {
class WaitableEvent;
}

namespace media {

class OHOSCaptureDelegate;

// OHOS capture implementation of VideoCaptureDevice.
class VideoCaptureDeviceOHOS : public VideoCaptureDevice {
 public:

  VideoCaptureDeviceOHOS() = delete;

  explicit VideoCaptureDeviceOHOS(
      const VideoCaptureDeviceDescriptor& device_descriptor);

  VideoCaptureDeviceOHOS(const VideoCaptureDeviceOHOS&) = delete;
  VideoCaptureDeviceOHOS& operator=(const VideoCaptureDeviceOHOS&) = delete;

  ~VideoCaptureDeviceOHOS() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

 protected:
  virtual void SetRotation(int rotation);

  const VideoCaptureDeviceDescriptor device_descriptor_;

 private:
  void StopAndDeAllocateInternal(base::WaitableEvent* waiter);

  std::unique_ptr<OHOSCaptureDelegate> capture_impl_;

  // Thread used for reading data from the device.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // SetRotation() may get called even when the device is not started. When that
  // is the case we remember the value here and use it as soon as the device
  // gets started.
  int rotation_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_OHOS_VIDEO_CAPTURE_DEVICE_OHOS_H_
