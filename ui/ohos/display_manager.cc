// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ohos/display_manager.h"

#include <window_manager/oh_display_manager.h>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "ui/display/display.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/icc_profile.h"

namespace ui::ohos {
namespace {

display::Display::Rotation ToRotation(NativeDisplayManager_Rotation rotation) {
  static_assert(
      display::Display::Rotation::ROTATE_0 ==
      static_cast<display::Display::Rotation>(DISPLAY_MANAGER_ROTATION_0));
  static_assert(
      display::Display::Rotation::ROTATE_90 ==
      static_cast<display::Display::Rotation>(DISPLAY_MANAGER_ROTATION_90));
  static_assert(
      display::Display::Rotation::ROTATE_180 ==
      static_cast<display::Display::Rotation>(DISPLAY_MANAGER_ROTATION_180));
  static_assert(
      display::Display::Rotation::ROTATE_270 ==
      static_cast<display::Display::Rotation>(DISPLAY_MANAGER_ROTATION_270));
  return static_cast<display::Display::Rotation>(rotation);
}

}  // namespace

using display::Display;
using display::DisplayList;

DisplayManager::DisplayManager(OH_NativeXComponent* native_xcomponent)
    : native_xcomponent_(native_xcomponent) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto result =
      OH_NativeDisplayManager_GetDefaultDisplayId(&default_display_id_);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  result = OH_NativeDisplayManager_RegisterDisplayChangeListener(
      DisplayManager::OnDisplayChanged, &callback_listner_index_);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  UpdateDisplay(default_display_id_);
}

DisplayManager::~DisplayManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto result = OH_NativeDisplayManager_UnregisterDisplayChangeListener(
      callback_listner_index_);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);
}

Display DisplayManager::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetPrimaryDisplay();
}

Display DisplayManager::GetDisplayNearestView(gfx::NativeView view) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

Display DisplayManager::GetDisplayNearestPoint(const gfx::Point& point) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

Display DisplayManager::GetDisplayMatching(const gfx::Rect& match_rect) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

std::optional<float> DisplayManager::GetPreferredScaleFactorForView(
    gfx::NativeView view) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDisplayNearestView(view).device_scale_factor();
}

bool DisplayManager::IsWindowUnderCursor(gfx::NativeWindow window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

void DisplayManager::UpdateDisplay(uint64_t display_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(default_display_id_, display_id);
  NativeDisplayManager_ErrorCode result;

  int32_t width = 0;
  int32_t height = 0;
  result = OH_NativeDisplayManager_GetDefaultDisplayWidth(&width);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);
  result = OH_NativeDisplayManager_GetDefaultDisplayHeight(&height);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  NativeDisplayManager_Rotation rotation = DISPLAY_MANAGER_ROTATION_0;
  result = OH_NativeDisplayManager_GetDefaultDisplayRotation(&rotation);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  float virtual_pixels = 0;
  result = OH_NativeDisplayManager_GetDefaultDisplayVirtualPixelRatio(
      &virtual_pixels);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  uint32_t refresh_rate = 60;
  result = OH_NativeDisplayManager_GetDefaultDisplayRefreshRate(&refresh_rate);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  int32_t density_dpi = 0;
  result = OH_NativeDisplayManager_GetDefaultDisplayDensityDpi(&density_dpi);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  float density_pixels = 1.0f;
  result =
      OH_NativeDisplayManager_GetDefaultDisplayDensityPixels(&density_pixels);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  float scaled_density = 1.0f;
  result =
      OH_NativeDisplayManager_GetDefaultDisplayScaledDensity(&scaled_density);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  float x_dpi = 0.0f;
  result = OH_NativeDisplayManager_GetDefaultDisplayDensityXdpi(&x_dpi);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  float y_dpi = 0.0f;
  result = OH_NativeDisplayManager_GetDefaultDisplayDensityYdpi(&y_dpi);
  DCHECK_EQ(result, DISPLAY_MANAGER_OK);

  display::Display display(display_id);
  display.SetScaleAndBounds(scaled_density, gfx::Rect(0, 0, width, height));
  display.set_rotation(ToRotation(rotation));

  ProcessDisplayChanged(display, /*is_primary=*/true);
}

// static
void DisplayManager::OnDisplayChanged(uint64_t display_id) {
  auto* display_manager =
      static_cast<DisplayManager*>(display::Screen::GetScreen());
  display_manager->UpdateDisplay(display_id);
}

}  // namespace ui::ohos
