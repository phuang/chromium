#ifndef SURFACE_CONTROL_H_
#define SURFACE_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NativeWindow OHNativeWindow;
typedef struct OH_NativeBuffer OH_NativeBuffer;
typedef struct OH_SurfaceControl OH_SurfaceControl;

struct OH_Rect {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};
typedef struct OH_Rect OH_Rect;

// Create the root surface from a node.
OH_SurfaceControl* OH_SurfaceControl_FromNodeId(uint64_t parentNodeId,
                                                const char* name);

// Create the root surface from a native window.
OH_SurfaceControl* OH_SurfaceControl_FromNativeWindow(OHNativeWindow* parent,
                                                      const char* name);

// Create a surface which is not attached to surface tree.
OH_SurfaceControl* OH_SurfaceControl_Create(const char* name);
OH_SurfaceControl* OH_SurfaceControl_CreateFromDisplay(const char* name);

// Increase a ref for a surface
void OH_SurfaceControl_Acquire(OH_SurfaceControl* surface_control);
// Decrease a ref for a surface
void OH_SurfaceControl_Release(OH_SurfaceControl* surface_control);

struct OH_SurfaceTransaction;
typedef struct OH_SurfaceTransaction OH_SurfaceTransaction;

// Create a transaction.
OH_SurfaceTransaction* OH_SurfaceTransaction_Create();

// Destroy a transaction.
void OH_SurfaceTransaction_Delete(OH_SurfaceTransaction* transaction);

// Commit transaction.
void OH_SurfaceTransaction_Commit(OH_SurfaceTransaction* transaction);

/**
 * An opaque handle returned during a callback that can be used to query general
 * stats and stats for surfaces which were either removed or for which buffers
 * were updated after this transaction was applied.
 */
typedef struct OH_SurfaceTransactionStats OH_SurfaceTransactionStats;
/**
 * Since the transactions are applied asynchronously, the
 * OH_SurfaceTransaction_OnComplete callback can be used to be notified when a
 * frame including the updates in a transaction was presented.
 *
 * Buffers which are replaced or removed from the scene in the transaction
 * invoking this callback may be reused after this point.
 *
 * Starting with API level 36, prefer using \a
 * OH_SurfaceTransaction_OnBufferRelease to listen to when a buffer is ready to
 * be reused.
 *
 * \param context Optional context provided by the client that is passed into
 * the callback.
 *
 * \param stats Opaque handle that can be passed to OH_SurfaceTransactionStats
 * functions to query information about the transaction. The handle is only
 * valid during the callback.
 *
 * THREADING
 * The transaction completed callback can be invoked on any thread.
 */
typedef void (*OH_SurfaceTransaction_OnComplete)(void* context,
                                                 uint64_t timestamp);
/**
 * The OH_SurfaceTransaction_OnCommit callback is invoked when transaction is
 * applied and the updates are ready to be presented. This callback will be
 * invoked before the OH_SurfaceTransaction_OnComplete callback.
 *
 * This callback does not mean buffers have been released! It simply means that
 * any new transactions applied will not overwrite the transaction for which we
 * are receiving a callback and instead will be included in the next frame. If
 * you are trying to avoid dropping frames (overwriting transactions), and
 * unable to use timestamps (Which provide a more efficient solution), then this
 * method provides a method to pace your transaction application.
 *
 * \param context Optional context provided by the client that is passed into
 * the callback.
 *
 * \param stats Opaque handle that can be passed to OH_SurfaceTransactionStats
 * functions to query information about the transaction. The handle is only
 * valid during the callback. Present and release fences are not available for
 * this callback. Querying them using
 * OH_SurfaceTransactionStats_getPresentFenceFd and
 * OH_SurfaceTransactionStats_getPreviousReleaseFenceFd will result in failure.
 *
 * THREADING
 * The transaction committed callback can be invoked on any thread.
 */
typedef void (*OH_SurfaceTransaction_OnCommit)(
    void* context,
    OH_SurfaceTransactionStats* stats);
