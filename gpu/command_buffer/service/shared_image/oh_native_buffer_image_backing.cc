// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/oh_native_buffer_image_backing.h"

#include <dawn/webgpu_cpp.h>
#include <native_buffer/native_buffer.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
// #include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#include "gpu/command_buffer/service/shared_image/dawn_oh_native_buffer_image_representation.h"
// #include
// "gpu/command_buffer/service/shared_image/gl_texture_android_image_representation.h"
// #include
// "gpu/command_buffer/service/shared_image/gl_texture_passthrough_android_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
// #include
// "gpu/command_buffer/service/shared_image/skia_vk_android_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_image.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
// #include "ui/gfx/android/android_surface_control_compat.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/egl_fence_utils.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

#define EGL_NATIVE_BUFFER_OHOS 0x34E1

namespace gpu {
namespace {

GLuint CreateAndBindTexture(EGLImage image, GLenum target) {
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder texture_binder(target, service_id);

  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glEGLImageTargetTexture2DOES(target, image);

  return service_id;
}

gl::ScopedEGLImage CreateEGLImageFromOHNativeBuffer(
    const ScopedOHNativeBuffer& buffer) {
  EGLint egl_image_attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_FALSE, EGL_NONE};
  OHNativeWindowBuffer* window_buffer =
      OH_NativeWindow_CreateNativeWindowBufferFromNativeBuffer(buffer.buffer());
  DCHECK(window_buffer);
  return gl::MakeScopedEGLImage(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_OHOS,
                                window_buffer, egl_image_attribs);
}

}  // namespace

ScopedOHNativeBuffer::ScopedOHNativeBuffer() = default;

ScopedOHNativeBuffer::ScopedOHNativeBuffer(OH_NativeBuffer* buffer)
    : buffer_(buffer) {}

ScopedOHNativeBuffer::ScopedOHNativeBuffer(ScopedOHNativeBuffer&& other)
    : buffer_(other.buffer_) {
  other.buffer_ = nullptr;
}

ScopedOHNativeBuffer::~ScopedOHNativeBuffer() {
  Reset();
}

const ScopedOHNativeBuffer& ScopedOHNativeBuffer::operator=(
    ScopedOHNativeBuffer&& other) {
  Reset();
  std::swap(buffer_, other.buffer_);
  return *this;
}

void ScopedOHNativeBuffer::Reset() {
  if (buffer_) {
    OH_NativeBuffer_Unreference(buffer_);
    buffer_ = nullptr;
  }
}

OH_NativeBuffer* ScopedOHNativeBuffer::Release() {
  OH_NativeBuffer* buffer = buffer_;
  buffer_ = nullptr;
  return buffer;
}

ScopedOHNativeBuffer ScopedOHNativeBuffer::Clone() const {
  if (!is_valid()) {
    return {};
  }

  if (OH_NativeBuffer_Reference(buffer_) != 0) {
    return {};
  }

  return ScopedOHNativeBuffer(buffer_);
}

OHNativeBufferImageBacking::OHNativeBufferImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    ScopedOHNativeBuffer buffer,
    size_t estimated_size,
    bool is_thread_safe,
    base::ScopedFD initial_upload_fd,
    bool use_passthrough,
    const GLFormatCaps& gl_format_caps)
    : AndroidImageBacking(mailbox,
                          format,
                          size,
                          color_space,
                          surface_origin,
                          alpha_type,
                          usage,
                          std::move(debug_label),
                          estimated_size,
                          is_thread_safe,
                          std::move(initial_upload_fd)),
      buffer_(std::move(buffer)),
      use_passthrough_(use_passthrough),
      gl_format_caps_(gl_format_caps) {
  DCHECK(buffer_.is_valid());
}

OHNativeBufferImageBacking::~OHNativeBufferImageBacking() {
  // Locking here in destructor since we are accessing member variable
  // |have_context_| via have_context().
  AutoLock auto_lock(this);
  DCHECK(buffer_.is_valid());
}

SharedImageBackingType OHNativeBufferImageBacking::GetType() const {
  return SharedImageBackingType::kOHNativeBuffer;
}

gfx::Rect OHNativeBufferImageBacking::ClearedRect() const {
  AutoLock auto_lock(this);
  return ClearedRectInternal();
}

void OHNativeBufferImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  SetClearedRectInternal(cleared_rect);
}

void OHNativeBufferImageBacking::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

ScopedOHNativeBuffer OHNativeBufferImageBacking::GetBuffer() const {
  AutoLock auto_lock(this);
  return buffer_.Clone();
}

