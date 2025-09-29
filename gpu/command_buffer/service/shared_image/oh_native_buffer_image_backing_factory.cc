// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/oh_native_buffer_image_backing_factory.h"

#include <dawn/webgpu_cpp.h>
#include <native_buffer/native_buffer.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

// #include "base/android/android_hardware_buffer_compat.h"
// #include "base/android/scoped_hardware_buffer_fence_sync.h"
// #include "base/android/scoped_hardware_buffer_handle.h"
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
#include "gpu/command_buffer/service/shared_image/dawn_ahardwarebuffer_image_representation.h"
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#include "gpu/command_buffer/service/shared_image/oh_native_buffer_image_backing.h"
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
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
// #include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

#define EGL_NATIVE_BUFFER_OHOS 0x34E1

namespace gpu {
namespace {

constexpr viz::SharedImageFormat kSupportedFormats[]{
    viz::SinglePlaneFormat::kRGBA_8888, viz::SinglePlaneFormat::kBGRA_8888,
    viz::SinglePlaneFormat::kRGB_565,   viz::SinglePlaneFormat::kBGR_565,
    viz::SinglePlaneFormat::kRGBX_8888, viz::SinglePlaneFormat::kRGBA_1010102};

// Returns whether the format is supported by OHNativeBuffer.
// TODO(vikassoni): In future we will need to expose the set of formats and
// constraints (e.g. max size) to the clients somehow that are available for
// certain combinations of SharedImageUsage flags (e.g. when Vulkan is on,
// (SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE) +
// SHARED_IMAGE_USAGE_DISPLAY_READ implies AHB, so those restrictions apply, but
// that's decided on the service side). For now getting supported format is a
// static mechanism like this. We probably need something like
// gpu::SharedImageCapabilities.texture_target_exception_list.
bool OHNativeBufferSupportedFormat(viz::SharedImageFormat format) {
  return base::Contains(kSupportedFormats, format);
}

// Returns the corresponding OHNativeBuffer format.
OH_NativeBuffer_Format OHNativeBufferFormat(viz::SharedImageFormat format) {
  DCHECK(OHNativeBufferSupportedFormat(format));

  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return NATIVEBUFFER_PIXEL_FMT_BGRA_8888;
  } else if (format == viz::SinglePlaneFormat::kRGB_565) {
    return NATIVEBUFFER_PIXEL_FMT_RGB_565;
  } else if (format == viz::SinglePlaneFormat::kBGR_565) {
    return NATIVEBUFFER_PIXEL_FMT_BGR_565;
  } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    return NATIVEBUFFER_PIXEL_FMT_RGBX_8888;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return NATIVEBUFFER_PIXEL_FMT_RGBA_1010102;
  }

  NOTREACHED();
}

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_RASTER_READ |
    SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY |
    SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_WEBGPU_READ |
    SHARED_IMAGE_USAGE_WEBGPU_WRITE | SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE |
    SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

}  // namespace

// static
OHNativeBufferImageBackingFactory::FormatInfo
OHNativeBufferImageBackingFactory::FormatInfoForSupportedFormat(
    viz::SharedImageFormat format,
    const gles2::Validators* validators,
    const GLFormatCaps& gl_format_caps) {
  CHECK(OHNativeBufferSupportedFormat(format));

  FormatInfo info;
  info.ohnb_format = OHNativeBufferFormat(format);

  // TODO(vikassoni): In future when we use GL_TEXTURE_EXTERNAL_OES target
  // with AHB, we need to check if oes_egl_image_external is supported or
  // not.
  const bool is_egl_image_supported =
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image;
  if (!is_egl_image_supported) {
    return info;
  }

  // Check if AHB backed GL texture can be created using this format and
  // gather GL related format info.
  // TODO(vikassoni): Add vulkan related information in future.
  GLFormatDesc format_desc =
      gl_format_caps.ToGLFormatDescOverrideHalfFloatType(format,
                                                         /*plane_index=*/0);
  GLuint internal_format = format_desc.image_internal_format;
  GLenum gl_format = format_desc.data_format;
  GLenum gl_type = format_desc.data_type;

  // OHNativeBufferImageBacking supports internal format GL_RGBA and GL_RGB.
  if (internal_format != GL_RGBA && internal_format != GL_RGB &&
      internal_format != GL_RGBA16F) {
    return info;
  }

  // kRGBA_F16 is a core part of ES3.
  const bool at_least_es3 = gl::g_current_gl_version->IsAtLeastGLES(3, 0);
  bool supports_data_type = (gl_type == GL_HALF_FLOAT && at_least_es3) ||
                            validators->pixel_type.IsValid(gl_type);
  bool supports_internal_format =
      (internal_format == GL_RGBA16F && at_least_es3) ||
      validators->texture_internal_format.IsValid(internal_format);

  // Validate if GL format, type and internal format is supported.
  if (supports_internal_format &&
      validators->texture_format.IsValid(gl_format) && supports_data_type) {
    info.gl_supported = true;
    info.gl_format = gl_format;
    info.gl_type = gl_type;
    info.internal_format = internal_format;
  }
  return info;
}

