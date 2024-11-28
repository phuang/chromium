// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_ANDROID_H_
#define BASE_BASE_PATHS_ANDROID_H_

// This file declares Android-specific path keys for the base module.
// These can be used with the PathService to access various special
// directories and files.

namespace base {

enum {
  PATH_OHOS_START = 500,

  DIR_OHOS_APP_DATA,  // Directory where to put Android app's data.

  PATH_OHOS_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_ANDROID_H_
