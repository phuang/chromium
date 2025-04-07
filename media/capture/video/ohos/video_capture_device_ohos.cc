// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/ohos/video_capture_device_ohos.h"

#include <cstddef>

#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "media/capture/video/ohos/ohos_capture_delegate.h"

namespace media {

VideoCaptureDeviceOHOS::VideoCaptureDeviceOHOS(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor),
      task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
           base::WithBaseSyncPrimitives()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      rotation_(0) {}

VideoCaptureDeviceOHOS::~VideoCaptureDeviceOHOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!capture_impl_)
      << "StopAndDeAllocate() must be called before destruction.";
}

void VideoCaptureDeviceOHOS::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!capture_impl_);

  LOG(INFO) << "VideoCaptureDeviceOHOS::AllocateAndStart: "
            << ", device_id: " << device_descriptor_.device_id
            << ", facing: " << device_descriptor_.facing
            << ", width: " << params.requested_format.frame_size.width()
            << ", height: " << params.requested_format.frame_size.height()
            << ", frame_rate: " << params.requested_format.frame_rate
            << ", pixel_format: " << params.requested_format.pixel_format
            << ", buffer_type: " << static_cast<int>(params.buffer_type)
            << ", resolution_change_policy: "
            << static_cast<int>(params.resolution_change_policy)
            << ", power_line_frequency: "
            << static_cast<int>(params.power_line_frequency)
            << ", enable_face_detection: " << params.enable_face_detection;

  capture_impl_ = std::make_unique<OHOSCaptureDelegate>(device_descriptor_,
                                                        task_runner_, params);
  if (!capture_impl_) {
    client->OnError(VideoCaptureError::
                        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate,
                    FROM_HERE, "Failed to create VideoCaptureDelegate");
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSCaptureDelegate::AllocateAndStart,
                     capture_impl_->GetWeakPtr(), std::move(client)));
}

void VideoCaptureDeviceOHOS::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!capture_impl_) {
    return;  // Wrong state.
  }

  // Shutdown must be synchronous, otherwise the next created capture device
  // may conflict.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoCaptureDeviceOHOS::StopAndDeAllocateInternal,
                         base::Unretained(this), base::Unretained(&waiter)))) {
    waiter.Wait();
  }
}

void VideoCaptureDeviceOHOS::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSCaptureDelegate::TakePhoto,
                     capture_impl_->GetWeakPtr(), std::move(callback)));
}

void VideoCaptureDeviceOHOS::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OHOSCaptureDelegate::GetPhotoState,
                     capture_impl_->GetWeakPtr(), std::move(callback)));
}

void VideoCaptureDeviceOHOS::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                             SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OHOSCaptureDelegate::SetPhotoOptions,
                                capture_impl_->GetWeakPtr(),
                                std::move(settings), std::move(callback)));
}

void VideoCaptureDeviceOHOS::SetRotation(int rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  rotation_ = rotation;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&OHOSCaptureDelegate::SetRotation,
                                        capture_impl_->GetWeakPtr(), rotation));
}

void VideoCaptureDeviceOHOS::StopAndDeAllocateInternal(
    base::WaitableEvent* waiter) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(capture_impl_);
  capture_impl_->StopAndDeAllocate();
  capture_impl_.reset();
  waiter->Signal();
}

}  // namespace media
