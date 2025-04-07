// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/ohos/ohos_capture_delegate.h"

#include <cstddef>

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/ohos/ohos_capture_delegate.h"
#include "third_party/libyuv/include/libyuv.h"
#include "video_capture_common_ohos.h"

#include "ohos/adapter/media_manager/media_adapter.h"

namespace media {

OHOSCaptureDelegate::OHOSCaptureDelegate(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    const scoped_refptr<base::SingleThreadTaskRunner>& capture_stask_runner,
    const VideoCaptureParams capture_params)
    : capture_task_runner_(capture_stask_runner),
      device_descriptor_(device_descriptor),
      is_capturing_(false),
      capture_params_(capture_params) {}

void OHOSCaptureDelegate::AllocateAndStart(
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(client);

  client_ = std::move(client);
  OH_Camera_GetCameraManager(&camera_manager_);
  if (!StartStream()) {
    LOG(ERROR) << "start stream failed";
    return;
  }
  auto& media_adapter_instance = ohos::adapter::MediaAdapter::GetInstance();
  media_adapter_instance.RegisterBufferAvailableCallback(
      std::bind(&OHOSCaptureDelegate::OnBufferAvailable, this,
                std::placeholders::_1, std::placeholders::_2));
  client_->OnStarted();
}

void OHOSCaptureDelegate::StopAndDeAllocate() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  StopStream();
  // At this point we can close the device.
  // This is also needed for correctly changing settings later via VIDIOC_S_FMT.
  client_.reset();
  OH_Camera_DeleteCameraManager(camera_manager_);
  camera_manager_ = nullptr;
}

void OHOSCaptureDelegate::TakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  take_photo_callbacks_.push(std::move(callback));
}

const std::unordered_map<Camera_ExposureMode, MeteringMode> EXP_MODE_MAP = {
    {Camera_ExposureMode::EXPOSURE_MODE_LOCKED, MeteringMode::SINGLE_SHOT},
    {Camera_ExposureMode::EXPOSURE_MODE_AUTO, MeteringMode::CONTINUOUS},
    {Camera_ExposureMode::EXPOSURE_MODE_CONTINUOUS_AUTO,
     MeteringMode::CONTINUOUS},
};

int OHOSCaptureDelegate::GetUsableExposureMode(
    Camera_ExposureMode& camera_exposure_mode,
    MeteringMode& exposure_mode) {
  auto item = EXP_MODE_MAP.find(camera_exposure_mode);
  if (item == EXP_MODE_MAP.end()) {
    LOG(ERROR) << "concect type: " << static_cast<int>(camera_exposure_mode)
               << " not found.";
    exposure_mode = MeteringMode::NONE;
    return kErrorReturnValue;
  }
  exposure_mode = item->second;
  return kSuccessReturnValue;
}

MeteringMode OHOSCaptureDelegate::GetCurrentExposureMode(
    Camera_ExposureMode& camera_exposure_mode) {
  if (static_cast<int>(camera_exposure_mode) == -1) {
    return MeteringMode::kMaxValue;
  }
  auto item = EXP_MODE_MAP.find(camera_exposure_mode);
  if (item == EXP_MODE_MAP.end()) {
    LOG(ERROR) << "concect type: " << static_cast<int>(camera_exposure_mode)
               << " not found.";
    return MeteringMode::NONE;
  }
  return item->second;
}

mojom::RangePtr OHOSCaptureDelegate::RetrieveUserControlRange() {
  float max;
  float min;
  float curr;
  float step;
  mojom::RangePtr capability = mojom::Range::New();
  if (capture_session_ == nullptr) {
    LOG(ERROR) << "captureSession is nullptr when RetrieveUserControlRange";
    return capability;
  }

  if (OH_CaptureSession_GetExposureBiasRange(capture_session_, &min, &max,
                                             &step) != CAMERA_OK) {
    LOG(ERROR) << "get exposure bias range failed";
    return capability;
  }

  if (OH_CaptureSession_GetExposureBias(capture_session_, &curr) != CAMERA_OK) {
    LOG(ERROR) << "get current exposure bias failed";
    return capability;
  }
  capability->max = max;
  capability->min = min;
  capability->step = step;
  capability->current = curr;
  return capability;
}

