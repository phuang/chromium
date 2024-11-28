#ifndef UI_GFX_OHOS_SCOPED_OH_NATIVE_WINDOW_H_
#define UI_GFX_OHOS_SCOPED_OH_NATIVE_WINDOW_H_

#include <native_window/external_window.h>

#include "ui/gfx/native_widget_types.h"

namespace ui {

class ScopedOHNativeWindow {
 public:
  ScopedOHNativeWindow();
  explicit ScopedOHNativeWindow(gfx::AcceleratedWidget accelerated_widget);
  ScopedOHNativeWindow(OHNativeWindow* native_window);

  ~ScopedOHNativeWindow();

  ScopedOHNativeWindow(const ScopedOHNativeWindow&) = delete;
  const ScopedOHNativeWindow& operator=(const ScopedOHNativeWindow& other) =
      delete;

  ScopedOHNativeWindow(ScopedOHNativeWindow&& other);
  const ScopedOHNativeWindow& operator=(ScopedOHNativeWindow&& other);

  void Reset();

  OHNativeWindow* Release();

  OHNativeWindow* native_window() { return native_window_; }

 private:
  OHNativeWindow* native_window_ = nullptr;
};

}  // namespace ui

#endif  // UI_GFX_OHOS_SCOPED_OH_NATIVE_WINDOW_H_