/**
 * The OH_SurfaceTransaction_OnBufferRelease callback is invoked when a buffer
 * that was passed in OH_SurfaceTransaction_setBuffer is ready to be reused.
 *
 * This callback is guaranteed to be invoked if OH_SurfaceTransaction_setBuffer
 * is called with a non null buffer. If the buffer in the transaction is
 * replaced via another call to OH_SurfaceTransaction_setBuffer, the callback
 * will be invoked immediately. Otherwise the callback will be invoked before
 * the OH_SurfaceTransaction_OnComplete callback after the buffer was presented.
 *
 * If this callback is set, caller should not release the buffer using the
 * OH_SurfaceTransaction_OnComplete.
 *
 * \param context Optional context provided by the client that is passed into
 * the callback.
 *
 * \param release_fence_fd Returns the fence file descriptor used to signal the
 * release of buffer associated with this callback. If this fence is valid
 * (>=0), the buffer has not yet been released and the fence will signal when
 * the buffer has been released. If the fence is -1 , the buffer is already
 * released. The recipient of the callback takes ownership of the fence fd and
 * is responsible for closing it.
 *
 * THREADING
 * The callback can be invoked on any thread.
 */
typedef void (*OH_SurfaceTransaction_OnBufferRelease)(void* context,
                                                      int release_fence_fd);
/**
 * Returns the timestamp of when the frame was latched by the framework. Once a
 * frame is latched by the framework, it is presented at the following hardware
 * vsync.
 */
int64_t OH_SurfaceTransactionStats_GetLatchTime(
    OH_SurfaceTransactionStats* surface_transaction_stats);
/**
 * Returns a sync fence that signals when the transaction has been presented.
 * The recipient of the callback takes ownership of the fence and is responsible
 * for closing it. If a device does not support present fences, a -1 will be
 * returned.
 *
 * This query is not valid for OH_SurfaceTransaction_OnCommit callback.
 *
 */
int OH_SurfaceTransactionStats_GetPresentFenceFd(
    OH_SurfaceTransactionStats* surface_transaction_stats);

void OH_SurfaceTransactionStats_GetSurfaceControls(
    OH_SurfaceTransactionStats* surface_transaction_stats,
    OH_SurfaceControl*** out_surface_controls,
    size_t* out_surface_controls_size);

void OH_SurfaceTransactionStats_ReleaseSurfaceControls(
    OH_SurfaceControl** surface_controls);

/**
 * Sets the callback that will be invoked when the updates from this transaction
 * are presented. For details on the callback semantics and data, see the
 * comments on the OH_SurfaceTransaction_OnComplete declaration above.
 */
void OH_SurfaceTransaction_SetOnComplete(OH_SurfaceTransaction* transaction,
                                         void* context,
                                         OH_SurfaceTransaction_OnComplete func);
/**
 * Sets the callback that will be invoked when the updates from this transaction
 * are applied and are ready to be presented. This callback will be invoked
 * before the OH_SurfaceTransaction_OnComplete callback.
 */
void OH_SurfaceTransaction_SetOnCommit(OH_SurfaceTransaction* transaction,
                                       void* context,
                                       OH_SurfaceTransaction_OnCommit func);
/**
 * Reparents the \a surface_control from its old parent to the \a new_parent
 * surface control. Any children of the reparented \a surface_control will
 * remain children of the \a surface_control.
 *
 * The \a new_parent can be null. Surface controls with a null parent do not
 * appear on the display.
 *
 */
void OH_SurfaceTransaction_Reparent(OH_SurfaceTransaction* transaction,
                                    OH_SurfaceControl* surface_control,
                                    OH_SurfaceControl* new_parent);
/**
 * Parameter for OH_SurfaceTransaction_setVisibility().
 */
enum OH_SurfaceTransactionVisibility : int8_t {
  OH_SURFACE_TRANSACTION_VISIBILITY_HIDE = 0,
  OH_SURFACE_TRANSACTION_VISIBILITY_SHOW = 1,
};

