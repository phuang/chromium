// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_OHOS_H_
#define COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_OHOS_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

namespace input {

class COMPONENT_EXPORT(INPUT) WebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(const void* motion_event,
                                    blink::WebInputEvent::Type type,
                                    int click_count,
                                    int action_button);
};

class COMPONENT_EXPORT(INPUT) WebMouseWheelEventBuilder {
 public:
  static blink::WebMouseWheelEvent Build(const void* motion_event);
};

class COMPONENT_EXPORT(INPUT) WebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(const void* ohos_key_event,
                                       blink::WebInputEvent::Type type,
                                       int modifiers,
                                       base::TimeTicks time,
                                       int keycode,
                                       int scancode,
                                       int unicode_character,
                                       bool is_system_key);
};

class COMPONENT_EXPORT(INPUT) WebGestureEventBuilder {
 public:
  static blink::WebGestureEvent Build(blink::WebInputEvent::Type type,
                                      base::TimeTicks time,
                                      float x,
                                      float y);
};

}  // namespace input

#endif  // COMPONENTS_INPUT_WEB_INPUT_EVENT_BUILDERS_OHOS_H_
