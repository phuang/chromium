// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/ohos/window.h"

#include <ace/xcomponent/native_interface_xcomponent.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/ohos/display_manager.h"

namespace ui::ohos {
namespace {

Window* g_window_ = nullptr;

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
  g_window_->OnSurfaceCreated(component, window);
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
  g_window_->OnSurfaceChanged(component, window);
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
  g_window_->OnSurfaceDestroyed(component, window);
}

void DispatchTouchEventCB(OH_NativeXComponent* component, void* window) {
  g_window_->DispatchTouchEvent(component, window);
}

OH_NativeXComponent_Callback g_callback_ = {
    &OnSurfaceCreatedCB,
    &OnSurfaceChangedCB,
    &OnSurfaceDestroyedCB,
    &DispatchTouchEventCB,
};

}  // namespace

Window::Window(PlatformWindowDelegate* delegate, const gfx::Rect& bounds)
    : delegate_(delegate),
      display_manager_(
          static_cast<DisplayManager*>(display::Screen::GetScreen())),
      native_xcomponent_(display_manager_->native_xcomponent()) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(delegate_);
  DCHECK(display_manager_);
  DCHECK(native_xcomponent_);
  LOG(ERROR) << "OH_NativeXComponent_RegisterCallback()";
  DCHECK(!g_window_);
  g_window_ = this;
  int32_t ret =
      OH_NativeXComponent_RegisterCallback(native_xcomponent_, &g_callback_);
  DCHECK_EQ(ret, OH_NATIVEXCOMPONENT_RESULT_SUCCESS);
}

Window::~Window() {
  DCHECK_EQ(this, g_window_);
  g_window_ = nullptr;
}

void Window::Destroy() {
  NOTIMPLEMENTED();
}

void Window::Show(bool inactive) {
  NOTIMPLEMENTED();
}

void Window::Hide() {
  NOTIMPLEMENTED();
}

void Window::Close() {
  NOTIMPLEMENTED();
  Destroy();
}

bool Window::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

void Window::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void Window::SetBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect Window::GetBoundsInPixels() const {
  return bounds_;
}

void Window::SetBoundsInDIP(const gfx::Rect& bounds) {
  // SetBounds should not be used on Windows tests.
  NOTIMPLEMENTED();
}
gfx::Rect Window::GetBoundsInDIP() const {
  // GetBounds should not be used on Windows tests.
  NOTIMPLEMENTED();
  return GetBoundsInPixels();
}

void Window::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED();
}

void Window::SetCapture() {
  NOTIMPLEMENTED();
}

void Window::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool Window::HasCapture() const {
  NOTIMPLEMENTED();
  return true;
}

void Window::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  NOTIMPLEMENTED();
}

void Window::Maximize() {}

void Window::Minimize() {}

void Window::Restore() {}

PlatformWindowState Window::GetPlatformWindowState() const {
  NOTIMPLEMENTED();
  return PlatformWindowState::kUnknown;
}

void Window::Activate() {
  NOTIMPLEMENTED();
}

void Window::Deactivate() {
  NOTIMPLEMENTED();
}

void Window::SetUseNativeFrame(bool use_native_frame) {
  NOTIMPLEMENTED();
}

bool Window::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED();
  return false;
}

void Window::SetCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  NOTIMPLEMENTED();
}

void Window::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void Window::ConfineCursorToBounds(const gfx::Rect& bounds) {}

void Window::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {}

gfx::Rect Window::GetRestoredBoundsInDIP() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool Window::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED();
  // The window contents need to be transparent when the titlebar area is drawn
  // by the DWM rather than Chrome, so that area can show through.  This
  // function does not describe the transparency of the whole window appearance,
  // but merely of the content Chrome draws, so even when the system titlebars
  // appear opaque, the content above them needs to be transparent, or they'll
  // be covered by a black (undrawn) region.
  return !IsFullscreen();
}

void Window::SetZOrderLevel(ZOrderLevel order) {
  NOTIMPLEMENTED();
}

ZOrderLevel Window::GetZOrderLevel() const {
  NOTIMPLEMENTED();
  return ZOrderLevel::kNormal;
}

void Window::StackAbove(gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::StackAtTop() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::SetVisibilityChangedAnimationsEnabled(bool enabled) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::SetShape(std::unique_ptr<ShapeRects> native_shape,
                      const gfx::Transform& transform) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::SetWindowIcons(const gfx::ImageSkia& window_icon,
                            const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Window::SizeConstraintsChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool Window::IsAnimatingClosed() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool Window::IsFullscreen() const {
  return GetPlatformWindowState() == PlatformWindowState::kFullScreen;
}

void Window::UpdateWindowBounds() {
  uint64_t width = 0;
  uint64_t height = 0;

  int32_t ret = OH_NativeXComponent_GetXComponentSize(native_xcomponent_,
                                                      window_, &width, &height);
  DCHECK_EQ(ret, OH_NATIVEXCOMPONENT_RESULT_SUCCESS);

  double x = 0.0;
  double y = 0.0;
  ret = OH_NativeXComponent_GetXComponentOffset(native_xcomponent_, window_, &x,
                                                &y);
  DCHECK_EQ(ret, OH_NATIVEXCOMPONENT_RESULT_SUCCESS);

  bounds_ = gfx::Rect(x, y, width, height);
}

void Window::OnSurfaceCreated(OH_NativeXComponent* native_xcomponent,
                              void* window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(native_xcomponent_, native_xcomponent);
  DCHECK(!window_);
  window_ = static_cast<OHNativeWindow*>(window);
  UpdateWindowBounds();
  gfx::AcceleratedWidget surface_handle = gfx::kNullAcceleratedWidget;
  int32_t result = OH_NativeWindow_GetSurfaceId(window_, &surface_handle);
  DCHECK_EQ(result, 0);
  delegate_->OnAcceleratedWidgetAvailable(surface_handle);
  delegate_->OnBoundsChanged({true});
}

void Window::OnSurfaceChanged(OH_NativeXComponent* native_xcomponent,
                              void* window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(native_xcomponent_, native_xcomponent);
  DCHECK_EQ(window_, window);
  UpdateWindowBounds();
  delegate_->OnBoundsChanged({true});
}

void Window::OnSurfaceDestroyed(OH_NativeXComponent* native_xcomponent,
                                void* window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(native_xcomponent_, native_xcomponent);
  DCHECK_EQ(window_, window);
  delegate_->OnAcceleratedWidgetDestroyed();
}

void Window::DispatchTouchEvent(OH_NativeXComponent* native_xcomponent,
                                void* window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(native_xcomponent_, native_xcomponent);
  OH_NativeXComponent_TouchEvent event;
  int32_t ret =
      OH_NativeXComponent_GetTouchEvent(native_xcomponent_, window, &event);
  DCHECK_EQ(ret, 0);

  EventType type;
  switch (event.type) {
    case OH_NATIVEXCOMPONENT_DOWN:
      type = EventType::kTouchPressed;
      break;
    case OH_NATIVEXCOMPONENT_UP:
      type = EventType::kTouchReleased;
      break;
    case OH_NATIVEXCOMPONENT_MOVE:
      type = EventType::kTouchMoved;
      break;
    default:
      LOG(ERROR) << "EEEE event.type=" << static_cast<int>(event.type);
      return;
  }

  PointerDetails details(EventPointerType::kTouch, event.id);
  gfx::PointF location(event.x, event.y);
  gfx::PointF root_location(event.screenX, event.screenY);
  TouchEvent touch_event(type, location, root_location, base::TimeTicks::Now(),
                         details, 0);
  delegate_->DispatchEvent(&touch_event);
}

}  // namespace ui::ohos
