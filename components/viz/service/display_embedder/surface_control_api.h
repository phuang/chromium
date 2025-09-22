#ifndef SURFACE_CONTROL_API_H_
#define SURFACE_CONTROL_API_H_

#include "components/viz/service/display_embedder/surface_control.h"

namespace viz {

#define SURFACE_CONTROL_FUNCTION_NAMES(E)           \
  E(SurfaceControl_Acquire)                         \
  E(SurfaceControl_Create)                          \
  E(SurfaceControl_CreateFromDisplay)               \
  E(SurfaceControl_FromNativeWindow)                \
  E(SurfaceControl_FromNodeId)                      \
  E(SurfaceControl_Release)                         \
  E(SurfaceTransactionStats_GetPresentFenceFd)      \
  E(SurfaceTransactionStats_GetSurfaceControls)     \
  E(SurfaceTransactionStats_ReleaseSurfaceControls) \
  E(SurfaceTransaction_Commit)                      \
  E(SurfaceTransaction_Create)                      \
  E(SurfaceTransaction_Delete)                      \
  E(SurfaceTransaction_Reparent)                    \
  E(SurfaceTransaction_SetBackgroundColor)          \
  E(SurfaceTransaction_SetBorderColor)              \
  E(SurfaceTransaction_SetBorderStyle)              \
  E(SurfaceTransaction_SetBorderWidth)              \
  E(SurfaceTransaction_SetBounds)                   \
  E(SurfaceTransaction_SetBuffer)                   \
  E(SurfaceTransaction_SetBufferAlpha)              \
  E(SurfaceTransaction_SetBufferTransform)          \
  E(SurfaceTransaction_SetCrop)                     \
  E(SurfaceTransaction_SetDamageRegion)             \
  E(SurfaceTransaction_SetForegroundColor)          \
  E(SurfaceTransaction_SetFrame)                    \
  E(SurfaceTransaction_SetFrameGravity)             \
  E(SurfaceTransaction_SetHardwareEnableHint)       \
  E(SurfaceTransaction_SetName)                     \
  E(SurfaceTransaction_SetOnCommit)                 \
  E(SurfaceTransaction_SetOnComplete)               \
  E(SurfaceTransaction_SetPivot)                    \
  E(SurfaceTransaction_SetPosition)                 \
  E(SurfaceTransaction_SetScale)                    \
  E(SurfaceTransaction_SetTranslate)                \
  E(SurfaceTransaction_SetVisibility)               \
  E(SurfaceTransaction_SetZOrder)

class SurfaceControlAPI {
 public:
  static const SurfaceControlAPI& GetInstance();

#define DECLARE_FUNCTION_TYPE(name) using OH_##name##_Fn = decltype(&OH_##name);
  SURFACE_CONTROL_FUNCTION_NAMES(DECLARE_FUNCTION_TYPE)
#undef DECLARE_FUNCTION_TYPE

#define DECLARE_FUNCTION(name) OH_##name##_Fn name = nullptr;
  SURFACE_CONTROL_FUNCTION_NAMES(DECLARE_FUNCTION)
#undef DECLARE_FUNCTION

 private:
  SurfaceControlAPI();
  ~SurfaceControlAPI();

  void* handle_ = nullptr;
};

}  // namespace viz
#endif  // SURFACE_CONTROL_API_H_
