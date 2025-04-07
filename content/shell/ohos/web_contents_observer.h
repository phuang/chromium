#ifndef _CONTENT_SHELL_OHOS_WEB_CONTENTS_OBSERVER_H
#define _CONTENT_SHELL_OHOS_WEB_CONTENTS_OBSERVER_H

#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content::ohos {

class WebContentsObserver : public content::WebContentsDelegate,
                            public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* web_contents);
  ~WebContentsObserver() override;

 private:
  // override
  void OnFocusChangedInPage(FocusedNodeDetails* details) override;
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override;

  void ShowKeyboard();
};

}  // namespace content::ohos

#endif
