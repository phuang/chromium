// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_ohos.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
// #include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// This class holds strongly so we don't leak that in the header of the
// WebContentsViewOHOS.
class WebContentsUIViewHolder {
 public:
  // UIScrollView* __strong view_;
};

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* render_view_host_delegate_view) {
  auto rv =
      std::make_unique<WebContentsViewOHOS>(web_contents, std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

WebContentsViewOHOS::WebContentsViewOHOS(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : web_contents_(web_contents), delegate_(std::move(delegate)) {
  // ui_view_ = std::make_unique<WebContentsUIViewHolder>();
  // ui_view_->view_ = [[UIScrollView alloc] init];
  // [ui_view_->view_ setScrollEnabled:NO];
  // [ui_view_->view_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
  //                                      UIViewAutoresizingFlexibleHeight];
}

WebContentsViewOHOS::~WebContentsViewOHOS() {}

gfx::NativeView WebContentsViewOHOS::GetNativeView() const {
  return const_cast<gfx::NativeView>(&view_);
}

gfx::NativeView WebContentsViewOHOS::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    return gfx::NativeView();
  }
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewOHOS::GetTopLevelNativeWindow() const {
  gfx::NativeView view = GetContentNativeView();
  if (!view) {
    return gfx::NativeWindow();
  }
  NOTIMPLEMENTED();
  return gfx::NativeWindow();
  // return gfx::NativeWindow(view.Get().window);
}

gfx::Rect WebContentsViewOHOS::GetContainerBounds() const {
  return gfx::Rect();
}

void WebContentsViewOHOS::OnCapturerCountChanged() {}

void WebContentsViewOHOS::FullscreenStateChanged(bool is_fullscreen) {}

void WebContentsViewOHOS::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {}

void WebContentsViewOHOS::Focus() {
  if (delegate_) {
    delegate_->ResetStoredFocus();
  }

  // Focus the the fullscreen view, if one exists; otherwise, focus the content
  // native view. This ensures that the view currently attached to a NSWindow is
  // being used to query or set first responder state.
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    return;
  }

  static_cast<RenderWidgetHostViewBase*>(rwhv)->Focus();
}

void WebContentsViewOHOS::SetInitialFocus() {
  if (delegate_) {
    delegate_->ResetStoredFocus();
  }

  if (web_contents_->FocusLocationBarByDefault()) {
    web_contents_->SetFocusToLocationBar();
  } else {
    Focus();
  }
}

void WebContentsViewOHOS::StoreFocus() {
  if (delegate_) {
    delegate_->StoreFocus();
  }
}

void WebContentsViewOHOS::RestoreFocus() {
  if (delegate_ && delegate_->RestoreFocus()) {
    return;
  }

  // Fall back to the default focus behavior if we could not restore focus.
  // TODO(shess): If location-bar gets focus by default, this will
  // select-all in the field.  If there was a specific selection in
  // the field when we navigated away from it, we should restore
  // that selection.
  SetInitialFocus();
}

void WebContentsViewOHOS::FocusThroughTabTraversal(bool reverse) {
  if (delegate_) {
    delegate_->ResetStoredFocus();
  }

  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewOHOS::GetDropData() const {
  return nullptr;
}

gfx::Rect WebContentsViewOHOS::GetViewBounds() const {
  return gfx::Rect(view_.GetSize());
}

void WebContentsViewOHOS::GotFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewOHOS::LostFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

void WebContentsViewOHOS::ShowContextMenu(RenderFrameHost& render_frame_host,
                                          const ContextMenuParams& params) {
  if (delegate_) {
    delegate_->ShowContextMenu(render_frame_host, params);
  } else {
    DLOG(ERROR) << "Cannot show context menus without a delegate.";
  }
}

void WebContentsViewOHOS::CreateView(gfx::NativeView context) {}

RenderWidgetHostViewBase* WebContentsViewOHOS::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  NOTIMPLEMENTED();
  // return new RenderWidgetHostViewOHOS(render_widget_host);
  return nullptr;
}

RenderWidgetHostViewBase* WebContentsViewOHOS::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  NOTIMPLEMENTED();
  // return new RenderWidgetHostViewOHOS(render_widget_host);
  return nullptr;
}

void WebContentsViewOHOS::SetPageTitle(const std::u16string& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void WebContentsViewOHOS::RenderViewReady() {}

void WebContentsViewOHOS::RenderViewHostChanged(RenderViewHost* old_host,
                                                RenderViewHost* new_host) {
  // ScopedCAActionDisabler disabler;
  // if (old_host) {
  //   auto* rwhv = old_host->GetWidget()->GetView();
  //   if (rwhv && rwhv->GetNativeView()) {
  //     static_cast<RenderWidgetHostViewOHOS*>(rwhv)->UpdateNativeViewTree(
  //         gfx::NativeView());
  //   }
  // }

  // auto* rwhv = new_host->GetWidget()->GetView();
  // if (rwhv && rwhv->GetNativeView()) {
  //   static_cast<RenderWidgetHostViewOHOS*>(rwhv)->UpdateNativeViewTree(
  //       GetNativeView());
  // }
  // web_contents_->UpdateBrowserControlsState(cc::BrowserControlsState::kBoth,
  //                                           cc::BrowserControlsState::kHidden,
  //                                           false, std::nullopt);
  NOTIMPLEMENTED();
}

void WebContentsViewOHOS::SetOverscrollControllerEnabled(bool enabled) {}

int WebContentsViewOHOS::GetTopControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsHeight() : 0;
}

int WebContentsViewOHOS::GetTopControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsMinHeight() : 0;
}

int WebContentsViewOHOS::GetBottomControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsHeight() : 0;
}

int WebContentsViewOHOS::GetBottomControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsMinHeight() : 0;
}

bool WebContentsViewOHOS::ShouldAnimateBrowserControlsHeightChanges() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->ShouldAnimateBrowserControlsHeightChanges();
}

bool WebContentsViewOHOS::DoBrowserControlsShrinkRendererSize() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate &&
         delegate->DoBrowserControlsShrinkRendererSize(web_contents_);
}

bool WebContentsViewOHOS::OnlyExpandTopControlsAtPageTop() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->OnlyExpandTopControlsAtPageTop();
}

BackForwardTransitionAnimationManager*
WebContentsViewOHOS::GetBackForwardTransitionAnimationManager() {
  return nullptr;
}

}  // namespace content
