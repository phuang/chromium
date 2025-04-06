// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_media_decoder_bridge_impl.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_runner.h"
#include "base/trace_event/trace_event.h"

using namespace media;
using namespace std;

void OnError(OH_AVCodec* codec, int32_t error_code, void* user_data) {
  if (user_data) {
    ((CodecBridgeCallback*)(user_data))->OnError(error_code);
  }
}
void OnStreamChanged(OH_AVCodec* codec, OH_AVFormat* format, void* user_data) {
  if (user_data) {
    ((CodecBridgeCallback*)(user_data))->OnStreamChanged(format);
  }
}
void OnNeedInputData(OH_AVCodec* codec,
                     uint32_t index,
                     OH_AVMemory* data,
                     void* user_data) {
  if (user_data == nullptr) {
    return;
  }
  if (data != nullptr && OH_AVMemory_GetAddr(data) != nullptr) {
    OhosBuffer ohos_buffer_;
    ohos_buffer_.addr = OH_AVMemory_GetAddr(data);
    ohos_buffer_.buffer_size = OH_AVMemory_GetSize(data);
    ((CodecBridgeCallback*)(user_data))->OnNeedInputData(index, ohos_buffer_);
  }
}
void OnNewOutputData(OH_AVCodec* codec,
                     uint32_t index,
                     OH_AVMemory* data,
                     OH_AVCodecBufferAttr* attr,
                     void* user_data) {
  if (user_data == nullptr) {
    return;
  }
  if (attr != nullptr) {
    BufferInfo info;
    info.presentation_time_us = attr->pts;
    info.size = attr->size;
    info.offset = attr->offset;
    ((CodecBridgeCallback*)(user_data))
        ->OnNewOutputData(index, info, attr->flags);
  }
}

void clearInputQueue(std::queue<VideoBridgeDecoderInputBuffer>& q) {
  std::queue<VideoBridgeDecoderInputBuffer> empty;
  std::swap(empty, q);
}

void clearOutputQueue(std::queue<VideoBridgeDecoderOutputBuffer>& q) {
  std::queue<VideoBridgeDecoderOutputBuffer> empty;
  std::swap(empty, q);
}

VideoBridgeCodecConfig::VideoBridgeCodecConfig() = default;
VideoBridgeCodecConfig::~VideoBridgeCodecConfig() = default;

