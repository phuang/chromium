#include "ui/gfx/ohos/scoped_oh_native_window.h"

#include "base/check_op.h"

namespace ui {

ScopedOHNativeWindow::ScopedOHNativeWindow() = default;

ScopedOHNativeWindow::ScopedOHNativeWindow(OHNativeWindow* native_window)
    : native_window_(native_window) {}

ScopedOHNativeWindow::~ScopedOHNativeWindow() {
  Reset();
}

ScopedOHNativeWindow::ScopedOHNativeWindow(ScopedOHNativeWindow&& other)
    : ScopedOHNativeWindow(other.Release()) {}

const ScopedOHNativeWindow& ScopedOHNativeWindow::operator=(
    ScopedOHNativeWindow&& other) {
  Reset();
  native_window_ = other.Release();
  return *this;
}

void ScopedOHNativeWindow::Reset() {
  if (native_window_) {
    // OH_NativeWindow_DestroyNativeWindow(native_window_);
    native_window_ = nullptr;
  }
}

OHNativeWindow* ScopedOHNativeWindow::Release() {
  OHNativeWindow* ret = native_window_;
  native_window_ = nullptr;
  return ret;
}

}  // namespace ui
