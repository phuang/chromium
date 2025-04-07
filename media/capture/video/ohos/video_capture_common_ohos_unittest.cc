// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/ohos/video_capture_common_ohos.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoCaptureCommonOhosTest : public ::testing::Test {
 public:
  VideoCaptureCommonOhosTest() {}
  ~VideoCaptureCommonOhosTest() override = default;
};

TEST_F(VideoCaptureCommonOhosTest, GetCameraTransportType) {
  EXPECT_EQ(VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN,
            VideoCaptureCommonOHOS::GetCameraTransportType(
                Camera_Connection::CAMERA_CONNECTION_BUILT_IN));
  EXPECT_EQ(VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN,
            VideoCaptureCommonOHOS::GetCameraTransportType(
                Camera_Connection::CAMERA_CONNECTION_USB_PLUGIN));
  EXPECT_EQ(VideoCaptureTransportType::OTHER_TRANSPORT,
            VideoCaptureCommonOHOS::GetCameraTransportType(
                Camera_Connection::CAMERA_CONNECTION_REMOTE));
}

TEST_F(VideoCaptureCommonOhosTest, GetCameraFacingMode) {
  EXPECT_EQ(MEDIA_VIDEO_FACING_NONE,
            VideoCaptureCommonOHOS::GetCameraFacingMode(
                Camera_Position::CAMERA_POSITION_UNSPECIFIED));
  EXPECT_EQ(MEDIA_VIDEO_FACING_ENVIRONMENT,
            VideoCaptureCommonOHOS::GetCameraFacingMode(
                Camera_Position::CAMERA_POSITION_BACK));
  EXPECT_EQ(MEDIA_VIDEO_FACING_USER,
            VideoCaptureCommonOHOS::GetCameraFacingMode(
                Camera_Position::CAMERA_POSITION_FRONT));
}

TEST_F(VideoCaptureCommonOhosTest, GetCameraPixelFormatType) {
  EXPECT_EQ(PIXEL_FORMAT_ABGR, VideoCaptureCommonOHOS::GetCameraPixelFormatType(
                                   Camera_Format::CAMERA_FORMAT_RGBA_8888));
  EXPECT_EQ(PIXEL_FORMAT_NV21, VideoCaptureCommonOHOS::GetCameraPixelFormatType(
                                   Camera_Format::CAMERA_FORMAT_YUV_420_SP));
  EXPECT_EQ(PIXEL_FORMAT_MJPEG,
            VideoCaptureCommonOHOS::GetCameraPixelFormatType(
                Camera_Format::CAMERA_FORMAT_JPEG));
}

TEST_F(VideoCaptureCommonOhosTest, GetAdapterCameraPixelFormatType) {
  EXPECT_EQ(Camera_Format::CAMERA_FORMAT_RGBA_8888,
            VideoCaptureCommonOHOS::GetAdapterCameraPixelFormatType(
                PIXEL_FORMAT_ABGR));
  EXPECT_EQ(Camera_Format::CAMERA_FORMAT_YUV_420_SP,
            VideoCaptureCommonOHOS::GetAdapterCameraPixelFormatType(
                PIXEL_FORMAT_NV21));
  EXPECT_EQ(Camera_Format::CAMERA_FORMAT_JPEG,
            VideoCaptureCommonOHOS::GetAdapterCameraPixelFormatType(
                PIXEL_FORMAT_MJPEG));
}

}  // namespace media
