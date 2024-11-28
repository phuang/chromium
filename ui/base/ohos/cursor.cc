// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ohos/cursor.h"

#include "base/memory/scoped_refptr.h"

namespace ui::ohos {

// static
scoped_refptr<Cursor> Cursor::FromPlatformCursor(
    scoped_refptr<PlatformCursor> platform_cursor) {
  return base::WrapRefCounted(static_cast<Cursor*>(platform_cursor.get()));
}

Cursor::Cursor(bool should_destroy) : should_destroy_(should_destroy) {}

Cursor::~Cursor() {}

}  // namespace ui::ohos