/**
 * Updates the visibility of \a surface_control. If show is set to
 * OH_SURFACE_TRANSACTION_VISIBILITY_HIDE, the \a surface_control and all
 * surfaces in its subtree will be hidden.
 */
void OH_SurfaceTransaction_SetVisibility(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    enum OH_SurfaceTransactionVisibility visibility);
/**
 * Updates the z order index for \a surface_control. Note that the z order for a
 * surface is relative to other surfaces which are siblings of this surface. The
 * behavior of sibilings with the same z order is undefined.
 *
 * Z orders may be from MIN_INT32 to MAX_INT32. A layer's default z order index
 * is 0.
 */
void OH_SurfaceTransaction_SetZOrder(OH_SurfaceTransaction* transaction,
                                     OH_SurfaceControl* surface_control,
                                     int32_t z_order);
/**
 * Updates the OH_NativeBuffer displayed for \a surface_control. If not -1, the
 * acquire_fence_fd should be a file descriptor that is signaled when all
 * pending work for the buffer is complete and the buffer can be safely read.
 *
 * The frameworks takes ownership of the \a acquire_fence_fd passed and is
 * responsible for closing it.
 *
 * Note that the buffer must be allocated with
 * AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE as the surface control might be
 * composited using the GPU.
 *
 * When the buffer is ready to be reused, the
 * OH_SurfaceTransaction_OnBufferRelease callback will be invoked. If the buffer
 * is null, the callback will not be invoked.
 */
void OH_SurfaceTransaction_SetBuffer(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    OH_NativeBuffer* buffer,
    int acquire_fence_fd,
    void* context,
    OH_SurfaceTransaction_OnBufferRelease func);

/**
 * Bounds the surface and its children to the bounds specified. The crop and
 * buffer size will be used to determine the bounds of the surface. If no crop
 * is specified and the surface has no buffer, the surface bounds is only
 * constrained by the size of its parent bounds.
 *
 * \param crop The bounds of the crop to apply.
 */
void OH_SurfaceTransaction_SetCrop(OH_SurfaceTransaction* transaction,
                                   OH_SurfaceControl* surface_control,
                                   const OH_Rect* crop);
void OH_SurfaceTransaction_SetBounds(OH_SurfaceTransaction* transaction,
                                     OH_SurfaceControl* surface_control,
                                     float x,
                                     float y,
                                     float w,
                                     float h);
void OH_SurfaceTransaction_SetFrame(OH_SurfaceTransaction* transaction,
                                    OH_SurfaceControl* surface_control,
                                    float x,
                                    float y,
                                    float w,
                                    float h);

enum OH_FRAME_GRAVITY : int32_t {
  OH_FRAME_GRAVITY_CENTER = 0,
  OH_FRAME_GRAVITY_TOP,
  OH_FRAME_GRAVITY_BOTTOM,
  OH_FRAME_GRAVITY_LEFT,
  OH_FRAME_GRAVITY_RIGHT,
  OH_FRAME_GRAVITY_TOP_LEFT,
  OH_FRAME_GRAVITY_TOP_RIGHT,
  OH_FRAME_GRAVITY_BOTTOM_LEFT,
  OH_FRAME_GRAVITY_BOTTOM_RIGHT,
  OH_FRAME_GRAVITY_RESIZE,
  OH_FRAME_GRAVITY_RESIZE_ASPECT,
  OH_FRAME_GRAVITY_RESIZE_ASPECT_TOP_LEFT,
  OH_FRAME_GRAVITY_RESIZE_ASPECT_BOTTOM_RIGHT,
  OH_FRAME_GRAVITY_RESIZE_ASPECT_FILL,
  OH_FRAME_GRAVITY_RESIZE_ASPECT_FILL_TOP_LEFT,
  OH_FRAME_GRAVITY_RESIZE_ASPECT_FILL_BOTTOM_RIGHT,
  OH_FRAME_GRAVITY_DEFAULT = OH_FRAME_GRAVITY_TOP_LEFT
};
void OH_SurfaceTransaction_SetFrameGravity(OH_SurfaceTransaction* transaction,
                                           OH_SurfaceControl* surface_control,
                                           int32_t gravity);