class OHNativeBufferImageBacking::GLRepresentation
    : public GLTextureImageRepresentation {
 public:
  GLRepresentation(SharedImageManager* manager,
                   SharedImageBacking* backing,
                   MemoryTypeTracker* tracker,
                   gl::ScopedEGLImage egl_image,
                   gles2::Texture* texture)
      : GLTextureImageRepresentation(manager, backing, tracker),
        egl_image_(std::move(egl_image)),
        texture_(texture) {}

  ~GLRepresentation() override {
    EndAccess();
    if (texture_) {
      texture_.ExtractAsDangling()->RemoveLightweightRef(has_context());
    }
  }

  GLRepresentation(const GLRepresentation&) = delete;
  GLRepresentation& operator=(const GLRepresentation&) = delete;

  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return texture_;
  }

  bool BeginAccess(GLenum mode) override {
    bool read_only_mode = (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    bool read_write_mode =
        (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    DCHECK(read_only_mode || read_write_mode);

    if (read_only_mode) {
      base::ScopedFD write_sync_fd;
      if (!ohnb_backing()->BeginRead(this, &write_sync_fd)) {
        return false;
      }
      if (!gl::InsertEglFenceAndWait(std::move(write_sync_fd))) {
        return false;
      }
    } else {
      base::ScopedFD sync_fd;
      if (!ohnb_backing()->BeginWrite(&sync_fd)) {
        return false;
      }

      if (!gl::InsertEglFenceAndWait(std::move(sync_fd))) {
        return false;
      }
    }

    if (read_only_mode) {
      mode_ = RepresentationAccessMode::kRead;
    } else {
      mode_ = RepresentationAccessMode::kWrite;
    }

    return true;
  }

  void EndAccess() override {
    if (mode_ == RepresentationAccessMode::kNone) {
      return;
    }

    base::ScopedFD sync_fd = gl::CreateEglFenceAndExportFd();

    // Pass this fd to its backing.
    if (mode_ == RepresentationAccessMode::kRead) {
      ohnb_backing()->EndRead(this, std::move(sync_fd));
    } else if (mode_ == RepresentationAccessMode::kWrite) {
      ohnb_backing()->EndWrite(std::move(sync_fd));
    }

    mode_ = RepresentationAccessMode::kNone;
  }

 private:
  OHNativeBufferImageBacking* ohnb_backing() {
    return static_cast<OHNativeBufferImageBacking*>(backing());
  }

  gl::ScopedEGLImage egl_image_;
  raw_ptr<gles2::Texture> texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
};

std::unique_ptr<GLTextureImageRepresentation>
OHNativeBufferImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                             MemoryTypeTracker* tracker) {
  AutoLock auto_lock(this);
  // Use same texture for all the texture representations generated from same
  // backing.
  DCHECK(buffer_.is_valid());

  auto egl_image = CreateEGLImageFromOHNativeBuffer(buffer_);
  if (!egl_image.is_valid()) {
    return nullptr;
  }

  GLFormatDesc gl_format_desc =
      gl_format_caps_.ToGLFormatDescOverrideHalfFloatType(format(),
                                                          /*plane_index=*/0);
  GLuint service_id =
      CreateAndBindTexture(egl_image.get(), gl_format_desc.target);

  auto* texture =
      gles2::CreateGLES2TextureWithLightRef(service_id, gl_format_desc.target);
  texture->SetLevelInfo(gl_format_desc.target, 0,
                        gl_format_desc.image_internal_format, size().width(),
                        size().height(), 1, 0, gl_format_desc.data_format,
                        gl_format_desc.data_type, ClearedRect());
  texture->SetImmutable(true, false);

  return std::make_unique<GLRepresentation>(
      manager, this, tracker, std::move(egl_image), std::move(texture));
}

