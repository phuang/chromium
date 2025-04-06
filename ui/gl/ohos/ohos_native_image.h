// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_OHOS_NATIVE_IMAGE_H_
#define UI_GL_OHOS_NATIVE_IMAGE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_export.h"
#include "native_image/native_image.h"

namespace gl {

class GL_EXPORT OhosNativeImage
    : public base::RefCountedThreadSafe<OhosNativeImage> {
 public:
  static scoped_refptr<OhosNativeImage> Create(int texture_id);

  OhosNativeImage(const OhosNativeImage&) = delete;
  OhosNativeImage& operator=(const OhosNativeImage&) = delete;
  void SetFrameAvailableCallback(base::RepeatingClosure callback);
  void UpdateNativeImage();
  void GetTransformMatrix(float mtx[16]);
  void AttachToGLContext();
  void DetachFromGLContext();
  void ReleaseNativeImage();
  void* AquireOhosNativeWindow();
  static void OnFrameAvailableListener(void* context);

 protected:
  explicit OhosNativeImage(OH_NativeImage* native_image);

 private:
  friend class base::RefCountedThreadSafe<OhosNativeImage>;
  virtual ~OhosNativeImage();

  OH_NativeImage* native_image_;
  base::RepeatingClosure frame_available_cb_;
};

}  // namespace gl

#endif // UI_GL_OHOS_NATIVE_IMAGE_H_