// static
std::unique_ptr<MediaCodecDecoderBridgeImpl>
MediaCodecDecoderBridgeImpl::CreateVideoDecoder(
    const VideoBridgeCodecConfig& config) {
  std::string codec_type;
  if (config.codec == media::VideoCodec::kH264) {
    LOG(INFO) << "MediaCodecDecoderBridgeImpl::CreateVideoDecoder video/avc";
    codec_type = "video/avc";
  } else if (config.codec == media::VideoCodec::kHEVC) {
    LOG(INFO) << "MediaCodecDecoderBridgeImpl::CreateVideoDecoder video/hevc";
    codec_type = "video/hevc";
  } else {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::CreateVideoDecoder not supported type.";
    return nullptr;
  }
  return base::WrapUnique(new MediaCodecDecoderBridgeImpl(
      codec_type, config.on_buffers_available_cb));
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::CreateVideoBridgeDecoderByMime(
    std::string mimetype) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::CreateVideoBridgeDecoderByMime.";

  if (video_decoder_ != nullptr) {
    LOG(ERROR) << "decoder is not NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  video_decoder_ = OH_VideoDecoder_CreateByMime(mimetype.c_str());
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << "create decoder failed.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  has_created_ = true;

  if (signal_ == nullptr) {
    signal_ = make_shared<DecoderBridgeSignal>();
  }

  if (cb_ == nullptr) {
    cb_ = make_shared<CodecBridgeCallback>(signal_);
  }
  OH_AVCodecAsyncCallback oh_cb = {&OnError, &OnStreamChanged, &OnNeedInputData,
                                   &OnNewOutputData};
  OH_AVErrCode ret =
      OH_VideoDecoder_SetCallback(video_decoder_, oh_cb, cb_.get());

  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::CreateVideoBridgeDecoderByName(
    std::string name) {
  LOG(INFO) << "create video decoder by name, type : " << name.c_str();

  if (video_decoder_ != nullptr) {
    LOG(ERROR) << "decoder is not NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  video_decoder_ = OH_VideoDecoder_CreateByName(name.c_str());
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << "create decoder failed.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  has_created_ = true;

  if (signal_ == nullptr) {
    signal_ = make_shared<DecoderBridgeSignal>();
  }

  if (cb_ == nullptr) {
    cb_ = make_shared<CodecBridgeCallback>(signal_);
  }

  OH_AVCodecAsyncCallback oh_cb = {&OnError, &OnStreamChanged, &OnNeedInputData,
                                   &OnNewOutputData};
  OH_AVErrCode ret =
      OH_VideoDecoder_SetCallback(video_decoder_, oh_cb, cb_.get());

  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

MediaCodecDecoderBridgeImpl::MediaCodecDecoderBridgeImpl(
    std::string codec_type,
    base::RepeatingClosure on_buffers_available_cb) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::MediaCodecDecoderBridgeImpl.";
  if (!on_buffers_available_cb) {
    return;
  }
  DecoderAdapterCode ret = CreateVideoBridgeDecoderByMime(codec_type);
  if (ret == DecoderAdapterCode::DECODER_ERROR) {
    LOG(ERROR) << "create decoder failed.";
    return;
  }
  cb_->on_buffers_available_cb_ = on_buffers_available_cb;
}

MediaCodecDecoderBridgeImpl::~MediaCodecDecoderBridgeImpl() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::~MediaCodecDecoderBridgeImpl.";
  ReleaseBridgeDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ConfigureBridgeDecoder(
    const DecoderFormat& format,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::ConfigureBridgeDecoder configure "
               "decoder.";
  width_ = format.width;
  height_ = format.height;
  decoder_task_runner_ = decoder_task_runner;
  cb_->decoder_callback_task_runner_ = decoder_task_runner;
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::ConfigureBridgeDecoder decoder "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  OH_AVFormat* av_format = OH_AVFormat_Create();
  OH_AVFormat_SetIntValue(av_format, OH_MD_KEY_WIDTH, format.width);
  OH_AVFormat_SetIntValue(av_format, OH_MD_KEY_HEIGHT, format.height);

  OH_AVErrCode ret = OH_VideoDecoder_Configure(video_decoder_, av_format);

  OH_AVFormat_Destroy(av_format);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::SetBridgeParameterDecoder(
    const DecoderFormat& format) {
  LOG(INFO) << " MediaCodecDecoderBridgeImpl::SetBridgeParameterDecoder set "
               "decoder parameter.";
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::SetBridgeParameterDecoder "
                  "decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  OH_AVFormat* av_format = OH_AVFormat_Create();
  OH_AVFormat_SetIntValue(av_format, OH_MD_KEY_WIDTH, format.width);
  OH_AVFormat_SetIntValue(av_format, OH_MD_KEY_HEIGHT, format.height);

  OH_AVErrCode ret = OH_VideoDecoder_SetParameter(video_decoder_, av_format);
  OH_AVFormat_Destroy(av_format);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface(
    void* window) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface set "
               "decoder outputsurface.";
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface decoder "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (window == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface window "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  OH_AVErrCode ret =
      OH_VideoDecoder_SetSurface(video_decoder_, (OHNativeWindow*)window);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::GetOutputFormatBridgeDecoder(
    DecoderFormat& format) {
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::GetOutputFormatBridgeDecoder "
                  "decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  OH_AVFormat* av_format = OH_VideoDecoder_GetOutputDescription(video_decoder_);
  if (av_format) {
    OH_AVFormat_GetIntValue(av_format, OH_MD_KEY_WIDTH, &format.width);
    OH_AVFormat_GetIntValue(av_format, OH_MD_KEY_HEIGHT, &format.height);
    OH_AVFormat_Destroy(av_format);
    return DecoderAdapterCode::DECODER_OK;
  }
  return DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::PrepareBridgeDecoder() {
  if (video_decoder_ == nullptr) {
    LOG(ERROR) << " MediaCodecDecoderBridgeImpl::PrepareBridgeDecoder decoder "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  OH_AVErrCode ret = OH_VideoDecoder_Prepare(video_decoder_);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::StartBridgeDecoder() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::StartBridgeDecoder start decoder.";
  is_running_.store(true);

  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::StartBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  OH_AVErrCode ret = OH_VideoDecoder_Start(video_decoder_);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::StopBridgeDecoder() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::StopBridgeDecoder stop decoder.";
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::StopBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  OH_AVErrCode ret = OH_VideoDecoder_Stop(video_decoder_);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::FlushBridgeDecoder() {
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::FlushBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  signal_->is_decoder_flushing_.store(true);

  OH_AVErrCode ret = OH_VideoDecoder_Flush(video_decoder_);
  if (ret != AV_ERR_OK) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::FlushBridgeDecoder flush "
                  "decoder failed. errcode:"
               << ret;
    return DecoderAdapterCode::DECODER_ERROR;
  }

  clearInputQueue(signal_->input_queue_);
  clearOutputQueue(signal_->output_queue_);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaCodecDecoderBridgeImpl::UpdateFlushToFalse,
                     base::Unretained(this)));
  return StartBridgeDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ResetBridgeDecoder() {
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::ResetBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  signal_->is_decoder_flushing_.store(true);
  OH_AVErrCode ret = OH_VideoDecoder_Reset(video_decoder_);
  if (ret != AV_ERR_OK) {
    LOG(ERROR) << " MediaCodecDecoderBridgeImpl::ResetBridgeDecoder reset "
                  "decoder failed. errcode:"
               << ret;
    return DecoderAdapterCode::DECODER_ERROR;
  }

  clearInputQueue(signal_->input_queue_);
  clearOutputQueue(signal_->output_queue_);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaCodecDecoderBridgeImpl::UpdateFlushToFalse,
                     base::Unretained(this)));
  return DecoderAdapterCode::DECODER_OK;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ReleaseBridgeDecoder() {
  LOG(INFO)
      << "MediaCodecDecoderBridgeImpl::ReleaseBridgeDecoder release decoder.";
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::ReleaseBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  is_running_.store(false);

  OH_AVErrCode ret = OH_VideoDecoder_Destroy(video_decoder_);
  video_decoder_ = nullptr;
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

void MediaCodecDecoderBridgeImpl::PopInqueueDec() {
  signal_->input_queue_.pop();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::PushInbufferDec(
    const uint32_t index,
    const uint32_t& buffer_size,
    const int64_t& time) {
  OH_AVCodecBufferFlags buffer_flag;
  if (is_first_decFrame_) {
    buffer_flag = AVCODEC_BUFFER_FLAGS_CODEC_DATA;
    is_first_decFrame_ = false;
  } else {
    buffer_flag = AVCODEC_BUFFER_FLAGS_NONE;
  }
  DVLOG(2) << "PushInbufferDec index:" << index
             << ", buffer_size:" << buffer_size;

  OH_AVCodecBufferAttr attr;
  attr.size = buffer_size;
  attr.offset = 0;
  attr.pts = time;
  attr.flags = buffer_flag;

  OH_AVErrCode ret = OH_VideoDecoder_PushInputData(video_decoder_, index, attr);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::PushInbufferDecEos(
    const uint32_t index) {
  OH_AVCodecBufferAttr attr;
  attr.size = 0;
  attr.offset = 0;
  attr.pts = 0;
  attr.flags = AVCODEC_BUFFER_FLAGS_EOS;
  OH_AVErrCode ret = OH_VideoDecoder_PushInputData(video_decoder_, index, attr);
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::QueueInputBuffer(
    const uint8_t* data,
    size_t data_size,
    int64_t presentation_time) {
  if (signal_ == nullptr || signal_->is_on_error_) {
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (signal_->is_decoder_flushing_.load() || signal_->input_queue_.empty()) {
    return DecoderAdapterCode::DECODER_RETRY;
  }
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::QueueInputBuffer decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  uint32_t index = signal_->input_queue_.front().input_buffer_index;
  OhosBuffer buffer = signal_->input_queue_.front().input_buffer;
  uint32_t buffer_size = buffer.buffer_size;

  size_t input_size = buffer_size >= data_size ? data_size : buffer_size;
  memcpy(buffer.addr, data, input_size);
  DecoderAdapterCode ret =
      PushInbufferDec(index, input_size, presentation_time);

  PopInqueueDec();
  return ret;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::QueueInputBufferEOS() {
  if (signal_ == nullptr || signal_->is_on_error_) {
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (signal_->is_decoder_flushing_.load() || signal_->input_queue_.empty() ||
      !is_running_.load()) {
    return DecoderAdapterCode::DECODER_RETRY;
  }
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::QueueInputBufferEOS decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  uint32_t index = signal_->input_queue_.front().input_buffer_index;
  DecoderAdapterCode ret = PushInbufferDecEos(index);

  PopInqueueDec();
  is_running_.store(false);
  return ret;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ReleaseOutputBuffer(
    uint32_t index,
    bool render) {
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::ReleaseOutputBuffer decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  OH_AVErrCode ret = AV_ERR_OK;
  if (render) {
    // Use RenderOutputData in surface mode and FreeOutputData in buffer mode.
    ret = OH_VideoDecoder_RenderOutputData(video_decoder_, index);
  } else {
    ret = OH_VideoDecoder_FreeOutputData(video_decoder_, index);
  }
  return ret == AV_ERR_OK ? DecoderAdapterCode::DECODER_OK
                          : DecoderAdapterCode::DECODER_ERROR;
}

void MediaCodecDecoderBridgeImpl::PopOutqueueDec() {
  signal_->output_queue_.pop();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    uint32_t& index,
    bool& eos) {
  if (signal_ == nullptr || signal_->is_on_error_) {
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (signal_->is_decoder_flushing_.load() || signal_->output_queue_.empty()) {
    return DecoderAdapterCode::DECODER_RETRY;
  }
  if (video_decoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::DequeueOutputBuffer decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  index = signal_->output_queue_.front().output_buffer_index;
  eos = (signal_->output_queue_.front().output_buffer_flags &
         AVCODEC_BUFFER_FLAGS_EOS) == AVCODEC_BUFFER_FLAGS_EOS;
  *presentation_time = base::Microseconds(
      signal_->output_queue_.front().output_buffer_info.presentation_time_us);

  PopOutqueueDec();
  return DecoderAdapterCode::DECODER_OK;
}

void MediaCodecDecoderBridgeImpl::DestoryNativeWindow(void* window) {
  if (window) {
    OH_NativeWindow_DestroyNativeWindow((OHNativeWindow*)window);
  }
}

void CodecBridgeCallback::OnError(int32_t error_code) {
  LOG(ERROR) << "CodecBridgeCallback::OnError error_code=" << error_code;
  signal_->is_on_error_ = true;
  clearInputQueue(signal_->input_queue_);
  clearOutputQueue(signal_->output_queue_);
}

void CodecBridgeCallback::OnStreamChanged(OH_AVFormat* format) {
  LOG(INFO) << "CodecBridgeCallback::OnStreamChanged Output Format Changed.";
}

void CodecBridgeCallback::OnNeedInputData(uint32_t index, OhosBuffer buffer) {
  DVLOG(2) << "CodecBridgeCallback::OnNeedInputData";
  if (!decoder_callback_task_runner_->RunsTasksInCurrentSequence()) {
    decoder_callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CodecBridgeCallback::OnNeedInputData,
                                  shared_from_this(), std::move(index),
                                  std::move(buffer)));
    return;
  }
  TRACE_EVENT0("media", "CodecBridgeCallback::OnNeedInputData");
  DVLOG(3) << "CodecBridgeCallback::OnNeedInputData Input Buffer Available, index= "
           << index;
  if (signal_->is_decoder_flushing_.load()) {
    return;
  }
  VideoBridgeDecoderInputBuffer input_buffer;
  input_buffer.input_buffer_index = index;
  input_buffer.input_buffer = buffer;
  signal_->input_queue_.push(input_buffer);
  on_buffers_available_cb_.Run();
}

void CodecBridgeCallback::OnNewOutputData(uint32_t index,
                                          BufferInfo info,
                                          uint32_t flags) {
  if (!decoder_callback_task_runner_->RunsTasksInCurrentSequence()) {
    decoder_callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CodecBridgeCallback::OnNewOutputData,
                                  shared_from_this(), std::move(index),
                                  std::move(info), std::move(flags)));
    return;
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  TRACE_EVENT0("media", "CodecBridgeCallback::OnNewOutputData");
  DVLOG(3) << "CodecBridgeCallback::OnNewOutputData Output Buffer Available, index = "
           << index << ", timestamp = " << info.presentation_time_us;
  if (signal_->is_decoder_flushing_.load()) {
    return;
  }
  VideoBridgeDecoderOutputBuffer outputBuffer;
  outputBuffer.output_buffer_index = index;
  outputBuffer.output_buffer_flags = flags;
  outputBuffer.output_buffer_info = info;
  signal_->output_queue_.push(outputBuffer);
  on_buffers_available_cb_.Run();
}
