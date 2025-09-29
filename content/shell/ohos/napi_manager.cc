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

#include "content/shell/ohos/napi_manager.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <dlfcn.h>
#include <stdio.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_ohos.h"
#include "base/strings/string_split.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/ohos/shell_main_delegate.h"
#include "content/shell/ohos/web_contents_observer.h"
#include "gpu/config/gpu_info.h"
#include "url/gurl.h"

#define ENABLE_GRAPHITE 1

namespace content::ohos {
namespace {

// clang-format off
// const char* kURLs[] = {
//     "https://webkit.org/blog-files/3d-transforms/poster-circle.html",
//     "https://webkit.org/demos/webgpu",
//     "https://webglsamples.org",
//     "https://webglsamples.org/aquarium/aquarium.html",
//     "https://www.taobao.com",
//     "https://www.amazon.ca",
//     "https://www.cnn.com",
//     "https://browserbench.org/MotionMark1.3/",
//     "https://browserbench.org/MotionMark1.3/developer.html",
//     "https://developer.mozilla.org/en-US/docs/Web/HTML/Element/video",
//     "https://www.youtube.com/watch?v=gsiAYjyiIBM",
//     "https://download.blender.org/peach/bigbuckbunny_movies/BigBuckBunny_640x360.m4v",
//     "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_30MB.mp4",
//     "https://webgpu.github.io/webgpu-samples/sample/texturedCube",
//     "https://zh.wikipedia.org/wiki/%E5%8D%8E%E4%B8%BA",
// };
// clang-format on

enum class ContextType {
  APP_LIFECYCLE = 0,
  JS_PAGE_LIFECYCLE,
};

const char kCommandLinePath[] = "/dev/shm/commandline.txt";
const char kGPUInfoPath[] = "/dev/shm/gpuinfo.txt";

std::vector<std::string> read_command_line() {
  std::ifstream file(kCommandLinePath);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open command line file: " << kCommandLinePath;
    return {};
  }

  std::vector<std::string> result;

  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      // Remove characters after '#'
      size_t pos = line.find('#');
      if (pos != std::string::npos) {
        line.erase(pos);
      }

      // Remote spaces and tabs before and after the string
      line.erase(0, line.find_first_not_of(" \t\r"));
      line.erase(line.find_last_not_of(" \t") + 1);

      // skip empty line
      if (line.empty()) {
        continue;
      }

      auto args = base::SplitString(line, " \t", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY);
      // append args to result
      result.insert(result.end(), std::make_move_iterator(args.begin()),
                    std::make_move_iterator(args.end()));
    }
  }
  return result;
}

}  // namespace

class NapiManager::GpuDataManagerObserverImpl
    : public content::GpuDataManagerObserver {
 public:
  GpuDataManagerObserverImpl() {
    content::GpuDataManager::GetInstance()->AddObserver(this);
  }

  ~GpuDataManagerObserverImpl() override {
    content::GpuDataManager::GetInstance()->RemoveObserver(this);
  }

  void OnGpuInfoUpdate() override {
    LOG(ERROR) << "EEEE OnGpuInfoUpdate";
    auto* manager = content::GpuDataManager::GetInstance();
    auto gpu_info = manager->GetGPUInfo();
    // auto gpu_feature_info = manager->GetGpuFeatureInfo();

    std::ofstream file(kGPUInfoPath);
    if (!file.is_open()) {
      LOG(ERROR) << "Failed to open GPU info file: " << kGPUInfoPath;
      return;
    }

    const char* FeatureName[] = {
        "GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS",
        "GPU_FEATURE_TYPE_ACCELERATED_WEBGL",
        "GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE",
        "GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE",
        "GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION",
        "GPU_FEATURE_TYPE_ACCELERATED_WEBGL2",
        "GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL",
        "GPU_FEATURE_TYPE_ACCELERATED_GL",
        "GPU_FEATURE_TYPE_VULKAN",
        "GPU_FEATURE_TYPE_ACCELERATED_WEBGPU",
        "GPU_FEATURE_TYPE_SKIA_GRAPHITE",
        "GPU_FEATURE_TYPE_WEBNN",
    };

    file << "GPU Feature Info:\n";
    for (size_t i = 0; i < std::size(FeatureName); i++) {
      bool enabled =
          manager->GetFeatureStatus(static_cast<gpu::GpuFeatureType>(i)) ==
          gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled;
      file << "  " << FeatureName[i] << ": "
           << (enabled ? "Enabled" : "Disabled") << "\n";
    }

    file << "GL Info:\n";
    file << "  Vendor: " << gpu_info.gl_vendor << "\n";
    file << "  Renderer: " << gpu_info.gl_renderer << "\n";
    file << "  Version: " << gpu_info.gl_version << "\n";
    file << "  Extensions: " << gpu_info.gl_extensions << "\n";
  }
};

