/**
 * @file
 * @brief Public C API for Viture Camera device provider lifecycle and management.
 * @copyright 2026 VITURE Inc. All rights reserved.
 *
 * This header defines the core public C API for XR camera provider instances:
 * - Lifecycle management: create, start, stop, destroy
 * - Callback registration: camera frame callbacks
 * - Device validation: camera product ID validation for different device models
 *
 * Camera Device Support:
 * - Luma Pro, Luma Cyber, Luma Ultra: VID=0x0C45, PID=0x636B
 * - Beast: VID=0x0C45, PID=0x6368
 * - Luma: No camera support
 * - One, Lite, Pro, Pro 2: No camera support
 */

#ifndef VITURE_CAMERA_PROVIDER_H
#define VITURE_CAMERA_PROVIDER_H

#include "viture_macros_public.h"
#include <stdint.h>

/**
 * @brief Handle type for XRCameraProvider instances. This is an opaque pointer
 * returned by `xr_camera_provider_create` and consumed by other API calls.
 */
typedef void* XRCameraProviderHandle;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera frame format
 */
typedef enum {
    XR_CAMERA_FORMAT_MJPEG = 0  // MJPEG compressed (raw from camera)
} XRCameraFormat;

/**
 * @brief Camera frame data structure
 */
typedef struct {
    uint8_t* data;           // Frame data pointer
    uint32_t size;           // Frame data size in bytes
    uint32_t width;          // Frame width in pixels
    uint32_t height;         // Frame height in pixels
    XRCameraFormat format;   // Frame format
    uint64_t timestamp;      // Frame timestamp in nanoseconds
    uint32_t sequence;       // Frame sequence number
} XRCameraFrame;

/**
 * @brief Callback for receiving camera frames
 *
 * @param frame Pointer to the camera frame data (valid only during callback)
 * @param user_data Application context pointer passed to xr_camera_provider_start
 *                  Can be used to pass application-specific state/context
 */
typedef void (*XRCameraFrameCallback)(const XRCameraFrame* frame, void* user_data);

/**
 * @brief Get camera vendor ID for the given glasses product ID
 *
 * @param glasses_product_id Product ID of the glasses (from xr_device_provider)
 * @return Camera vendor ID if supported, 0 if camera not available for this device
 */
VITURE_API int xr_camera_provider_get_camera_vid(int glasses_product_id);

/**
 * @brief Get camera product ID for the given glasses product ID
 *
 * @param glasses_product_id Product ID of the glasses (from xr_device_provider)
 * @return Camera product ID if supported, 0 if camera not available for this device
 *
 * Supported devices and their camera PIDs:
 * - Luma Pro, Luma Cyber: Camera PID 0x636B
 * - Luma Ultra: Camera PID 0x636B
 * - Beast: Camera PID 0x6368
 * - Luma, One, Lite, Pro, Pro 2: No camera (returns 0)
 */
VITURE_API int xr_camera_provider_get_camera_pid(int glasses_product_id);

/**
 * @brief Check if the given USB device is a valid Viture camera
 *
 * @param vendor_id USB Vendor ID
 * @param product_id USB Product ID
 * @return 1 if valid Viture camera, 0 otherwise
 */
VITURE_API int xr_camera_provider_is_valid_camera(int vendor_id, int product_id);

#ifdef __ANDROID__
/**
 * @brief Android variant: Create an XRCameraProvider instance
 *
 * Due to Android restrictions, USB file descriptor is needed for creation
 *
 * @param camera_vid Camera Vendor ID (use xr_camera_provider_get_camera_vid)
 * @param camera_pid Camera Product ID (use xr_camera_provider_get_camera_pid)
 * @param file_descriptor File descriptor of the opened USB camera device
 * @return Handle to the created instance, or NULL on failure
 */
VITURE_API XRCameraProviderHandle xr_camera_provider_create(int camera_vid,
                                                             int camera_pid,
                                                             int file_descriptor);
#else
/**
 * @brief Non-Android variant: Create an XRCameraProvider instance
 *
 * @param camera_vid Camera Vendor ID (use xr_camera_provider_get_camera_vid)
 * @param camera_pid Camera Product ID (use xr_camera_provider_get_camera_pid)
 * @return Handle to the created instance, or NULL on failure
 */
VITURE_API XRCameraProviderHandle xr_camera_provider_create(int camera_vid,
                                                             int camera_pid);
#endif

/**
 * @brief Start the camera stream with callback
 *
 * When started, camera frames will be delivered through the registered callback.
 * The callback is invoked on a dedicated camera thread.
 * Camera uses fixed configuration: 1920x1080@30fps, MJPEG format
 *
 * @param handle Handle to the XRCameraProvider instance
 * @param callback Callback function for receiving camera frames
 * @param user_data Application context passed to callback. Can be used to access
 *                  application state/objects from the callback thread (can be NULL)
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   invalid handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE camera device not found or failed to open
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   failed to negotiate stream format
 *         - VITURE_GLASSES_ERROR_USB_EXEC        failed to start streaming
 *         - VITURE_GLASSES_ERROR_INVALID_STATE   already streaming
 */
VITURE_API int xr_camera_provider_start(XRCameraProviderHandle handle,
                                         XRCameraFrameCallback callback,
                                         void* user_data);

/**
 * @brief Stop the camera stream
 *
 * After this call, no more frames will be delivered through the callback.
 *
 * @param handle Handle to the XRCameraProvider instance
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM  invalid handle
 *         - VITURE_GLASSES_ERROR_INVALID_STATE  not currently streaming
 */
VITURE_API int xr_camera_provider_stop(XRCameraProviderHandle handle);

/**
 * @brief Destroy the XRCameraProvider instance and release all resources
 *
 * This will stop streaming if active and release all associated resources.
 *
 * @param handle Handle to the XRCameraProvider instance
 */
VITURE_API void xr_camera_provider_destroy(XRCameraProviderHandle handle);

/**
 * @brief Check if camera is currently streaming
 *
 * @param handle Handle to the XRCameraProvider instance
 * @return 1 if streaming, 0 if not streaming or invalid handle
 */
VITURE_API int xr_camera_provider_is_streaming(XRCameraProviderHandle handle);


#ifdef __cplusplus
}
#endif

#endif // VITURE_CAMERA_PROVIDER_H
