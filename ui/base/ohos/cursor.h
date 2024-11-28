// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_OHOS_CURSOR_H_
#define UI_BASE_OHOS_CURSOR_H_

#include "base/component_export.h"
// #include "base/win/windows_types.h"
#include "ui/base/cursor/platform_cursor.h"

template <class T>
class scoped_refptr;

namespace ui::ohos {

// Ref counted class to hold a Windows cursor, i.e. an HCURSOR. Clears the
// resources on destruction.
class COMPONENT_EXPORT(UI_BASE) Cursor : public PlatformCursor {
 public:
  static scoped_refptr<Cursor> FromPlatformCursor(
      scoped_refptr<PlatformCursor> platform_cursor);

  explicit Cursor(bool should_destroy = false);
  Cursor(const Cursor&) = delete;
  Cursor& operator=(const Cursor&) = delete;

 private:
  friend class base::RefCounted<Cursor>;
  ~Cursor() override;

  // Release the cursor on deletion. To be used by custom image cursors.
  bool should_destroy_;
};

}  // namespace ui::ohos

#endif  // UI_BASE_OHOS_CURSOR_H_
