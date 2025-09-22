#include "content/shell/ohos/shell_main_delegate.h"

#include "ui/ohos/display_manager.h"

namespace content::ohos {

ShellMainDelegate::ShellMainDelegate(OH_NativeXComponent* native_xcomponent)
    : native_xcomponent_(native_xcomponent) {}

ShellMainDelegate::~ShellMainDelegate() = default;

std::optional<int> ShellMainDelegate::PostEarlyInitialization(InvokedIn invoked_in) {
  auto ret = Base::PostEarlyInitialization(invoked_in);

  // Create DisplayManager in PreBrowserMain(), becasue DisplayManager cannot be
  // created until the UI message loop is initialized.
  display_manager_ =
      std::make_unique<ui::ohos::DisplayManager>(native_xcomponent_);

  DCHECK_EQ(display::Screen::GetScreen(), nullptr);
  display::Screen::SetScreenInstance(display_manager_.get());

  return ret;
}


}  // namespace content::ohos