/**
 * Specifies the position in the parent's space where the surface will be drawn.
 *
 * \param x The x position to render the surface.
 * \param y The y position to render the surface.
 */
void OH_SurfaceTransaction_SetPosition(OH_SurfaceTransaction* transaction,
                                       OH_SurfaceControl* surface_control,
                                       float x,
                                       float y);

void OH_SurfaceTransaction_SetPivot(OH_SurfaceTransaction* transaction,
                                    OH_SurfaceControl* surface_control,
                                    float x,
                                    float y);
enum OH_TRANSFORM : int32_t {
  OH_TRANSFORM_ROTATE_NONE = 0, /**< No rotation */
  OH_TRANSFORM_ROTATE_90,       /**< Rotation by 90 degrees */
  OH_TRANSFORM_ROTATE_180,      /**< Rotation by 180 degrees */
  OH_TRANSFORM_ROTATE_270,      /**< Rotation by 270 degrees */
  OH_TRANSFORM_FLIP_H,          /**< Flip horizontally */
  OH_TRANSFORM_FLIP_V,          /**< Flip vertically */
  OH_TRANSFORM_FLIP_H_ROT90,    /**< Flip horizontally and rotate 90 degrees */
  OH_TRANSFORM_FLIP_V_ROT90,    /**< Flip vertically and rotate 90 degrees */
  OH_TRANSFORM_FLIP_H_ROT180,   /**< Flip horizontally and rotate 180 degrees */
  OH_TRANSFORM_FLIP_V_ROT180,   /**< Flip vertically and rotate 180 degrees */
  OH_TRANSFORM_FLIP_H_ROT270,   /**< Flip horizontally and rotate 270 degrees */
  OH_TRANSFORM_FLIP_V_ROT270,   /**< Flip vertically and rotate 270 degrees */
  OH_TRANSFORM_ROTATE_LAST = OH_TRANSFORM_FLIP_V_ROT270,
};
/**
 * \param transform The transform applied after the source rect is applied to
 * the buffer. This parameter should be set to 0 for no transform. To specify a
 * transform use the NATIVE_WINDOW_TRANSFORM_* enum.
 */
void OH_SurfaceTransaction_SetBufferTransform(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    int32_t transform);
/**
 * Sets an x and y scale of a surface with (0, 0) as the centerpoint of the
 * scale.
 *
 * \param xScale The scale in the x direction. Must be greater than 0.
 * \param yScale The scale in the y direction. Must be greater than 0.
 */
void OH_SurfaceTransaction_SetScale(OH_SurfaceTransaction* transaction,
                                    OH_SurfaceControl* surface_control,
                                    float scaleX,
                                    float scaleY,
                                    float scaleZ);

/**
 * Sets an x and y scale of a surface with (0, 0) as the centerpoint of the
 * scale.
 *
 * \param xScale The scale in the x direction. Must be greater than 0.
 * \param yScale The scale in the y direction. Must be greater than 0.
 */
void OH_SurfaceTransaction_SetRotation(OH_SurfaceTransaction* transaction,
                                       OH_SurfaceControl* surface_control,
                                       float degreeX,
                                       float degreeY,
                                       float degreeZ);

void OH_SurfaceTransaction_SetTranslate(OH_SurfaceTransaction* transaction,
                                        OH_SurfaceControl* surface_control,
                                        float translateX,
                                        float translateY,
                                        float translateZ);

void OH_SurfaceTransaction_SetSkew(OH_SurfaceTransaction* transaction,
                                   OH_SurfaceControl* surface_control,
                                   float skewX,
                                   float skewY,
                                   float skewZ);

void OH_SurfaceTransaction_SetPerspective(OH_SurfaceTransaction* transaction,
                                          OH_SurfaceControl* surface_control,
                                          float perspX,
                                          float perspY,
                                          float perspZ,
                                          float perspW);

