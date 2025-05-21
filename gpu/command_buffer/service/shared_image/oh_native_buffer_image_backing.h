// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OH_NATIVE_BUFFER_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OH_NATIVE_BUFFER_IMAGE_BACKING_H_

#include "base/containers/flat_map.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

struct OH_NativeBuffer;

namespace gpu {

class ScopedOHNativeBuffer {
 public:
  ScopedOHNativeBuffer();
  explicit ScopedOHNativeBuffer(OH_NativeBuffer* buffer);
  ScopedOHNativeBuffer(ScopedOHNativeBuffer&& other);
  ~ScopedOHNativeBuffer();

  ScopedOHNativeBuffer(const ScopedOHNativeBuffer& other) = delete;
  const ScopedOHNativeBuffer& operator=(const ScopedOHNativeBuffer& other) =
      delete;

  const ScopedOHNativeBuffer& operator=(ScopedOHNativeBuffer&& other);
  void Reset();
  OH_NativeBuffer* Release();
  ScopedOHNativeBuffer Clone() const;
  bool is_valid() const { return buffer_ != nullptr; }
  OH_NativeBuffer* buffer() const { return buffer_; }

 private:
  OH_NativeBuffer* buffer_ = nullptr;
};

// Implementation of SharedImageBacking that holds an OHNativeBuffer. This
// can be used to create a GL texture or a VK Image from the OHNativeBuffer
// backing.
class OHNativeBufferImageBacking : public AndroidImageBacking {
 public:
  OHNativeBufferImageBacking(const Mailbox& mailbox,
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
                             const GLFormatCaps& gl_format_caps);

  OHNativeBufferImageBacking(const OHNativeBufferImageBacking&) = delete;
  OHNativeBufferImageBacking& operator=(const OHNativeBufferImageBacking&) =
      delete;

  ~OHNativeBufferImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  ScopedOHNativeBuffer GetBuffer() const;
  // OverlayImage* BeginOverlayAccess(gfx::GpuFenceHandle&);
  // void EndOverlayAccess();

 protected:
  class GLRepresentation;
  class GLPassthroughRepresentation;
  class DrawRepresentation;

  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  ScopedOHNativeBuffer buffer_ GUARDED_BY(lock_);
  const bool use_passthrough_;
  const GLFormatCaps gl_format_caps_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OH_NATIVE_BUFFER_IMAGE_BACKING_H_
