#ifndef _CONTENT_SHELL_OHOS_SHELL_MAIN_DELEGATE_H
#define _CONTENT_SHELL_OHOS_SHELL_MAIN_DELEGATE_H

#include "content/shell/app/shell_main_delegate.h"

struct OH_NativeXComponent;

namespace ui::ohos {
class DisplayManager;
}

namespace content::ohos {

class ShellMainDelegate : public content::ShellMainDelegate {
 public:
  using Base = content::ShellMainDelegate;
  ShellMainDelegate(OH_NativeXComponent* native_xcomponent);
  ~ShellMainDelegate() override;

 protected:
  // ContentMainDelegate:
  std::optional<int> PreBrowserMain() override;

 private:
  OH_NativeXComponent* const native_xcomponent_;
  std::unique_ptr<ui::ohos::DisplayManager> display_manager_;
};

}  // namespace content::ohos

#endif  // _CONTENT_SHELL_OHOS_SHELL_MAIN_DELEGATE_H