/**
 * Parameter for OH_SurfaceTransaction_setBufferTransparency().
 */
enum OH_SurfaceTransactionTransparency : int8_t {
  OH_SURFACE_TRANSACTION_TRANSPARENCY_TRANSPARENT = 0,
  OH_SURFACE_TRANSACTION_TRANSPARENCY_TRANSLUCENT = 1,
  OH_SURFACE_TRANSACTION_TRANSPARENCY_OPAQUE = 2,
};

/**
 * Updates whether the content for the buffer associated with this surface is
 * completely opaque. If true, every pixel of content inside the buffer must be
 * opaque or visual errors can occur.
 *
 * Available since API level 29.
 */
void OH_SurfaceTransaction_SetBufferTransparency(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    enum OH_SurfaceTransactionTransparency transparency);
/**
 * Updates the region for the content on this surface updated in this
 * transaction. If unspecified, the complete surface is assumed to be damaged.
 */
void OH_SurfaceTransaction_SetDamageRegion(OH_SurfaceTransaction* transaction,
                                           OH_SurfaceControl* surface_control,
                                           const OH_Rect* rects,
                                           uint32_t count);
/**
 * Specifies a desired_presentTime for the transaction. The framework will try
 * to present the transaction at or after the time specified.
 *
 * Transactions will not be presented until all of their acquire fences have
 * signaled even if the app requests an earlier present time.
 *
 * If an earlier transaction has a desired present time of x, and a later
 * transaction has a desired present time that is before x, the later
 * transaction will not preempt the earlier transaction.
 */
void OH_SurfaceTransaction_SetDesiredPresentTime(
    OH_SurfaceTransaction* transaction,
    int64_t desired_presentTime);
/**
 * Sets the alpha for the buffer. It uses a premultiplied blending.
 *
 * The \a alpha must be between 0.0 and 1.0.
 */
void OH_SurfaceTransaction_SetBufferAlpha(OH_SurfaceTransaction* transaction,
                                          OH_SurfaceControl* surface_control,
                                          float alpha);
/**
 * Sets the intended frame rate for \a surface_control.
 *
 * On devices that are capable of running the display at different refresh
 * rates, the system may choose a display refresh rate to better match this
 * surface's frame rate. Usage of this API won't directly affect the
 * application's frame production pipeline. However, because the system may
 * change the display refresh rate, calls to this function may result in changes
 * to Choreographer callback timings, and changes to the time interval at which
 * the system releases buffers back to the application.
 *
 * You can register for changes in the refresh rate using
 * \a AChoreographer_registerRefreshRateCallback.
 *
 * See OH_SurfaceTransaction_clearFrameRate().
 *
 * \param frameRate is the intended frame rate of this surface, in frames per
 * second. 0 is a special value that indicates the app will accept the system's
 * choice for the display frame rate, which is the default behavior if this
 * function isn't called. The frameRate param does <em>not</em> need to be a
 * valid refresh rate for this device's display - e.g., it's fine to pass 30fps
 * to a device that can only run the display at 60fps.
 *
 * \param compatibility The frame rate compatibility of this surface. The
 * compatibility value may influence the system's choice of display frame rate.
 * To specify a compatibility use the ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_*
 * enum. This parameter is ignored when frameRate is 0.
 *
 * \param changeFrameRateStrategy Whether display refresh rate transitions
 * caused by this surface should be seamless. A seamless transition is one that
 * doesn't have any visual interruptions, such as a black screen for a second or
 * two. See the ANATIVEWINDOW_CHANGE_FRAME_RATE_* values. This parameter is
 * ignored when frameRate is 0.
 *
 */
void OH_SurfaceTransaction_SetFrameRateWithChangeStrategy(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    float frameRate,
    int8_t compatibility,
    int8_t changeFrameRateStrategy);
