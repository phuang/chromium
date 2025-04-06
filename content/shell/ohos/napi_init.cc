#include <napi/native_api.h>
#include <sys/mman.h>
#include <uv.h>

#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "base/logging.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/ohos/napi.h"
#include "content/shell/ohos/napi_manager.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content::ohos {
namespace {

Napi::Value Navigate(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() != 1) {
    return Napi::Boolean::New(env, false);
  }

  if (!info[0].IsString()) {
    return Napi::Boolean::New(env, false);
  }

  std::string url = info[0].As<Napi::String>();

  LOG(ERROR) << "Hello World from UI thread!";
  DCHECK(!Shell::windows().empty());

  auto* web_contents = Shell::windows()[0]->web_contents();
  web_contents->GetController().LoadURL(
      GURL(url), content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  return env.Undefined();
}

Napi::Value SetController(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() != 1) {
    return Napi::Boolean::New(env, false);
  }

  if (!info[0].IsObject()) {
    return Napi::Boolean::New(env, false);
  }

  Napi::Object obj = info[0].As<Napi::Object>();
  NapiManager::GetInstance()->set_controller(
      Napi::Reference<Napi::Object>::New(obj));

  return Napi::Boolean::New(env, true);
}

}  // namespace

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  logging::SetLogItems(false /* Process ID */, false /* Thread ID */,
                       false /* Timestamp */, false /* Tick count */);

  exports.Set("navigate", Napi::Function::New<Navigate>(env));
  exports.Set("setController", Napi::Function::New<SetController>(env));
  exports.Set("getContext", Napi::Function::New<NapiManager::GetContext>(env));

  NapiManager::GetInstance()->Export(env, exports);
  return exports;
}

}  // namespace content::ohos

using content::ohos::Init;

NODE_API_MODULE(content_view, Init)
