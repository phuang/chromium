// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/frame_info_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/ohos/shared_image_video_ohos.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/gpu/ohos/codec_output_buffer_renderer.h"

namespace media {

FrameInfoHelper::FrameInfo::FrameInfo() = default;
FrameInfoHelper::FrameInfo::~FrameInfo() = default;
FrameInfoHelper::FrameInfo::FrameInfo(FrameInfo&&) = default;
FrameInfoHelper::FrameInfo::FrameInfo(const FrameInfoHelper::FrameInfo&) =
    default;
FrameInfoHelper::FrameInfo& FrameInfoHelper::FrameInfo::operator=(
    const FrameInfoHelper::FrameInfo&) = default;

class FrameInfoHelperImpl : public FrameInfoHelper,
                            public gpu::RefCountedLockHelperDrDc {
 public:
  FrameInfoHelperImpl(scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
                      SharedImageVideoProvider::GetStubCB get_stub_cb,
                      scoped_refptr<gpu::RefCountedLock> drdc_lock)
      : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)) {
    on_gpu_ = base::SequenceBound<OnGpu>(std::move(gpu_task_runner),
                                         std::move(get_stub_cb),
                                         std::move(drdc_lock));
  }

  ~FrameInfoHelperImpl() override = default;

  void GetFrameInfo(std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
                    FrameInfoReadyCB callback) override {
    Request request = {.buffer_renderer = std::move(buffer_renderer),
                       .callback = std::move(callback)};
    requests_.push(std::move(request));
    if (requests_.size() == 1) {
      ProcessRequestsQueue();
    }
  }

 private:
  struct Request {
    std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer;
    FrameInfoReadyCB callback;
  };

  class OnGpu : public gpu::RefCountedLockHelperDrDc {
   public:
    OnGpu(SharedImageVideoProvider::GetStubCB get_stub_cb,
          scoped_refptr<gpu::RefCountedLock> drdc_lock)
        : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
          frame_info_helper_holder_(
              base::MakeRefCounted<FrameInfoHelperHolder>(this)) {
      auto* stub = get_stub_cb.Run();
      if (stub) {
        gpu::ContextResult result;
        shared_context_ =
            stub->channel()->gpu_channel_manager()->GetSharedContextState(
                &result);
        if (result == gpu::ContextResult::kSuccess) {
          DCHECK(shared_context_);
          if (shared_context_->GrContextIsVulkan()) {
            vulkan_context_provider_ = shared_context_->vk_context_provider();
          }
        }
      }
    }

    ~OnGpu() {
      DCHECK(frame_info_helper_holder_);
      frame_info_helper_holder_->SetFrameInfoHelperOnGpuToNull();
    }

    void GetFrameInfoImpl(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                absl::optional<FrameInfo>)> cb) {
      AssertAcquiredDrDcLock();
      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      absl::optional<FrameInfo> info;

      if (buffer_renderer->RenderToTextureOwnerFrontBuffer()) {
        gfx::Size coded_size;
        gfx::Rect visible_rect;
        if (texture_owner->GetCodedSizeAndVisibleRect(
                buffer_renderer->size(), &coded_size, &visible_rect)) {
          info.emplace();
          info->coded_size = coded_size;
          info->visible_rect = visible_rect;
        }
      }

      std::move(cb).Run(std::move(buffer_renderer), info);
    }

    void GetFrameInfo(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                absl::optional<FrameInfo>)> cb) {
      base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      auto buffer_available_cb =
          base::BindOnce(&FrameInfoHelperHolder::GetFrameInfoImpl,
                         base::RetainedRef(frame_info_helper_holder_),
                         std::move(buffer_renderer), std::move(cb));

      texture_owner->RunWhenBufferIsAvailable(std::move(buffer_available_cb));
    }

   private:
    class FrameInfoHelperHolder
        : public base::RefCountedThreadSafe<FrameInfoHelperHolder> {
     public:
      explicit FrameInfoHelperHolder(raw_ptr<OnGpu> frame_info_helper_on_gpu)
          : frame_info_helper_on_gpu_(frame_info_helper_on_gpu) {
        DCHECK(frame_info_helper_on_gpu_);
      }

      void GetFrameInfoImpl(
          std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
          base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                  absl::optional<FrameInfo>)> cb) {
        base::AutoLock l(lock_);
        if (frame_info_helper_on_gpu_) {
          frame_info_helper_on_gpu_->GetFrameInfoImpl(
              std::move(buffer_renderer), std::move(cb));
        }
      }

      void SetFrameInfoHelperOnGpuToNull() {
        base::AutoLock l(lock_);
        frame_info_helper_on_gpu_ = nullptr;
      }

     private:
      friend class base::RefCountedThreadSafe<FrameInfoHelperHolder>;
      ~FrameInfoHelperHolder() = default;

      base::Lock lock_;
      raw_ptr<OnGpu> frame_info_helper_on_gpu_ GUARDED_BY(lock_) = nullptr;
    };
    scoped_refptr<gpu::SharedContextState> shared_context_;
    raw_ptr<viz::VulkanContextProvider> vulkan_context_provider_ = nullptr;
    scoped_refptr<FrameInfoHelperHolder> frame_info_helper_holder_;
  };

  FrameInfo GetFrameInfoWithVisibleSize(const gfx::Size& visible_size) {
    FrameInfo info;
    info.coded_size = visible_size;
    info.visible_rect = gfx::Rect(visible_size);
    return info;
  }

  void OnFrameInfoReady(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      absl::optional<FrameInfo> frame_info) {
    DCHECK(buffer_renderer);
    DCHECK(!requests_.empty());

    auto& request = requests_.front();

    if (frame_info) {
      visible_size_ = buffer_renderer->size();
      frame_info_ = *frame_info;
      std::move(request.callback).Run(std::move(buffer_renderer), frame_info_);
    } else {
      auto info = GetFrameInfoWithVisibleSize(buffer_renderer->size());
      std::move(request.callback)
          .Run(std::move(buffer_renderer), std::move(info));
    }
    requests_.pop();
    ProcessRequestsQueue();
  }

  void ProcessRequestsQueue() {
    while (!requests_.empty()) {
      auto& request = requests_.front();
      if (!request.buffer_renderer) {
        std::move(request.callback).Run(nullptr, FrameInfo());
      } else if (!request.buffer_renderer->texture_owner()) {
        auto info =
            GetFrameInfoWithVisibleSize(request.buffer_renderer->size());
        std::move(request.callback)
            .Run(std::move(request.buffer_renderer), std::move(info));
      } else if (visible_size_ == request.buffer_renderer->size()) {
        std::move(request.callback)
            .Run(std::move(request.buffer_renderer), frame_info_);
      } else {
        DVLOG(2) << "OnGpu::ProcessRequestsQueue will GetFrameInfo";
        auto cb = base::BindPostTaskToCurrentDefault(
            base::BindOnce(&FrameInfoHelperImpl::OnFrameInfoReady,
                           weak_factory_.GetWeakPtr()));

        on_gpu_.AsyncCall(&OnGpu::GetFrameInfo)
            .WithArgs(std::move(request.buffer_renderer), std::move(cb));
        break;
      }
      requests_.pop();
    }
  }

  base::SequenceBound<OnGpu> on_gpu_;
  std::queue<Request> requests_;

  // Cached values.
  FrameInfo frame_info_;
  gfx::Size visible_size_;

  base::WeakPtrFactory<FrameInfoHelperImpl> weak_factory_{this};
};

// static
std::unique_ptr<FrameInfoHelper> FrameInfoHelper::Create(
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::GetStubCB get_stub_cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  return std::make_unique<FrameInfoHelperImpl>(
      std::move(gpu_task_runner), std::move(get_stub_cb), std::move(drdc_lock));
}

}  // namespace media
