// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_OHOS_WINDOW_H_
#define UI_PLATFORM_WINDOW_OHOS_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/platform_window/ohos/window_export.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

struct OH_NativeXComponent;

namespace ui::ohos {

class DisplayManager;

class OHOS_WINDOW_EXPORT Window : public PlatformWindow {
 public:
  Window(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  ~Window() override;

  void OnSurfaceCreated(OH_NativeXComponent* component, void* window);
  void OnSurfaceChanged(OH_NativeXComponent* component, void* window);
  void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window);
  void DispatchTouchEvent(OH_NativeXComponent* component, void* window);

 private:
  void Destroy();

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetTitle(const std::u16string& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  void StackAbove(gfx::AcceleratedWidget widget) override;
  void StackAtTop() override;
  void FlashFrame(bool flash_frame) override;
  void SetVisibilityChangedAnimationsEnabled(bool enabled) override;
  void SetShape(std::unique_ptr<ShapeRects> native_shape,
                const gfx::Transform& transform) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  bool IsAnimatingClosed() const override;

  bool IsFullscreen() const;

  void UpdateWindowBounds();

  base::ThreadChecker thread_checker_;
  raw_ptr<PlatformWindowDelegate> delegate_;
  raw_ptr<DisplayManager> display_manager_;

  OH_NativeXComponent* const native_xcomponent_;
  OHNativeWindow* window_ = nullptr;

  gfx::Rect bounds_;
};

}  // namespace ui::ohos

#endif  // UI_PLATFORM_WINDOW_OHOS_WINDOW_H_
