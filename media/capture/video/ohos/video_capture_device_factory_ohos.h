// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OHOS implementation of the VideoCaptureDeviceFactory class

#ifndef MEDIA_CAPTURE_VIDEO_OHOS_VIDEO_CAPTURE_DEVICE_FACTORY_OHOS_H_
#define MEDIA_CAPTURE_VIDEO_OHOS_VIDEO_CAPTURE_DEVICE_FACTORY_OHOS_H_

#include "media/capture/video/video_capture_device_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "media/capture/video_capture_types.h"
#include "ohcamera/camera.h"
#include "ohcamera/camera_manager.h"

namespace media {
// Extension of VideoCaptureDeviceFactory to create and manipulate OHOS
// devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryOHOS
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryOHOS();

  VideoCaptureDeviceFactoryOHOS(const VideoCaptureDeviceFactoryOHOS&) = delete;
  VideoCaptureDeviceFactoryOHOS& operator=(
      const VideoCaptureDeviceFactoryOHOS&) = delete;

  ~VideoCaptureDeviceFactoryOHOS() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

 private:
  Camera_Manager* camera_manager_ = nullptr;
  Camera_OutputCapability* camera_output_capability_ = nullptr;
  Camera_Device* cameras_ = nullptr;
};

}  // namespace media
#endif  // MEDIA_CAPTURE_VIDEO_OHOS_VIDEO_CAPTURE_DEVICE_FACTORY_OHOS_H_