// static
NapiManager* NapiManager::GetInstance() {
  static NapiManager manager;
  return &manager;
}

NapiManager::NapiManager() = default;

NapiManager::~NapiManager() = default;

// static
Napi::Value NapiManager::GetContext(const Napi::CallbackInfo& info) {
  LOG(ERROR) << "NapiManager::GetContext()";
  Napi::Env env = info.Env();

  if (info.Length() != 1) {
    Napi::Error::New(env, "Wrong number of arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsNumber()) {
    Napi::Error::New(env, "Wrong arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  int64_t value = info[0].As<Napi::Number>().Int64Value();

  auto exports = Napi::Object::New(env);

  switch (static_cast<ContextType>(value)) {
    case ContextType::APP_LIFECYCLE: {
      /* AppInit对应EntryAbility.ts中的应用生命周期: onCreate, onShow, onHide,
       * onDestroy */
      LOG(ERROR) << "NapiManager::GetContext() APP_LIFECYCLE";

      exports.Set("onCreate",
                  Napi::Function::New<NapiManager::NapiOnCreate>(env));
      exports.Set("onShow", Napi::Function::New<NapiManager::NapiOnShow>(env));
      exports.Set("onHide", Napi::Function::New<NapiManager::NapiOnHide>(env));
      exports.Set("onDestroy",
                  Napi::Function::New<NapiManager::NapiOnDestroy>(env));
    }

    break;
    case ContextType::JS_PAGE_LIFECYCLE: {
      /* JS Page */
      LOG(ERROR) << "NapiManager::GetContext() JS_PAGE_LIFECYCLE";

      exports.Set("onPageShow",
                  Napi::Function::New<NapiManager::NapiOnPageShow>(env));
      exports.Set("onPageHide",
                  Napi::Function::New<NapiManager::NapiOnPageHide>(env));
    } break;
    default: {
      LOG(ERROR) << "unknown type";
      break;
    }
  }
  return exports;
}

bool NapiManager::Export(Napi::Env env, Napi::Object exports) {
  LOG(ERROR) << "NapiManager::Export()";
  if (env_ == nullptr) {
    env_ = env;
  }

  DCHECK_EQ(env_, env);

  if (!exports.Has(OH_NATIVE_XCOMPONENT_OBJ)) {
    LOG(ERROR) << "NapiManager::Export() cannot get xcomponent obj";
    return false;
  }

  auto export_instance =
      exports.Get(OH_NATIVE_XCOMPONENT_OBJ).As<Napi::Object>();
  auto* native_xcomponent =
      Napi::ObjectWrap<OH_NativeXComponent>::Unwrap(export_instance);
  if (!native_xcomponent) {
    LOG(ERROR) << "NapiManager::Export() cannot unwrap xcomponent obj";
    return false;
  }

  char buf[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
  uint64_t size = OH_XCOMPONENT_ID_LEN_MAX + 1;

  int32_t ret =
      OH_NativeXComponent_GetXComponentId(native_xcomponent, buf, &size);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOG(ERROR) << "NapiManager::Export() cannot get xcomponent id";
    return false;
  }

  RunContentMain(native_xcomponent);

  return true;
}

void NapiManager::OnCreateNative(Napi::Env env) {
  LOG(ERROR) << "NapiManager::OnCreateNative()";
}

void NapiManager::OnShowNative() {
  LOG(ERROR) << "NapiManager::OnShowNative()";
}

void NapiManager::OnHideNative() {
  LOG(ERROR) << "NapiManager::OnHideNative()";
}

void NapiManager::OnDestroyNative() {
  LOG(ERROR) << "NapiManager::OnDestroyNative()";
}

void NapiManager::OnPageShowNative() {
  LOG(ERROR) << "NapiManager::OnPageShowNative()";
}

void NapiManager::OnPageHideNative() {
  LOG(ERROR) << "NapiManager::OnPageHideNative()";
}

// static
Napi::Value NapiManager::NapiOnCreate(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  NapiManager::GetInstance()->OnCreateNative(env);
  return env.Null();
}

// static
Napi::Value NapiManager::NapiOnShow(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  NapiManager::GetInstance()->OnShowNative();
  return env.Null();
}

// static
Napi::Value NapiManager::NapiOnHide(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  NapiManager::GetInstance()->OnHideNative();
  return env.Null();
}

// static
Napi::Value NapiManager::NapiOnDestroy(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  NapiManager::GetInstance()->OnDestroyNative();
  return env.Null();
}

// static
Napi::Value NapiManager::NapiOnPageShow(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  NapiManager::GetInstance()->OnPageShowNative();
  return env.Null();
}

// static
Napi::Value NapiManager::NapiOnPageHide(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  NapiManager::GetInstance()->OnPageHideNative();
  return env.Null();
}

bool NapiManager::RunContentMain(OH_NativeXComponent* native_xcomponent) {
  LOG(ERROR) << "NapiManager::CreateContentMainThread()";
  if (content_main_delegate_) {
    return true;
  }

  std::vector<const char*> args = {
      "content_shell",
      "--single-process",
      "--no-sandbox",
      // Disable v8 jit
      // "--jitless",
      "--enable-unsafe-webgpu",
      // "--force-device-scale-factor=1.5",
      // TODO: support video capture on OHOS
      "--use-fake-device-for-media-stream",
  };

  auto param_args = read_command_line();
  LOG(ERROR) << "EEEE  param_args.size(): " << param_args.size();
  for (const auto& arg : param_args) {
    args.push_back(arg.c_str());
    LOG(ERROR) << "EEEE  arg: " << arg;
  }

  base::MessagePumpOHOS::SetNapiEnv(env_);
  content_main_delegate_ =
      std::make_unique<ShellMainDelegate>(native_xcomponent);
  content_main_runner_ = ContentMainRunner::Create();

  ContentMainParams params(content_main_delegate_.get());
  params.argv = args.data();
  params.argc = args.size();
  RunContentProcess(std::move(params), content_main_runner_.get());

  gpu_data_manager_observer_impl_ =
      std::make_unique<GpuDataManagerObserverImpl>();

  DCHECK(!Shell::windows().empty());
  auto* shell = Shell::windows()[0];

  web_contents_observer_ =
      std::make_unique<WebContentsObserver>(shell->web_contents());

  return true;
}

void NapiManager::SetAddressBarURL(const GURL& url) {
  auto controller = controller_.Value();
  if (!controller.IsObject()) {
    return;
  }

  auto callback = controller.Get("onAddressChanged");
  if (!callback.IsFunction()) {
    LOG(ERROR) << "controller.onAddressChanged is not a function";
    return;
  }

  callback.As<Napi::Function>().Call(
      {Napi::String::New(env_, url.GetContent())});
}

}  // namespace content::ohos
