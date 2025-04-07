// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/ohos/video_capture_device_factory_ohos.h"

#include "media/capture/video/ohos/video_capture_common_ohos.h"
#include "media/capture/video/ohos/video_capture_device_ohos.h"

namespace media {

namespace {

bool CompareCaptureDevices(const VideoCaptureDeviceInfo& a,
                           const VideoCaptureDeviceInfo& b) {
  return a.descriptor < b.descriptor;
}
}  // namespace

VideoCaptureDeviceFactoryOHOS::VideoCaptureDeviceFactoryOHOS() {
  Camera_ErrorCode ret = OH_Camera_GetCameraManager(&camera_manager_);
  if (!camera_manager_ || ret != CAMERA_OK) {
    LOG(ERROR) << "GetCameraManager failed, ErrorCode:" << ret;
  }
}

VideoCaptureDeviceFactoryOHOS::~VideoCaptureDeviceFactoryOHOS() {
  Camera_ErrorCode ret = OH_Camera_DeleteCameraManager(camera_manager_);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "DeleteCameraManager failed, ErrorCode:" << ret;
  }
  camera_manager_ = nullptr;
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryOHOS::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  LOG(INFO) << "VideoCaptureDeviceFactoryOHOS::CreateDevice id:"
            << device_descriptor.device_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  auto self = std::make_unique<VideoCaptureDeviceOHOS>(device_descriptor);
  return VideoCaptureErrorOrDevice(std::move(self));
}

void VideoCaptureDeviceFactoryOHOS::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(camera_manager_);
  std::vector<VideoCaptureDeviceInfo> devices_info;
  uint32_t size = 0;
  Camera_ErrorCode ret;

  ret = OH_CameraManager_GetSupportedCameras(camera_manager_, &cameras_, &size);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "GetSupportedCameras failed, ErrorCode:" << ret;
  }

  for (uint32_t i = 0; i < size; i++) {
    ret = OH_CameraManager_GetSupportedCameraOutputCapability(
        camera_manager_, &cameras_[i], &camera_output_capability_);
    if (ret != CAMERA_OK) {
      LOG(ERROR) << "GetSupportedCameraOutputCapability failed,"
                 << "CameraIndex:" << i << " ErrorCode:" << ret;
      continue;
    }
    VideoCaptureControlSupport control_support;
    control_support.pan = false;
    control_support.tilt = false;
    control_support.zoom = false;
    VideoFacingMode facing_mode =
        VideoCaptureCommonOHOS::GetCameraFacingMode(cameras_[i].cameraPosition);
    VideoCaptureDeviceInfo device_info(VideoCaptureDeviceDescriptor(
        cameras_[i].cameraId, cameras_[i].cameraId, "" /*model_id*/,
        VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE, control_support,
        VideoCaptureCommonOHOS::GetCameraTransportType(
            cameras_[i].connectionType),
        facing_mode));
    device_info.supported_formats = VideoCaptureCommonOHOS::GetSupportedFormats(
        camera_output_capability_->previewProfiles,
        camera_output_capability_->previewProfilesSize);
    if (facing_mode == MEDIA_VIDEO_FACING_USER) {
      devices_info.insert(devices_info.begin(), std::move(device_info));
    } else {
      devices_info.emplace_back(std::move(device_info));
    }
  }
  std::sort(devices_info.begin(), devices_info.end(), CompareCaptureDevices);

  std::move(callback).Run(std::move(devices_info));

  ret = OH_CameraManager_DeleteSupportedCameras(camera_manager_,
                                                cameras_,
                                                size);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "DeleteSupportedCameras failed, ErrorCode:" << ret;
  }
  cameras_ = nullptr;

  ret = OH_CameraManager_DeleteSupportedCameraOutputCapability(
      camera_manager_, camera_output_capability_);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "DeleteSupportedCameraOutputCapability failed, ErrorCode:" << ret;
  }
  camera_output_capability_ = nullptr;
}

}  // namespace media
