/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CONTENT_SHELL_OHOS_NAPI_MANAGER_H_
#define CONTENT_SHELL_OHOS_NAPI_MANAGER_H_

#define NODE_ADDON_API_DISABLE_DEPRECATED
#include <napi/native_api.h>

#include <string>
#include <unordered_map>

#include "content/shell/ohos/napi.h"

struct OH_NativeXComponent;

class GURL;

namespace content {

class ContentMainRunner;

namespace ohos {

class ShellMainDelegate;
class WebContentsObserver;

class NapiManager {
 public:
  static NapiManager* GetInstance();

  // Napi export
  bool Export(Napi::Env env, Napi::Object exports);

  static Napi::Value GetContext(const Napi::CallbackInfo& info);

  void SetAddressBarURL(const GURL& url);

  void set_controller(Napi::Reference<Napi::Object> controller) {
    controller_ = std::move(controller);
  }

 private:
  NapiManager();
  ~NapiManager();

  /**APP Lifecycle**/
  static Napi::Value NapiOnCreate(const Napi::CallbackInfo& info);
  static Napi::Value NapiOnShow(const Napi::CallbackInfo& info);
  static Napi::Value NapiOnHide(const Napi::CallbackInfo& info);
  static Napi::Value NapiOnDestroy(const Napi::CallbackInfo& info);

  void OnCreateNative(Napi::Env env);
  void OnShowNative();
  void OnHideNative();
  void OnDestroyNative();

  /**JS Page Lifecycle**/
  static Napi::Value NapiOnPageShow(const Napi::CallbackInfo& info);
  static Napi::Value NapiOnPageHide(const Napi::CallbackInfo& info);

  void OnPageShowNative();
  void OnPageHideNative();

  bool RunContentMain(OH_NativeXComponent* native_xcomponent);

  Napi::Env env_{nullptr};
  std::string id_;
  // std::unordered_map<std::string, AppNapi*> app_napi_map_;
  std::unique_ptr<ShellMainDelegate> content_main_delegate_;
  std::unique_ptr<ContentMainRunner> content_main_runner_;
  std::unique_ptr<WebContentsObserver> web_contents_observer_;

  // Hold a weak reference of the controller
  Napi::Reference<Napi::Object> controller_;
};

}  // namespace ohos
}  // namespace content

#endif  // CONTENT_SHELL_OHOS_NAPI_MANAGER_H_
