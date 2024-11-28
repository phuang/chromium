// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OHOS_UI_OHOS_EXPORT_H_
#define UI_OHOS_UI_OHOS_EXPORT_H_

// Defines UI_ANDROID_EXPORT so that functionality implemented by the UI module
// can be exported to consumers.

#if defined(COMPONENT_BUILD)

#if defined(WIN32)
#error Unsupported target architecture.
#else  // !defined(WIN32)

#if defined(UI_OHOS_IMPLEMENTATION)
#define UI_OHOS_EXPORT __attribute__((visibility("default")))
#else
#define UI_OHOS_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define UI_OHOS_EXPORT

#endif

#endif  // UI_OHOS_UI_OHOS_EXPORT_H_
