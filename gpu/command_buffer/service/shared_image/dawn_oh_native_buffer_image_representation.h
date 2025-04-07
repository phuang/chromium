// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OH_NATIVE_BUFFER_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OH_NATIVE_BUFFER_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/oh_native_buffer_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

class DawnOHNativeBufferImageRepresentation : public DawnImageRepresentation {
 public:
  DawnOHNativeBufferImageRepresentation(
      SharedImageManager* manager,
      AndroidImageBacking* backing,
      MemoryTypeTracker* tracker,
      wgpu::Device device,
      wgpu::TextureFormat format,
      std::vector<wgpu::TextureFormat> view_formats,
      ScopedOHNativeBuffer buffer);
  ~DawnOHNativeBufferImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  void EndAccess() override;

 private:
  OHNativeBufferImageBacking* ohnb_backing() {
    return static_cast<OHNativeBufferImageBacking*>(backing());
  }

  wgpu::Texture texture_;
  wgpu::Device device_;
  wgpu::TextureFormat format_;
  std::vector<wgpu::TextureFormat> view_formats_;
  ScopedOHNativeBuffer buffer_;
  // There is a SharedTextureMemory per representation with how this works
  // currently. Switching to a single cached SharedTextureMemory for the backing
  // needs some care as multiple representations would use the same VkImage and
  // layout/queue transitions might be problematic.
  wgpu::SharedTextureMemory shared_texture_memory_;
  AccessMode access_mode_ = AccessMode::kNone;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OH_NATIVE_BUFFER_IMAGE_REPRESENTATION_H_