OHNativeBufferImageBackingFactory::OHNativeBufferImageBackingFactory(
    const gles2::FeatureInfo* feature_info,
    const GpuPreferences& gpu_preferences)
    : SharedImageBackingFactory(kSupportedUsage),
      use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder),
      gl_format_caps_(GLFormatCaps(feature_info)) {
  // Build the feature info for all the supported formats.
  for (auto format : kSupportedFormats) {
    format_infos_[format] = FormatInfoForSupportedFormat(
        format, feature_info->validators(), gl_format_caps_);
  }

  // TODO(vikassoni): We are using below GL api calls for now as Vulkan mode
  // doesn't exist. Once we have vulkan support, we shouldn't query GL in this
  // code until we are asked to make a GL representation (or allocate a
  // backing for import into GL)? We may use an OHNativeBuffer exclusively
  // with Vulkan, where there is no need to require that a GL context is
  // current. Maybe we can lazy init this if someone tries to create an
  // OHNativeBuffer with SHARED_IMAGE_USAGE_GLES2_READ |
  // SHARED_IMAGE_USAGE_GLES2_WRITE || !gpu_preferences.enable_vulkan. When in
  // Vulkan mode, we should only need this with GLES2.
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_gl_texture_size_);

  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_gl_texture_size_ = std::min(max_gl_texture_size_, INT_MAX - 1);
}

OHNativeBufferImageBackingFactory::~OHNativeBufferImageBackingFactory() =
    default;

bool OHNativeBufferImageBackingFactory::ValidateUsage(
    SharedImageUsageSet usage,
    const gfx::Size& size,
    viz::SharedImageFormat format) const {
  if (!OHNativeBufferSupportedFormat(format)) {
    LOG(ERROR) << "viz::SharedImageFormat " << format.ToString()
               << " not supported by OHNativeBuffer";
    return false;
  }

  // Check if AHB can be created with the current size restrictions.
  // TODO(vikassoni): Check for VK size restrictions for VK import, GL size
  // restrictions for GL import OR both if this backing is needed to be used
  // with both GL and VK.
  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_gl_texture_size_ ||
      size.height() > max_gl_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size=" << size.ToString()
               << " max_gl_texture_size=" << max_gl_texture_size_;
    return false;
  }

  return true;
}