void OHOSCaptureDelegate::GetExposureState(
    mojom::PhotoStatePtr& photo_capabilities) {
  std::vector<Camera_ExposureMode> camera_exposure_modes;
  for (int i = 0; i <= static_cast<int>(
                           Camera_ExposureMode::EXPOSURE_MODE_CONTINUOUS_AUTO);
       i++) {
    bool is_supported = false;
    OH_CaptureSession_IsExposureModeSupported(
        capture_session_, static_cast<Camera_ExposureMode>(i), &is_supported);
    if (is_supported) {
      camera_exposure_modes.emplace_back(static_cast<Camera_ExposureMode>(i));
    }
  }

  for (auto camera_exposure_mode : camera_exposure_modes) {
    MeteringMode exposure_mode;
    if (GetUsableExposureMode(camera_exposure_mode, exposure_mode) ==
        kSuccessReturnValue) {
      photo_capabilities->supported_exposure_modes.push_back(exposure_mode);
    }
  }
  Camera_ExposureMode curr_camera_exposure_mode;
  if (OH_CaptureSession_GetExposureMode(capture_session_,
                                        &curr_camera_exposure_mode) !=
      Camera_ErrorCode::CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_GetExposureMode failed";
    return;
  }
  photo_capabilities->current_exposure_mode =
      GetCurrentExposureMode(curr_camera_exposure_mode);
  photo_capabilities->exposure_compensation = RetrieveUserControlRange();
}

void OHOSCaptureDelegate::GetFocusState(
    mojom::PhotoStatePtr& photo_capabilities) {
  bool is_supported = false;
  Camera_ErrorCode ret = OH_CaptureSession_IsFocusModeSupported(
      capture_session_, Camera_FocusMode::FOCUS_MODE_MANUAL, &is_supported);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_IsFocusModeSupported failed, ErrorCode:"
               << ret;
    return;
  }
  if (is_supported) {
    photo_capabilities->supported_focus_modes.push_back(MeteringMode::MANUAL);
  }

  ret = OH_CaptureSession_IsFocusModeSupported(
      capture_session_, Camera_FocusMode::FOCUS_MODE_CONTINUOUS_AUTO,
      &is_supported);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_IsFocusModeSupported failed, ErrorCode:"
               << ret;
    return;
  }
  if (is_supported) {
    photo_capabilities->supported_focus_modes.push_back(
        MeteringMode::CONTINUOUS);
  }

  Camera_FocusMode focus_mode;
  ret = OH_CaptureSession_GetFocusMode(capture_session_, &focus_mode);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_GetFocusMode failed, ErrorCode:" << ret;
    return;
  }
  if (focus_mode == Camera_FocusMode::FOCUS_MODE_MANUAL) {
    photo_capabilities->current_focus_mode = MeteringMode::MANUAL;
  } else if ((focus_mode == Camera_FocusMode::FOCUS_MODE_CONTINUOUS_AUTO) ||
             (focus_mode == Camera_FocusMode::FOCUS_MODE_AUTO)) {
    photo_capabilities->current_focus_mode = MeteringMode::CONTINUOUS;
  }
}

void OHOSCaptureDelegate::GetFlashState(
    mojom::PhotoStatePtr& photo_capabilities) {
  bool is_supported = false;
  Camera_ErrorCode ret = OH_CaptureSession_IsFlashModeSupported(
      capture_session_, Camera_FlashMode::FLASH_MODE_CLOSE, &is_supported);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_IsFlashModeSupported failed, ErrorCode:"
               << ret;
    return;
  }
  if (is_supported) {
    photo_capabilities->fill_light_mode.push_back(mojom::FillLightMode::OFF);
  }

  ret = OH_CaptureSession_IsFlashModeSupported(
      capture_session_, Camera_FlashMode::FLASH_MODE_OPEN, &is_supported);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_IsFlashModeSupported failed, ErrorCode:"
               << ret;
    return;
  }
  if (is_supported) {
    photo_capabilities->fill_light_mode.push_back(mojom::FillLightMode::FLASH);
  }

  ret = OH_CaptureSession_IsFlashModeSupported(
      capture_session_, Camera_FlashMode::FLASH_MODE_AUTO, &is_supported);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_IsFlashModeSupported failed, ErrorCode:"
               << ret;
    return;
  }
  if (is_supported) {
    photo_capabilities->fill_light_mode.push_back(mojom::FillLightMode::AUTO);
  }
}

