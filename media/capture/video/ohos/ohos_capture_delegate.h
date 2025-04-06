// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_CAPTURE_DELEGATE_H_
#define OHOS_CAPTURE_DELEGATE_H_

#include "base/containers/queue.h"
#include "base/threading/thread.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"
#include "ohos/adapter/media_manager/media_adapter.h"

#include "ohcamera/camera.h"
#include "ohcamera/camera_input.h"
#include "ohcamera/camera_manager.h"
#include "ohcamera/capture_session.h"

namespace media {
using media::mojom::MeteringMode;

static const int kSuccessReturnValue = 0;
static const int kErrorReturnValue = -1;

class CAPTURE_EXPORT OHOSCaptureDelegate final {
 public:
  OHOSCaptureDelegate(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      const scoped_refptr<base::SingleThreadTaskRunner>& capture_stask_runner,
      const VideoCaptureParams capture_params);

  OHOSCaptureDelegate(const OHOSCaptureDelegate&) = delete;
  OHOSCaptureDelegate& operator=(const OHOSCaptureDelegate&) = delete;

  OHOSCaptureDelegate() = default;

  ~OHOSCaptureDelegate();

  // Forward-to versions of VideoCaptureDevice virtual methods.
  void AllocateAndStart(std::unique_ptr<VideoCaptureDevice::Client> client);
  void StopAndDeAllocate();

  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);

  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);

  void MaybeSuspend();
  void Resume();

  void OnBufferAvailable(uint8_t* data, size_t data_size);

  void SetRotation(int rotation);

  base::WeakPtr<OHOSCaptureDelegate> GetWeakPtr();
  Camera_ErrorCode ReleaseSession();
  Camera_ErrorCode ReleaseSessionResource();

 private:
  bool StartStream();
  bool StopStream();

  Camera_ErrorCode InitCameraInput(uint32_t index);
  Camera_ErrorCode InitPreviewOutput(uint32_t index);
  Camera_ErrorCode InitCaptureSession();

  int GetUsableExposureMode(Camera_ExposureMode& exposure_mode_,
                            MeteringMode& exposure_mode);
  MeteringMode GetCurrentExposureMode(
      Camera_ExposureMode& exposure_mode_);
  void GetExposureState(mojom::PhotoStatePtr& photo_capabilities);
  mojom::RangePtr RetrieveUserControlRange();
  void GetFocusState(mojom::PhotoStatePtr& photo_capabilities);
  void GetFlashState(mojom::PhotoStatePtr& photo_capabilities);

  int GetIndexOfMatchedProfile();
  void SetErrorState(VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason);

  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  const VideoCaptureDeviceDescriptor device_descriptor_;

  // The following members are only known on AllocateAndStart().
  VideoCaptureFormat capture_format_;
  std::unique_ptr<VideoCaptureDevice::Client> client_;

  base::queue<VideoCaptureDevice::TakePhotoCallback> take_photo_callbacks_;

  bool is_capturing_;

  base::TimeTicks first_ref_time_;

  // Clockwise rotation in degrees. This value should be 0, 90, 180, or 270.
  int rotation_;

  base::WeakPtrFactory<OHOSCaptureDelegate> weak_factory_{this};

  const VideoCaptureParams capture_params_;

  Camera_Manager* camera_manager_ = nullptr;
  Camera_CaptureSession* capture_session_ = nullptr;
  Camera_OutputCapability* camera_output_capability_ = nullptr;
  Camera_Device* cameras_ = nullptr;
  Camera_Input* camera_input_ = nullptr;
  Camera_PreviewOutput* preview_output_ = nullptr;
  uint32_t camera_size_;
};

}  // namespace media

#endif  // OHOS_CAPTURE_DELEGATE_H_