class OHNativeBufferImageBacking::GLPassthroughRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLPassthroughRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              gl::ScopedEGLImage egl_image,
                              scoped_refptr<gles2::TexturePassthrough> texture)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        egl_image_(std::move(egl_image)),
        texture_(std::move(texture)) {}

  ~GLPassthroughRepresentation() override { EndAccess(); }

  GLPassthroughRepresentation(const GLPassthroughRepresentation&) = delete;
  GLPassthroughRepresentation& operator=(const GLPassthroughRepresentation&) =
      delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return texture_;
  }

  bool BeginAccess(GLenum mode) override {
    bool read_only_mode = (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    bool read_write_mode =
        (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    DCHECK(read_only_mode || read_write_mode);

    if (read_only_mode) {
      base::ScopedFD write_sync_fd;
      if (!ohnb_backing()->BeginRead(this, &write_sync_fd)) {
        return false;
      }
      if (!gl::InsertEglFenceAndWait(std::move(write_sync_fd))) {
        return false;
      }
    } else {
      base::ScopedFD sync_fd;
      if (!ohnb_backing()->BeginWrite(&sync_fd)) {
        return false;
      }

      if (!gl::InsertEglFenceAndWait(std::move(sync_fd))) {
        return false;
      }
    }

    if (read_only_mode) {
      mode_ = RepresentationAccessMode::kRead;
    } else {
      mode_ = RepresentationAccessMode::kWrite;
    }

    return true;
  }

  void EndAccess() override {
    if (mode_ == RepresentationAccessMode::kNone) {
      return;
    }

    base::ScopedFD sync_fd = gl::CreateEglFenceAndExportFd();

    // Pass this fd to its backing.
    if (mode_ == RepresentationAccessMode::kRead) {
      ohnb_backing()->EndRead(this, std::move(sync_fd));
    } else if (mode_ == RepresentationAccessMode::kWrite) {
      ohnb_backing()->EndWrite(std::move(sync_fd));
    }

    mode_ = RepresentationAccessMode::kNone;
  }

 private:
  OHNativeBufferImageBacking* ohnb_backing() {
    return static_cast<OHNativeBufferImageBacking*>(backing());
  }

  gl::ScopedEGLImage egl_image_;
  scoped_refptr<gles2::TexturePassthrough> texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
};

std::unique_ptr<GLTexturePassthroughImageRepresentation>
OHNativeBufferImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // Use same texture for all the texture representations generated from same
  // backing.

  DCHECK(buffer_.is_valid());
  auto egl_image = CreateEGLImageFromOHNativeBuffer(buffer_);
  if (!egl_image.is_valid()) {
    LOG(ERROR) << "Failed to create EGLImage from OHNativeBuffer";
    return nullptr;
  }

  GLFormatDesc gl_format_desc =
      gl_format_caps_.ToGLFormatDescOverrideHalfFloatType(format(),
                                                          /*plane_index=*/0);
  GLuint service_id =
      CreateAndBindTexture(egl_image.get(), gl_format_desc.target);

  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, gl_format_desc.target);
  texture->SetEstimatedSize(GetEstimatedSize());

  return std::make_unique<GLPassthroughRepresentation>(
      manager, this, tracker, std::move(egl_image), std::move(texture));
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
OHNativeBufferImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  AutoLock auto_lock(this);
  CHECK(context_state);
  CHECK(context_state->IsGraphiteDawn());

  auto device = context_state->dawn_context_provider()->GetDevice();
  auto backend_type = context_state->dawn_context_provider()->backend_type();
  auto dawn_representation = ProduceDawn(manager, tracker, device, backend_type,
                                         /*view_formats=*/{}, context_state);
  if (!dawn_representation) {
    LOG(ERROR) << "Could not create Dawn Representation";
    return nullptr;
  }

  // Use GPU main recorder since this should only be called for
  // fulfilling Graphite promise images on GPU main thread.
  // NOTE: OHNativeBufferImageBacking doesn't support multiplanar formats,
  // so there is no need to specify the `is_yuv_plane` or
  // `legacy_plane_index` optional parameters.
  return std::make_unique<SkiaGraphiteDawnImageRepresentation>(
      std::move(dawn_representation), context_state,
      context_state->gpu_main_graphite_recorder(), manager, this, tracker);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
OHNativeBufferImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  AutoLock auto_lock(this);
  DCHECK(context_state);

  DCHECK(context_state->GrContextIsGL());
  DCHECK(buffer_.is_valid());

  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation;
  if (use_passthrough_) {
    gl_representation = ProduceGLTexturePassthrough(manager, tracker);
  } else {
    gl_representation = ProduceGLTexture(manager, tracker);
  }

  if (!gl_representation) {
    return nullptr;
  }

  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

class OHNativeBufferImageRepresentation : public OverlayImageRepresentation {
 public:
  OHNativeBufferImageRepresentation(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}

  ~OHNativeBufferImageRepresentation() override { EndReadAccess({}); }

 private:
  OHNativeBufferImageBacking* ohnb_backing() {
    return static_cast<OHNativeBufferImageBacking*>(backing());
  }

  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    base::ScopedFD fd_to_wait_on;
    if (!ohnb_backing()->BeginRead(this, &fd_to_wait_on)) {
      return false;
    }

    acquire_fence.Adopt(std::move(fd_to_wait_on));
    is_reading_ = true;
    return true;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    if (is_reading_) {
      ohnb_backing()->EndRead(this, release_fence.Release());
      is_reading_ = false;
    }
  }

  OH_NativeBuffer* GetOHNativeBuffer() override {
    return ohnb_backing()->GetBuffer().buffer();
  }

  void InUseByWindowServerInc() override { ++in_use_count_; }

  void InUseByWindowServerDec() override {
    DCHECK(in_use_count_ != 0);
    --in_use_count_;
  }

  bool IsInUseByWindowServer() const override { return in_use_count_ > 0; }

  bool is_reading_ = false;
  std::atomic<uint32_t> in_use_count_ = 0;
};

std::unique_ptr<OverlayImageRepresentation>
OHNativeBufferImageBacking::ProduceOverlay(SharedImageManager* manager,
                                           MemoryTypeTracker* tracker) {
  AutoLock auto_lock(this);
  return std::make_unique<OHNativeBufferImageRepresentation>(manager, this,
                                                             tracker);
}

std::unique_ptr<DawnImageRepresentation>
OHNativeBufferImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  AutoLock auto_lock(this);
  // Use same texture for all the texture representations generated from same
  // backing.
  DCHECK(buffer_.is_valid());

  DCHECK_EQ(backend_type, wgpu::BackendType::Vulkan);
  wgpu::TextureFormat webgpu_format = ToDawnFormat(format());
  if (webgpu_format == wgpu::TextureFormat::Undefined) {
    LOG(ERROR) << "Unable to fine a suitable WebGPU format.";
    return nullptr;
  }

  return std::make_unique<DawnOHNativeBufferImageRepresentation>(
      manager, this, tracker, wgpu::Device(device), webgpu_format,
      std::move(view_formats), buffer_.Clone());
}

}  // namespace gpu
