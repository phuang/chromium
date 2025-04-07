// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_OHOS_WINDOW_EXPORT_H_
#define UI_PLATFORM_WINDOW_OHOS_WINDOW_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(OHOS_WINDOW_IMPLEMENTATION)
#define OHOS_WINDOW_EXPORT __declspec(dllexport)
#else
#define OHOS_WINDOW_EXPORT __declspec(dllimport)
#endif  // defined(OHOS_WINDOW_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(OHOS_WINDOW_IMPLEMENTATION)
#define OHOS_WINDOW_EXPORT __attribute__((visibility("default")))
#else
#define OHOS_WINDOW_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define OHOS_WINDOW_EXPORT
#endif

#endif  // UI_PLATFORM_WINDOW_OHOS_WINDOW_EXPORT_H_