void OHOSCaptureDelegate::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_) {
    return;
  }

  mojom::PhotoStatePtr photo_capabilities = mojo::CreateEmptyPhotoState();
  GetExposureState(photo_capabilities);
  GetFocusState(photo_capabilities);
  GetFlashState(photo_capabilities);

  photo_capabilities->height = mojom::Range::New(
      capture_format_.frame_size.height(), capture_format_.frame_size.height(),
      capture_format_.frame_size.height(), 0 /* step */);
  photo_capabilities->width = mojom::Range::New(
      capture_format_.frame_size.width(), capture_format_.frame_size.width(),
      capture_format_.frame_size.width(), 0 /* step */);
  std::move(callback).Run(std::move(photo_capabilities));
}

void OHOSCaptureDelegate::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_) {
    return;
  }
  std::move(callback).Run(true);
}

void OHOSCaptureDelegate::MaybeSuspend() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_) {
    return;
  }
  OH_CaptureSession_Stop(capture_session_);
}

void OHOSCaptureDelegate::Resume() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_) {
    return;
  }
  OH_CaptureSession_Stop(capture_session_);
  OH_CaptureSession_Start(capture_session_);
}

void OHOSCaptureDelegate::OnBufferAvailable(uint8_t* data, size_t data_size) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  const base::TimeTicks now = base::TimeTicks::Now();
  if (first_ref_time_.is_null()) {
    first_ref_time_ = now;
  }
  if (client_ != nullptr) {
    client_->OnIncomingCapturedData(
        data, data_size, capture_format_, gfx::ColorSpace(),
        0 /* clockwise rotation */, false /* flip_y */, now,
        now - first_ref_time_, std::nullopt);

    while (!take_photo_callbacks_.empty()) {
      VideoCaptureDevice::TakePhotoCallback cb =
          std::move(take_photo_callbacks_.front());
      take_photo_callbacks_.pop();

      mojom::BlobPtr blob = RotateAndBlobify(data, 0, capture_format_, 0);
      if (blob) {
        std::move(cb).Run(std::move(blob));
      }
    }
  } else {
    LOG(ERROR) << "OnBufferAvailable client is null";
  }
}

void OHOSCaptureDelegate::SetRotation(int rotation) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK_GE(rotation, 0);
  DCHECK_LT(rotation, 360);
  DCHECK_EQ(rotation % 90, 0);
  rotation_ = rotation;
}

base::WeakPtr<OHOSCaptureDelegate> OHOSCaptureDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

OHOSCaptureDelegate::~OHOSCaptureDelegate() = default;

Camera_ErrorCode OHOSCaptureDelegate::InitCameraInput(uint32_t index) {
  Camera_ErrorCode ret = CAMERA_SERVICE_FATAL_ERROR;

  if (camera_manager_ == nullptr) {
    LOG(ERROR) << "CameraManager is null";
    return ret;
  }

  if (camera_input_ == nullptr && index < camera_size_) {
    ret = OH_CameraManager_CreateCameraInput(camera_manager_, &cameras_[index],
                                             &camera_input_);
    if (ret != CAMERA_OK || camera_input_ == nullptr) {
      LOG(ERROR) << "OH_CameraManager_CreateCameraInput failed, ErrorCode:"
                 << ret;
      return ret;
    }

    ret = OH_CameraInput_Open(camera_input_);
    if (ret != CAMERA_OK) {
      LOG(ERROR) << "OH_CameraInput_Open failed, ErrorCode:" << ret;
      return ret;
    }
  }

  return CAMERA_OK;
}

void PreviewOutputOnFrameStart(Camera_PreviewOutput* preview_output_) {
  LOG(INFO) << "PreviewOutputOnFrameStart";
}

