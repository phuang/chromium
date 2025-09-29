#include "components/viz/service/display_embedder/surface_control_api.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace viz {

// static
const SurfaceControlAPI& SurfaceControlAPI::GetInstance() {
  static SurfaceControlAPI instance;
  return instance;
}

SurfaceControlAPI::SurfaceControlAPI() {
  const char kLibRenderServiceClient[] =
      "/system/lib64/librender_service_client.z.so";
  handle_ = dlopen(kLibRenderServiceClient, RTLD_LAZY);
  if (!handle_) {
    LOG(ERROR) << "Failed to open librenderservice_client.so: " << dlerror();
    return;
  }

#define LOAD_FUNCTION(name)                                             \
  name = reinterpret_cast<OH_##name##_Fn>(dlsym(handle_, "OH_" #name)); \
  if (!name) {                                                          \
    LOG(ERROR) << "Failed to load symbol OH_" #name ": " << dlerror();  \
  }
  SURFACE_CONTROL_FUNCTION_NAMES(LOAD_FUNCTION)
#undef LOAD_FUNCTION
}

SurfaceControlAPI::~SurfaceControlAPI() {
  if (handle_) {
    dlclose(handle_);
    handle_ = nullptr;
  }
}

}  // namespace viz
