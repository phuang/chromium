// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_non_backed.h"

namespace ui {

Clipboard* Clipboard::Create() {
  // TODO
  return new ClipboardNonBacked;
}

}  // namespace ui