/**
 * Clears the frame rate which is set for \a surface_control.
 *
 * This is equivalent to calling
 * OH_SurfaceTransaction_setFrameRateWithChangeStrategy(
 * transaction, 0, compatibility, changeFrameRateStrategy).
 *
 * Usage of this API won't directly affect the application's frame production
 * pipeline. However, because the system may change the display refresh rate,
 * calls to this function may result in changes to Choreographer callback
 * timings, and changes to the time interval at which the system releases
 * buffers back to the application.
 *
 * See OH_SurfaceTransaction_setFrameRateWithChangeStrategy()
 *
 * You can register for changes in the refresh rate using
 * \a AChoreographer_registerRefreshRateCallback.
 *
 * See OH_SurfaceTransaction_setFrameRateWithChangeStrategy().
 *
 */
void OH_SurfaceTransaction_ClearFrameRate(OH_SurfaceTransaction* transaction,
                                          OH_SurfaceControl* surface_control);
/**
 * Indicate whether to enable backpressure for buffer submission to a given
 * SurfaceControl.
 *
 * By default backpressure is disabled, which means submitting a buffer prior to
 * receiving a callback for the previous buffer could lead to that buffer being
 * "dropped". In cases where you are selecting for latency, this may be a
 * desirable behavior! We had a new buffer ready, why shouldn't we show it?
 *
 * When back pressure is enabled, each buffer will be required to be presented
 * before it is released and the callback delivered
 * (absent the whole SurfaceControl being removed).
 *
 * Most apps are likely to have some sort of backpressure internally, e.g. you
 * are waiting on the callback from frame N-2 before starting frame N. In high
 * refresh rate scenarios there may not be much time between SurfaceFlinger
 * completing frame N-1 (and therefore releasing buffer N-2) and beginning frame
 * N. This means your app may not have enough time to respond in the callback.
 * Using this flag and pushing buffers earlier for server side queuing will be
 * advantageous in such cases.
 *
 * \param transaction The transaction in which to make the change.
 * \param surface_control The OH_SurfaceControl on which to control buffer
 * backpressure behavior.
 * \param enableBackPressure Whether to enable back pressure.
 */
void OH_SurfaceTransaction_SetEnableBackPressure(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    bool enableBackPressure);

void OH_SurfaceTransaction_SetForegroundColor(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    float red,
    float green,
    float blue,
    float alpha);

void OH_SurfaceTransaction_SetBackgroundColor(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    float red,
    float green,
    float blue,
    float alpha);

void OH_SurfaceTransaction_SetBorderColor(OH_SurfaceTransaction* transaction,
                                          OH_SurfaceControl* surface_control,
                                          float red,
                                          float green,
                                          float blue,
                                          float alpha);

void OH_SurfaceTransaction_SetBorderWidth(OH_SurfaceTransaction* transaction,
                                          OH_SurfaceControl* surface_control,
                                          float left,
                                          float top,
                                          float right,
                                          float bottom);

enum BorderStyle : int32_t {
  OH_SURFACE_TRANSACTION_BORDER_STYLE_SOLID = 0,
  OH_SURFACE_TRANSACTION_BORDER_STYLE_DASHED = 1,
  OH_SURFACE_TRANSACTION_BORDER_STYLE_DOTTED = 2,
  OH_SURFACE_TRANSACTION_BORDER_STYLE_NONE = 3,
};
void OH_SurfaceTransaction_SetBorderStyle(OH_SurfaceTransaction* transaction,
                                          OH_SurfaceControl* surface_control,
                                          uint32_t left,
                                          uint32_t top,
                                          uint32_t right,
                                          uint32_t bottom);
void OH_SurfaceTransaction_SetName(OH_SurfaceTransaction* transaction,
                                   OH_SurfaceControl* surface_control,
                                   const char* name);
void OH_SurfaceTransaction_SetHardwareEnableHint(
    OH_SurfaceTransaction* transaction,
    OH_SurfaceControl* surface_control,
    bool enable);

#ifdef __cplusplus
}
#endif

#endif  // SURFACE_CONTROL_H_
