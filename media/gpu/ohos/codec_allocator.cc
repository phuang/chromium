// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/codec_allocator.h"

#include <cstddef>

#include <algorithm>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/limits.h"
#include "media/base/ohos/ohos_media_decoder_bridge_impl.h"
#include "media/base/timestamp_constants.h"

namespace media {
namespace {
std::unique_ptr<MediaCodecDecoderBridgeImpl> CreateMediaCodecInternal(
    CodecAllocator::CodecFactoryCB factory_cb,
    std::unique_ptr<VideoBridgeCodecConfig> codec_config) {
  TRACE_EVENT0("media", "CodecAllocator::CreateMediaCodecInternal");
  base::ScopedBlockingCall scoped_block(FROM_HERE,
                                        base::BlockingType::MAY_BLOCK);
  auto codecBridgeImpl = factory_cb.Run(*codec_config);
  if (codecBridgeImpl == nullptr) {
    return nullptr;
  }
  if (!codecBridgeImpl->CheckHasCreated()) {
    return nullptr;
  }
  return codecBridgeImpl;
}

void ReleaseMediaCodecInternal(
    std::unique_ptr<MediaCodecDecoderBridgeImpl> codec) {
  TRACE_EVENT0("media", "CodecAllocator::ReleaseMediaCodecInternal");
  base::ScopedBlockingCall scoped_block(FROM_HERE,
                                        base::BlockingType::MAY_BLOCK);
  codec->ReleaseBridgeDecoder();
  codec = nullptr;
}

scoped_refptr<base::SequencedTaskRunner> CreateCodecTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace

// static
CodecAllocator* CodecAllocator::GetInstance(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  static base::NoDestructor<CodecAllocator> allocator(
      base::BindRepeating(&MediaCodecDecoderBridgeImpl::CreateVideoDecoder),
      task_runner);

  DCHECK(!task_runner || allocator->task_runner_ == task_runner);
  return allocator.get();
}

void CodecAllocator::CreateMediaCodecAsync(
    CodecCreatedCB codec_created_cb,
    std::unique_ptr<VideoBridgeCodecConfig> codec_config) {
  DCHECK(codec_created_cb);

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CodecAllocator::CreateMediaCodecAsync, base::Unretained(this),
            base::BindPostTaskToCurrentDefault(std::move(codec_created_cb)),
            std::move(codec_config)));
    return;
  }

  auto* task_runner = SelectCodecTaskRunner();

  const auto start_time = tick_clock_->NowTicks();

  std::move(task_runner)
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CreateMediaCodecInternal, factory_cb_,
                         std::move(codec_config)),
          base::BindOnce(&CodecAllocator::OnCodecCreated,
                         base::Unretained(this), start_time,
                         std::move(codec_created_cb)));
}

void CodecAllocator::ReleaseMediaCodec(
    std::unique_ptr<MediaCodecDecoderBridgeImpl> codec,
    base::OnceClosure codec_released_cb) {
  DCHECK(codec);
  DCHECK(codec_released_cb);

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CodecAllocator::ReleaseMediaCodec,
                                  base::Unretained(this), std::move(codec),
                                  base::BindPostTaskToCurrentDefault(
                                      std::move(codec_released_cb))));
    return;
  }

  auto* task_runner = SelectCodecTaskRunner();
  const auto start_time = tick_clock_->NowTicks();

  task_runner->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReleaseMediaCodecInternal, std::move(codec)),
      base::BindOnce(&CodecAllocator::OnCodecReleased, base::Unretained(this),
                     start_time, std::move(codec_released_cb)));
}

CodecAllocator::CodecAllocator(
    CodecFactoryCB factory_cb,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      factory_cb_(std::move(factory_cb)),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

CodecAllocator::~CodecAllocator() = default;

void CodecAllocator::OnCodecCreated(
    base::TimeTicks start_time,
    CodecCreatedCB codec_created_cb,
    std::unique_ptr<MediaCodecDecoderBridgeImpl> codec) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::move(codec_created_cb).Run(std::move(codec));
}

void CodecAllocator::OnCodecReleased(base::TimeTicks start_time,
                                     base::OnceClosure codec_released_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::move(codec_released_cb).Run();
}

base::SequencedTaskRunner* CodecAllocator::SelectCodecTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!primary_task_runner_) {
    primary_task_runner_ = CreateCodecTaskRunner();
  }

  return primary_task_runner_.get();
}

}  // namespace media
