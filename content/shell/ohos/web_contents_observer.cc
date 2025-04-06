#include "content/shell/ohos/web_contents_observer.h"

#include "base/logging.h"
#include "content/shell/ohos/napi_manager.h"

namespace content::ohos {

WebContentsObserver::WebContentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebContentsObserver::~WebContentsObserver() = default;

void WebContentsObserver::OnFocusChangedInPage(FocusedNodeDetails* details) {}

void WebContentsObserver::NavigationStateChanged(
    WebContents* source,
    InvalidateTypes changed_flags) {
  if (changed_flags & INVALIDATE_TYPE_URL) {
    auto* manager = NapiManager::GetInstance();
    manager->SetAddressBarURL(source->GetVisibleURL());
  }
}

}  // namespace content::ohos
