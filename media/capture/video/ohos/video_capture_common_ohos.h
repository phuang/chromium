// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_COMMON_OHOS_H_
#define MEDIA_CAPTURE_COMMON_OHOS_H_

#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"

#include "ohcamera/camera.h"

namespace media {

const std::unordered_map<Camera_Connection, VideoCaptureTransportType>
  kTransTypeMap = {
    {Camera_Connection::CAMERA_CONNECTION_BUILT_IN,
        VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN},
    {Camera_Connection::CAMERA_CONNECTION_USB_PLUGIN,
        VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN},
    {Camera_Connection::CAMERA_CONNECTION_REMOTE,
        VideoCaptureTransportType::OTHER_TRANSPORT},
};

const std::unordered_map<Camera_Position, VideoFacingMode>
  kFracingModeMap = {
    {Camera_Position::CAMERA_POSITION_UNSPECIFIED, MEDIA_VIDEO_FACING_NONE},
    {Camera_Position::CAMERA_POSITION_BACK, MEDIA_VIDEO_FACING_ENVIRONMENT},
    {Camera_Position::CAMERA_POSITION_FRONT, MEDIA_VIDEO_FACING_USER},
};

const std::unordered_map<Camera_Format, VideoPixelFormat>
  kPixelFormatMap = {
    {Camera_Format::CAMERA_FORMAT_RGBA_8888, PIXEL_FORMAT_ABGR},
    {Camera_Format::CAMERA_FORMAT_YUV_420_SP, PIXEL_FORMAT_NV21},
    {Camera_Format::CAMERA_FORMAT_JPEG, PIXEL_FORMAT_MJPEG},
};

const std::unordered_map<VideoPixelFormat, Camera_Format>
  kAdapterPixelFormatMap = {
    {PIXEL_FORMAT_ABGR, Camera_Format::CAMERA_FORMAT_RGBA_8888},
    {PIXEL_FORMAT_NV21, Camera_Format::CAMERA_FORMAT_YUV_420_SP},
    {PIXEL_FORMAT_MJPEG, Camera_Format::CAMERA_FORMAT_JPEG},
};

class VideoCaptureCommonOHOS {
 public:
  VideoCaptureCommonOHOS();
  ~VideoCaptureCommonOHOS();

  static const int32_t kFrameRate = 30;

  static VideoCaptureTransportType GetCameraTransportType(
      Camera_Connection trans_type);
  static VideoFacingMode GetCameraFacingMode(Camera_Position facing_mode);
  static VideoPixelFormat GetCameraPixelFormatType(Camera_Format pixel_format);
  static VideoCaptureFormats GetSupportedFormats(
      Camera_Profile** previewProfiles,
      uint32_t previewProfilesSize);
  static int GetAdapterCameraPixelFormatType(
      VideoPixelFormat pixel_format);
};

}  // namespace media
#endif  // MEDIA_CAPTURE_COMMON_OHOS_H_