void PreviewOutputOnFrameEnd(Camera_PreviewOutput* preview_output_,
                             int32_t frameCount) {
  LOG(INFO) << "PreviewOutputOnFrameEnd frameCount:" << frameCount;
}

void PreviewOutputOnError(Camera_PreviewOutput* preview_output_,
                          Camera_ErrorCode errorCode) {
  LOG(INFO) << "PreviewOutputOnError ErrorCode:" << errorCode;
}

PreviewOutput_Callbacks* GetPreviewOutputListener() {
  static PreviewOutput_Callbacks previewOutputListener = {
      .onFrameStart = PreviewOutputOnFrameStart,
      .onFrameEnd = PreviewOutputOnFrameEnd,
      .onError = PreviewOutputOnError};
  return &previewOutputListener;
}

Camera_ErrorCode OHOSCaptureDelegate::InitPreviewOutput(uint32_t index) {
  Camera_ErrorCode ret = CAMERA_SERVICE_FATAL_ERROR;

  if (camera_manager_ == nullptr) {
    LOG(ERROR) << "CameraManager is null";
    return ret;
  }

  if (preview_output_ == nullptr) {
    std::string surfaceId = ohos::adapter::MediaAdapter::GetPreviewSurfaceId();
    ret = OH_CameraManager_CreatePreviewOutput(
        camera_manager_, camera_output_capability_->previewProfiles[index],
        surfaceId.c_str(), &preview_output_);
    if (ret != CAMERA_OK) {
      LOG(ERROR) << "OH_CameraManager_CreatePreviewOutput failed, ErrorCode:"
                 << ret;
      return ret;
    }
  }

  ret = OH_PreviewOutput_RegisterCallback(preview_output_,
                                          GetPreviewOutputListener());
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_PreviewOutput_RegisterCallback failed, ErrorCode:" << ret;
    return ret;
  }
  return CAMERA_OK;
}

Camera_ErrorCode OHOSCaptureDelegate::InitCaptureSession() {
  Camera_ErrorCode ret = CAMERA_SERVICE_FATAL_ERROR;
  if (camera_manager_ == nullptr) {
    LOG(ERROR) << "CameraManager is null";
    return ret;
  }

  ret =
      OH_CameraManager_CreateCaptureSession(camera_manager_, &capture_session_);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CameraManager_CreateCaptureSession failed, ErrorCode:"
               << ret;
    return ret;
  }

  OH_CaptureSession_BeginConfig(capture_session_);

  OH_CaptureSession_AddInput(capture_session_, camera_input_);
  OH_CaptureSession_AddPreviewOutput(capture_session_, preview_output_);

  ret = OH_CaptureSession_CommitConfig(capture_session_);
  if (ret != CAMERA_OK) {
    LOG(ERROR) << "OH_CaptureSession_CommitConfig failed, ErrorCode:" << ret;
    return ret;
  }

  return CAMERA_OK;
}

bool OHOSCaptureDelegate::StartStream() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_capturing_);

  if (is_capturing_) {
    LOG(ERROR) << "camera is not closed";
    return false;
  }

  Camera_ErrorCode ret;
  camera_size_ = 0;
  ret = OH_CameraManager_GetSupportedCameras(camera_manager_, &cameras_,
                                             &camera_size_);
  if (ret != Camera_ErrorCode::CAMERA_OK || camera_size_ <= 0) {
    LOG(ERROR) << "OH_CameraManager_GetSupportedCameras failed, ErrorCode:"
               << ret;
    return false;
  }

  uint32_t camera_index;
  for (camera_index = 0; camera_index < camera_size_; camera_index++) {
    if (cameras_[camera_index].cameraId == device_descriptor_.device_id) {
      break;
    }
  }

  ret = OH_CameraManager_GetSupportedCameraOutputCapability(
      camera_manager_, &cameras_[camera_index], &camera_output_capability_);
  if (ret != Camera_ErrorCode::CAMERA_OK) {
    LOG(ERROR)
        << "OH_CameraManager_GetSupportedCameraOutputCapability, ErrorCode:"
        << ret;
    return false;
  }

  if (InitCameraInput(camera_index) != Camera_ErrorCode::CAMERA_OK) {
    LOG(ERROR) << "InitCameraInput failed, camera_index:" << camera_index;
    return false;
  }

  int profile_index = GetIndexOfMatchedProfile();
  if (profile_index < 0 ||
      InitPreviewOutput(profile_index) != Camera_ErrorCode::CAMERA_OK) {
    LOG(ERROR) << "InitPreviewOutput failed, profile_index:" << profile_index;
    return false;
  }

  if (InitCaptureSession() != Camera_ErrorCode::CAMERA_OK) {
    LOG(ERROR) << "InitCaptureSession failed";
    return false;
  }

  OH_CaptureSession_Start(capture_session_);

  ohos::adapter::MediaAdapter::InitImageReceiver();
  is_capturing_ = true;
  return true;
}

