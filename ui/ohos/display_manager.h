// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OHOS_DISPLAY_OHOS_MANAGER_H_
#define UI_OHOS_DISPLAY_OHOS_MANAGER_H_

#include <ace/xcomponent/native_interface_xcomponent.h>

#include <optional>

#include "base/threading/thread_checker.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ohos/ui_ohos_export.h"

namespace ui::ohos {

class UI_OHOS_EXPORT DisplayManager : public display::ScreenBase {
 public:
  explicit DisplayManager(OH_NativeXComponent* native_xcomponent);
  ~DisplayManager() override;

  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;

  // Screen interface.
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  std::optional<float> GetPreferredScaleFactorForView(
      gfx::NativeView view) const override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;

  OH_NativeXComponent* native_xcomponent() const { return native_xcomponent_; }

 private:
  friend class WindowAndroid;

  void UpdateDisplay(uint64_t display_id);

  static void OnDisplayChanged(uint64_t display_id);

  base::ThreadChecker thread_checker_;
  OH_NativeXComponent* const native_xcomponent_;
  uint64_t default_display_id_;
  uint32_t callback_listner_index_;
};

}  // namespace ui::ohos

#endif  // UI_OHOS_DISPLAY_OHOS_MANAGER_H_
