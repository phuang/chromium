// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "ui/snapshot/snapshot_async.h"

namespace ui {

// static std::unique_ptr<viz::CopyOutputRequest> CreateCopyRequest(
//     gfx::NativeWindow view,
//     const gfx::Rect& source_rect,
//     viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
//   std::unique_ptr<viz::CopyOutputRequest> request =
//       std::make_unique<viz::CopyOutputRequest>(
//           viz::CopyOutputRequest::ResultFormat::RGBA,
//           viz::CopyOutputRequest::ResultDestination::kSystemMemory,
//           std::move(callback));
//   if (!source_rect.IsEmpty()) {
//     float scale = ui::GetScaleFactorForNativeView(view);
//     float scale = 1.0;
//     request->set_area(gfx::ScaleToEnclosingRect(source_rect, scale));
//   }
//   return request;
// }

static void MakeAsyncCopyRequest(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    std::unique_ptr<viz::CopyOutputRequest> copy_request) {
  // if (!window->GetCompositor())
  //   return;
  // window->GetCompositor()->RequestCopyOfOutputOnRootLayer(
  //     std::move(copy_request));
  NOTIMPLEMENTED();
}

void GrabWindowSnapshotAndScale(gfx::NativeWindow window,
                                const gfx::Rect& source_rect,
                                const gfx::Size& target_size,
                                GrabSnapshotImageCallback callback) {
  // MakeAsyncCopyRequest(
  //     window, source_rect,
  //     CreateCopyRequest(window, source_rect,
  //                       base::BindOnce(&SnapshotAsync::ScaleCopyOutputResult,
  //                                      std::move(callback), target_size)));
  NOTIMPLEMENTED();
}

void GrabWindowSnapshot(gfx::NativeWindow window,
                        const gfx::Rect& source_rect,
                        GrabSnapshotImageCallback callback) {
  // MakeAsyncCopyRequest(
  //     window, source_rect,
  //     CreateCopyRequest(
  //         window, source_rect,
  //         base::BindOnce(&SnapshotAsync::RunCallbackWithCopyOutputResult,
  //                        std::move(callback))));
  NOTIMPLEMENTED();
}

void GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& source_rect,
                      GrabSnapshotImageCallback callback) {
  // std::unique_ptr<viz::CopyOutputRequest> copy_request =
  //     view->MaybeRequestCopyOfView(CreateCopyRequest(
  //         view, source_rect,
  //         base::BindOnce(&SnapshotAsync::RunCallbackWithCopyOutputResult,
  //                        std::move(callback))));
  // if (!copy_request)
  //   return;

  // MakeAsyncCopyRequest(view->GetWindowAndroid(), source_rect,
  //                      std::move(copy_request));
  NOTIMPLEMENTED();
}

}  // namespace ui
