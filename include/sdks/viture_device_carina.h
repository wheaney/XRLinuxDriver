/**
 * @file
 * @brief Carina-specific device API and callback types.
 * @copyright 2025 VITURE Inc. All rights reserved.
 *
 * The Carina device (Viture Luma Ultra) provides higher-level callbacks for
 * pose, IMU, VSync and camera frames. These callbacks are only applicable
 * when the provider was created for `XR_DEVICE_TYPE_VITURE_CARINA`.
 */

#ifndef VITURE_DEVICE_CARINA_H
#define VITURE_DEVICE_CARINA_H

#include "viture_glasses_provider.h"

/**
 * @brief Callback invoked when a new pose sample is available, This function
 * returns pose data at the Camera's frequency (25 Hz). In most cases, this
 * interface can be ignored.
 *
 * @param pose      Array of 32 float pose data.
 * @param timestamp Monotonic timestamp in seconds.
 */
typedef void (*XRPoseCallback)(float* pose, double timestamp);

/**
 * @brief VSync notification callback.
 *
 * @param timestamp Monotonic timestamp in seconds when VSync occurred.
 */
typedef void (*XRVSyncCallback)(double timestamp);

/**
 * @brief IMU data callback for Carina device.
 *
 * @param imu       IMU data: [ax, ay, az, gx, gy, gz].
 * @param timestamp Timestamp in seconds for the IMU sample.
 */
typedef void (*XRImuCallback)(float* imu, double timestamp);

/**
 * @brief Camera frame callback for stereo frames.
 *
 * @param image_left0   Pointer to left image buffer (frame 0).
 * @param image_right0  Pointer to right image buffer (frame 0).
 * @param image_left1   Pointer to left image buffer (frame 1).
 * @param image_right1  Pointer to right image buffer (frame 1).
 * @param timestamp     Frame timestamp.
 * @param width         Frame width in pixels.
 * @param height        Frame height in pixels.
 */
typedef void (*XRCameraCallback)(char* image_left0,
                                 char* image_right0,
                                 char* image_left1,
                                 char* image_right1,
                                 double timestamp,
                                 int width,
                                 int height);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register callbacks for a Carina device instance.
 *
 * Callbacks registered here are ignored for devices that are not
 * `XR_DEVICE_TYPE_VITURE_CARINA`.
 *
 * @param handle         Opaque device provider handle.
 * @param pose_callback  Callback for pose samples.
 * @param vsync_callback Callback for VSync events.
 * @param imu_callback   Callback for IMU samples.
 * @param camera_callback Callback for stereo camera frames.
 * @return 0 on success, -1 on failure.
 */
VITURE_API int xr_device_provider_register_callbacks_carina(XRDeviceProviderHandle handle,
                                                            XRPoseCallback pose_callback,
                                                            XRVSyncCallback vsync_callback,
                                                            XRImuCallback imu_callback,
                                                            XRCameraCallback camera_callback);

/**
 * @brief Reset position
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0 on success, -1 on failure
 */
VITURE_API int xr_device_provider_reset_pose_carina(XRDeviceProviderHandle handle);

/**
 * @brief Get IMU pose data with prediction time (Twb matrix in OpenGL coordinate system: x -> right, y -> up, z -> backward)
 * @param handle Handle to the XRDeviceProvider instance
 * @param pose Array to store pose data (7 floats for position and quaternion)
 * @param predict_time Prediction time in nanoseconds, 0 for current pose data
 * @param pose_status Store pose status, 0 is good, 1 is bad. Can be null.
 * @return 0 on success, -1 on failure
 */
VITURE_API int xr_device_provider_get_gl_pose_carina(XRDeviceProviderHandle handle,
                                                     float *pose, double predict_time, int *pose_status);

#ifdef __cplusplus
}
#endif

#endif // VITURE_DEVICE_CARINA_H
