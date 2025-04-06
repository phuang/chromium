// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/video_frame_factory_impl.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/ohos/codec_image.h"
#include "media/gpu/ohos/codec_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

static void AllocateTextureOwnerOnGpuThread(
    VideoFrameFactory::InitCB init_cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock,
    scoped_refptr<gpu::SharedContextState> shared_context_state) {
  if (!shared_context_state) {
    std::move(init_cb).Run(nullptr);
    return;
  }

  std::move(init_cb).Run(
      gpu::NativeImageTextureOwner::Create(
        shared_context_state,
        gpu::NativeImageTextureOwner::Mode::kOhosSurfaceTexture));
}

}  // namespace

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    const gpu::GpuPreferences& gpu_preferences,
    std::unique_ptr<SharedImageVideoProvider> image_provider,
    std::unique_ptr<FrameInfoHelper> frame_info_helper,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      image_provider_(std::move(image_provider)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      frame_info_helper_(std::move(frame_info_helper)) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoFrameFactoryImpl::Initialize(InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto gpu_init_cb = base::BindOnce(&AllocateTextureOwnerOnGpuThread,
                                    base::BindPostTaskToCurrentDefault(std::move(init_cb)),
                                    GetDrDcLock());
  image_provider_->Initialize(std::move(gpu_init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  image_spec_.generation_id++;

  if (!surface_bundle) {
    codec_buffer_wait_coordinator_ = nullptr;
  } else {
    codec_buffer_wait_coordinator_ =
        surface_bundle->codec_buffer_wait_coordinator();
  }
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    OnceOutputCB output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size coded_size = output_buffer->size();
  gfx::Rect visible_rect(coded_size);

  auto output_buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
      std::move(output_buffer), codec_buffer_wait_coordinator_, GetDrDcLock());

  VideoPixelFormat pixel_format = PIXEL_FORMAT_ABGR;

  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE,
                                 coded_size, visible_rect, natural_size)) {
    LOG(ERROR) << __func__ << " unsupported video frame format";
    std::move(output_cb).Run(nullptr);
    return;
  }

  auto image_ready_cb =
      base::BindOnce(&VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady,
                     weak_factory_.GetWeakPtr(), std::move(output_cb),
                     timestamp, natural_size, !!codec_buffer_wait_coordinator_,
                     pixel_format, video_frame_copy_required_, gpu_task_runner_);

  RequestImage(std::move(output_buffer_renderer), std::move(image_ready_cb));
}

void VideoFrameFactoryImpl::RequestImage(
    std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
    ImageWithInfoReadyCB image_ready_cb) {
  auto info_cb =
      base::BindOnce(&VideoFrameFactoryImpl::CreateVideoFrame_OnFrameInfoReady,
                     weak_factory_.GetWeakPtr(), std::move(image_ready_cb));

  frame_info_helper_->GetFrameInfo(std::move(buffer_renderer),
                                   std::move(info_cb));
}

void VideoFrameFactoryImpl::CreateVideoFrame_OnFrameInfoReady(
    ImageWithInfoReadyCB image_ready_cb,
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    FrameInfoHelper::FrameInfo frame_info) {
  if (output_buffer_renderer) {
    image_spec_.coded_size = frame_info.coded_size;
    image_spec_.color_space = output_buffer_renderer->color_space();
  } else {
    if (image_spec_.coded_size.IsEmpty()) {
      DVLOG(2) << "VideoFrameFactoryImpl::CreateVideoFrame_OnFrameInfoReady.";
      std::move(image_ready_cb)
          .Run(nullptr, FrameInfoHelper::FrameInfo(),
               SharedImageVideoProvider::ImageRecord());
      return;
    }
  }
  DCHECK(!image_spec_.coded_size.IsEmpty());

  auto cb = base::BindOnce(std::move(image_ready_cb),
                           std::move(output_buffer_renderer), frame_info);
  image_provider_->RequestImage(std::move(cb), image_spec_);
}

// static
void VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady(
    base::WeakPtr<VideoFrameFactoryImpl> thiz,
    OnceOutputCB output_cb,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    bool is_texture_owner_backed,
    VideoPixelFormat pixel_format,
    bool video_frame_copy_required,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    FrameInfoHelper::FrameInfo frame_info,
    SharedImageVideoProvider::ImageRecord record) {
  TRACE_EVENT0("media", "VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady");

  if (!thiz)
    return;

  gfx::ColorSpace color_space = output_buffer_renderer->color_space();

  record.codec_image_holder->codec_image_raw()->Initialize(
      std::move(output_buffer_renderer), is_texture_owner_backed);

  auto codec_image_holder = std::move(record.codec_image_holder);

  auto frame = VideoFrame::WrapSharedImage(
      pixel_format, std::move(record.shared_image), gpu::SyncToken(),
      VideoFrame::ReleaseMailboxCB(), frame_info.coded_size,
      frame_info.visible_rect, natural_size, timestamp);

  DVLOG(3) << "CreateVideoFrame_OnImageReady frame id : "
           << frame->unique_id();
  frame->set_color_space(color_space);

  if (!frame) {
    LOG(ERROR) << " failed to create video frame";
    std::move(output_cb).Run(nullptr);
    return;
  }

  frame->metadata().copy_required = video_frame_copy_required;

  frame->metadata().allow_overlay = false;
  frame->metadata().texture_owner = is_texture_owner_backed;
  frame->SetReleaseMailboxCB(std::move(record.release_cb));
  std::move(output_cb).Run(std::move(frame));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto image_ready_cb = base::BindOnce(
      [](base::OnceClosure closure,
         std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
         FrameInfoHelper::FrameInfo frame_info,
         SharedImageVideoProvider::ImageRecord record) {
        std::move(closure).Run();
      },
      std::move(closure));

  RequestImage(nullptr, std::move(image_ready_cb));
}

}  // namespace media
