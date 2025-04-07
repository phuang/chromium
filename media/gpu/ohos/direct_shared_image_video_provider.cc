// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/direct_shared_image_video_provider.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/ohos/shared_image_video_ohos.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

scoped_refptr<gpu::SharedContextState> GetSharedContext(
    gpu::CommandBufferStub* stub,
    gpu::ContextResult* result) {
  auto shared_context =
      stub->channel()->gpu_channel_manager()->GetSharedContextState(result);
  return (*result == gpu::ContextResult::kSuccess) ? shared_context : nullptr;
}

}  // namespace


DirectSharedImageVideoProvider::DirectSharedImageVideoProvider(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCB get_stub_cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      gpu_factory_(gpu_task_runner, std::move(get_stub_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)) {}

DirectSharedImageVideoProvider::~DirectSharedImageVideoProvider() = default;

void DirectSharedImageVideoProvider::Initialize(GpuInitCB gpu_init_cb) {
  gpu_factory_.AsyncCall(&GpuSharedImageVideoFactory::Initialize)
      .WithArgs(std::move(gpu_init_cb));
}

void DirectSharedImageVideoProvider::RequestImage(ImageReadyCB cb,
                                                  const ImageSpec& spec) {
  DVLOG(2) << "DirectSharedImageVideoProvider::RequestImage";
  gpu_factory_.AsyncCall(&GpuSharedImageVideoFactory::CreateImage)
      .WithArgs(base::BindPostTaskToCurrentDefault(std::move(cb)), spec, GetDrDcLock());
}

GpuSharedImageVideoFactory::GpuSharedImageVideoFactory(
    SharedImageVideoProvider::GetStubCB get_stub_cb) {
  DETACH_FROM_THREAD(thread_checker_);
  stub_ = get_stub_cb.Run();
  if (stub_)
    stub_->AddDestructionObserver(this);
}

GpuSharedImageVideoFactory::~GpuSharedImageVideoFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_)
    stub_->RemoveDestructionObserver(this);
}

void GpuSharedImageVideoFactory::Initialize(
    SharedImageVideoProvider::GpuInitCB gpu_init_cb) {
  DVLOG(2) << "DirectSharedImageVideoProvider::Initialize";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    LOG(ERROR) << "Make Context Current failed.";
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    LOG(ERROR) << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      shared_context->context(), shared_context->surface());
  if (!shared_context->IsCurrent(nullptr)) {
    LOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to make shared context current.";
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  std::move(gpu_init_cb).Run(std::move(shared_context));
}

void GpuSharedImageVideoFactory::CreateImage(
    FactoryImageReadyCB image_ready_cb,
    const SharedImageVideoProvider::ImageSpec& spec,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!stub_) {
    return;
  }
 
  auto codec_image =
      base::MakeRefCounted<CodecImage>(spec.coded_size, drdc_lock);
 
  TRACE_EVENT0("media", "GpuSharedImageVideoFactory::CreateVideoFrame");
 
  scoped_refptr<gpu::GpuChannelSharedImageInterface>
      gpu_channel_shared_image_interface =
          stub_->channel()->shared_image_stub()->shared_image_interface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu_channel_shared_image_interface->CreateSharedImageForOhosVideo(
          spec.coded_size, spec.color_space, codec_image);
  if (!shared_image) {
    return;
  }

  SharedImageVideoProvider::ImageRecord record;
  record.shared_image = std::move(shared_image);
  record.is_vulkan = false;
  // Since |codec_image|'s ref holders can be destroyed by stub destruction,
  // we create a ref to it for the MaybeRenderEarlyManager.  This is a hack;
  // we should not be sending the CodecImage at all.  The
  // MaybeRenderEarlyManager should work with some other object that happens
  // to be used by CodecImage, and non-GL things, to hold the output buffer,
  // etc.
  record.codec_image_holder = base::MakeRefCounted<CodecImageHolder>(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(codec_image));

  std::move(image_ready_cb).Run(std::move(record));
}

bool GpuSharedImageVideoFactory::CreateImageInternal(
    const SharedImageVideoProvider::ImageSpec& spec,
    gpu::Mailbox mailbox,
    scoped_refptr<CodecImage> image,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    LOG(ERROR) << "DirectSharedImageVideoProvider::CreateImageInternal "
                  "MakeContextCurrent failed";
    return false;
  }

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group) {
    LOG(ERROR) << "DirectSharedImageVideoProvider::CreateImageInternal "
                  "GetContextGroup failed";
    return false;
  }

  const auto& coded_size = spec.coded_size;

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    LOG(ERROR) << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    return false;
  }
  auto shared_image = gpu::SharedImageVideoOhos::Create(
      mailbox, coded_size, spec.color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, std::move(image), std::move(shared_context));
  DCHECK(stub_->channel()->gpu_channel_manager()->shared_image_manager());
  stub_->channel()->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image));
  return true;
}

NOINLINE void GpuSharedImageVideoFactory::OnWillDestroyStub(bool have_context) {
  LOG(ERROR) << "DirectSharedImageVideoProvider::OnWillDestroyStub";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
}

}  // namespace media
