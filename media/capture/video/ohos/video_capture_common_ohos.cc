// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/ohos/video_capture_common_ohos.h"

namespace media {
VideoCaptureTransportType VideoCaptureCommonOHOS::GetCameraTransportType(
    Camera_Connection trans_type) {
  auto item = kTransTypeMap.find(trans_type);
  if (item == kTransTypeMap.end()) {
    LOG(ERROR) << "concect type: " << static_cast<int>(trans_type)
               << " not found.";
    return VideoCaptureTransportType::OTHER_TRANSPORT;
  }
  return item->second;
}

VideoFacingMode VideoCaptureCommonOHOS::GetCameraFacingMode(
    Camera_Position facing_mode) {
  auto item = kFracingModeMap.find(facing_mode);
  if (item == kFracingModeMap.end()) {
    LOG(ERROR) << "facine mode: " << static_cast<int>(facing_mode)
               << " not found.";
    return MEDIA_VIDEO_FACING_NONE;
  }
  return item->second;
}

VideoPixelFormat VideoCaptureCommonOHOS::GetCameraPixelFormatType(
    Camera_Format pixel_format) {
  auto item = kPixelFormatMap.find(pixel_format);
  if (item == kPixelFormatMap.end()) {
    LOG(ERROR) << "camera pixel format: " << static_cast<int>(pixel_format)
               << " not found.";
    return PIXEL_FORMAT_UNKNOWN;
  }
  return item->second;
}

int VideoCaptureCommonOHOS::GetAdapterCameraPixelFormatType(
    VideoPixelFormat pixel_format) {
  auto item = kAdapterPixelFormatMap.find(pixel_format);
  if (item == kAdapterPixelFormatMap.end()) {
    LOG(ERROR) << "adapter camera pixel format: "
               << static_cast<int>(pixel_format) << " not found.";
    return -1;
  }
  return item->second;
}

VideoCaptureFormats VideoCaptureCommonOHOS::GetSupportedFormats(
    Camera_Profile** previewProfiles,
    uint32_t previewProfilesSize) {
  VideoCaptureFormats capture_formats;
  if (!previewProfiles) {
    return capture_formats;
  }
  for (uint32_t i = 0; i < previewProfilesSize; i++) {
    VideoCaptureFormat format;
    if (!previewProfiles[i]) {
      continue;
    }
    format.frame_size.SetSize(previewProfiles[i]->size.width,
                              previewProfiles[i]->size.height);
    format.frame_rate = kFrameRate;
    format.pixel_format = GetCameraPixelFormatType(previewProfiles[i]->format);
    if (format.pixel_format != PIXEL_FORMAT_UNKNOWN) {
      capture_formats.push_back(format);
    }
  }
  return capture_formats;
}

}  // namespace media