Camera_ErrorCode OHOSCaptureDelegate::ReleaseSession() {
  if (capture_session_ != nullptr) {
    OH_CaptureSession_Stop(capture_session_);
    OH_CaptureSession_Release(capture_session_);
    capture_session_ = nullptr;
  }

  return Camera_ErrorCode::CAMERA_OK;
}

Camera_ErrorCode OHOSCaptureDelegate::ReleaseSessionResource() {
  if (camera_input_ != nullptr) {
    OH_CameraInput_Close(camera_input_);
    OH_CameraInput_Release(camera_input_);
    camera_input_ = nullptr;
  }

  if (preview_output_ != nullptr) {
    OH_PreviewOutput_Stop(preview_output_);
    OH_PreviewOutput_Release(preview_output_);
    preview_output_ = nullptr;
  }

  return Camera_ErrorCode::CAMERA_OK;
}

bool OHOSCaptureDelegate::StopStream() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_) {
    return false;
  }

  if (capture_session_) {
    ReleaseSessionResource();
    ReleaseSession();
    capture_session_ = nullptr;
  }
  is_capturing_ = false;

  if (cameras_ != nullptr) {
    OH_CameraManager_DeleteSupportedCameras(camera_manager_, cameras_,
                                            camera_size_);
    cameras_ = nullptr;
  }

  if (camera_output_capability_ != nullptr) {
    OH_CameraManager_DeleteSupportedCameraOutputCapability(
        camera_manager_, camera_output_capability_);
    camera_output_capability_ = nullptr;
  }

  return true;
}

int OHOSCaptureDelegate::GetIndexOfMatchedProfile() {
  int request_width = capture_params_.requested_format.frame_size.width();
  int request_height = capture_params_.requested_format.frame_size.height();

  capture_format_.frame_size.SetSize(request_width, request_height);
  capture_format_.frame_rate = capture_params_.requested_format.frame_rate;

  for (uint32_t i = 0; i < camera_output_capability_->previewProfilesSize;
       i++) {
    VideoPixelFormat pixel_format =
        VideoCaptureCommonOHOS::GetCameraPixelFormatType(
            camera_output_capability_->previewProfiles[i]->format);
    if (pixel_format == PIXEL_FORMAT_UNKNOWN) {
      continue;
    }
    if ((camera_output_capability_->previewProfiles[i]->size.width ==
         (uint32_t)request_width) &&
        (camera_output_capability_->previewProfiles[i]->size.height ==
         (uint32_t)request_height)) {
      capture_format_.pixel_format = pixel_format;
      LOG(INFO) << "VideoCaptureDeviceOHOS::GetIndexOfMatchedProfile"
                << ", width: " << capture_format_.frame_size.width()
                << ", height: " << capture_format_.frame_size.height()
                << ", frame_rate: " << capture_format_.frame_rate
                << ", pixel_format: " << capture_format_.pixel_format;
      return i;
    }
  }
  LOG(ERROR) << "VideoCaptureDeviceOHOS::GetIndexOfMatchedProfile failed.";
  capture_format_.pixel_format = PIXEL_FORMAT_UNKNOWN;
  return -1;
}

void OHOSCaptureDelegate::SetErrorState(VideoCaptureError error,
                                        const base::Location& from_here,
                                        const std::string& reason) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  client_->OnError(error, from_here, reason);
}

}  // namespace media
