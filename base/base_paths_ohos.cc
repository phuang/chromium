// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderAndroid which replaces base::PathProviderPosix for
// Android in base/path_service.cc.

#include <limits.h>
#include <unistd.h>

#include <ostream>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/process/process_metrics.h"

namespace base {
namespace {

const char kAppData[] = "/data/storage/el2/base";
const char kCache[] = "/data/storage/el2/base/cache/web";
const char kModule[] = "/system/lib64";
const char kResFile[] = "/data/storage/el1/bundle/entry/resources/resfile";

}  // namespace

bool PathProviderOHOS(int key, FilePath* result) {
  switch (key) {
    case base::FILE_EXE: {
      FilePath bin_dir;
      if (!ReadSymbolicLink(FilePath(kProcSelfExe), &bin_dir)) {
        // This fails for some devices (maybe custom OEM selinux policy?)
        // https://crbug.com/1416753
        LOG(ERROR) << "Unable to resolve " << kProcSelfExe << ".";
        return false;
      }
      *result = bin_dir;
      return true;
    }
    case base::FILE_MODULE:
      NOTIMPLEMENTED() << " FILE_MODULE";
      return false;
    case base::DIR_MODULE:
      NOTIMPLEMENTED() << " DIR_MODULE";
      *result = FilePath(kModule);
      return true;
    case base::DIR_SRC_TEST_DATA_ROOT:
      NOTIMPLEMENTED() << " DIR_SRC_TEST_DATA_ROOT";
      return false;
    case base::DIR_OUT_TEST_DATA_ROOT:
      NOTIMPLEMENTED() << " DIR_OUT_TEST_DATA_ROOT";
      return false;
    case base::DIR_USER_DESKTOP:
      NOTIMPLEMENTED() << " DIR_USER_DESKTOP";
      return false;
    case base::DIR_CACHE:
      *result = FilePath(kCache);
      return true;
    case base::DIR_ASSETS:
      *result = FilePath(kResFile);
      return true;
    case base::DIR_OHOS_APP_DATA:
      *result = FilePath(kAppData);
      return true;
  }

  // For all other keys, let the PathService fall back to a default, if defined.
  return false;
}

}  // namespace base