std::unique_ptr<SharedImageBacking>
OHNativeBufferImageBackingFactory::MakeBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  DCHECK(!format.IsCompressed());

  if (!ValidateUsage(usage, size, format)) {
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  auto estimated_size = format.MaybeEstimatedSizeInBytes(size);
  if (!estimated_size) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  const FormatInfo& format_info = GetFormatInfo(format);

  // Setup OHNativeBuffer.
  OH_NativeBuffer_Config ohnb_config = {};
  ohnb_config.width = size.width();
  ohnb_config.height = size.height();
  ohnb_config.format = format_info.ohnb_format;

  // Set usage so that gpu can both read as a texture/write as a framebuffer
  // attachment. TODO(vikassoni): Find out if we need to set some more usage
  // flags based on the usage params in the current function call.
  ohnb_config.usage =
      NATIVEBUFFER_USAGE_HW_TEXTURE | NATIVEBUFFER_USAGE_HW_RENDER;

  // Add WRITE usage as we'll it need to upload data
  if (!pixel_data.empty()) {
    ohnb_config.usage |= NATIVEBUFFER_USAGE_CPU_WRITE;
  }

  ScopedOHNativeBuffer buffer(OH_NativeBuffer_Alloc(&ohnb_config));
  if (!buffer.is_valid()) {
    LOG(ERROR) << "OH_NativeBuffer_Alloc() failed";
    return nullptr;
  }

  base::ScopedFD initial_upload_fd;
  // Upload data if necessary
  if (!pixel_data.empty()) {
    // // Get description about buffer to obtain stride
    // OHNativeBuffer_Desc hwb_info;
    // base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer,
    //                                                           &hwb_info);
    // void* address = nullptr;
    // if (int error = base::AndroidHardwareBufferCompat::GetInstance().Lock(
    //         buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, 0, &address))
    //         {
    //   LOG(ERROR) << "Failed to lock OHNativeBuffer: " << error;
    //   return nullptr;
    // }

    // int bytes_per_pixel = format.BitsPerPixel() / 8;

    // // NOTE: hwb_info.stride is in pixels
    // const size_t dst_stride = bytes_per_pixel * hwb_info.stride;
    // const size_t src_stride = bytes_per_pixel * size.width();
    // const size_t height = size.height();

    // if (pixel_data.size() != src_stride * height) {
    //   DLOG(ERROR) << "Invalid initial pixel data size";
    //   return nullptr;
    // }

    // for (size_t y = 0; y < height; y++) {
    //   void* dst = reinterpret_cast<uint8_t*>(address) + dst_stride * y;
    //   const void* src = pixel_data.data() + src_stride * y;

    //   memcpy(dst, src, src_stride);
    // }

    // int32_t fence = -1;
    // base::AndroidHardwareBufferCompat::GetInstance().Unlock(buffer, &fence);
    // initial_upload_fd = base::ScopedFD(fence);
  }

  auto backing = std::make_unique<OHNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(buffer), estimated_size.value(),
      is_thread_safe, std::move(initial_upload_fd), use_passthrough_,
      gl_format_caps_);

  // If we uploaded initial data, set the backing as cleared.
  if (!pixel_data.empty()) {
    backing->SetCleared();
  }

  return backing;
}

std::unique_ptr<SharedImageBacking>
OHNativeBufferImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe) {
  return MakeBacking(mailbox, format, size, color_space, surface_origin,
                     alpha_type, usage, std::move(debug_label), is_thread_safe,
                     base::span<uint8_t>());
}

std::unique_ptr<SharedImageBacking>
OHNativeBufferImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  return MakeBacking(mailbox, format, size, color_space, surface_origin,
                     alpha_type, usage, std::move(debug_label), is_thread_safe,
                     pixel_data);
}

bool OHNativeBufferImageBackingFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  NOTIMPLEMENTED();
  return false;
}

bool OHNativeBufferImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER && !CanImportGpuMemoryBuffer(gmb_type)) {
    return false;
  }

  if (!OHNativeBufferSupportedFormat(format)) {
    return false;
  }

  const FormatInfo& format_info = GetFormatInfo(format);

  bool used_by_skia = usage.HasAny(
      SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE);
  bool used_by_gl = (HasGLES2ReadOrWriteUsage(usage)) ||
                    (used_by_skia && gr_context_type == GrContextType::kGL);

  // If usage flags indicated this backing can be used as a GL texture, then
  // do below gl related checks.
  if (used_by_gl) {
    // Check if the GL texture can be created from AHB with this format.
    if (!format_info.gl_supported) {
      LOG(FATAL)
          << "viz::SharedImageFormat " << format.ToString()
          << " can not be used to create a GL texture from OHNativeBuffer.";
    }
  }

  return true;
}

OHNativeBufferImageBackingFactory::FormatInfo::FormatInfo() = default;

OHNativeBufferImageBackingFactory::FormatInfo::~FormatInfo() = default;

std::unique_ptr<SharedImageBacking>
OHNativeBufferImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  NOTIMPLEMENTED();
  return nullptr;
#if 0
  CHECK_EQ(handle.type, gfx::ANDROID_HARDWARE_BUFFER);
  if (!ValidateUsage(usage, size, format)) {
    return nullptr;
  }

  auto estimated_size = format.MaybeEstimatedSizeInBytes(size);
  if (!estimated_size) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  auto backing = std::make_unique<OHNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle.android_hardware_buffer),
      estimated_size.value(), false, base::ScopedFD(), use_passthrough_,
      gl_format_caps_);

  backing->SetCleared();
  return backing;
#endif
}

SharedImageBackingType OHNativeBufferImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kOHNativeBuffer;
}

}  // namespace gpu
