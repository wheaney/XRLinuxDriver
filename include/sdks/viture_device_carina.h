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
 * @param pose      Array of 7 floats: [px, py, pz, qw, qx, qy, qz].
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
 * @return VITURE_GLASSES_SUCCESS on success, VITURE_GLASSES_ERROR_INVALID_PARAM on failure.
 */
VITURE_API int xr_device_provider_register_callbacks_carina(XRDeviceProviderHandle handle,
                                                            XRPoseCallback pose_callback,
                                                            XRVSyncCallback vsync_callback,
                                                            XRImuCallback imu_callback,
                                                            XRCameraCallback camera_callback);

/**
 * @brief Set DOF type for Carina device. Must be called after xr_device_provider_create
 *        and before xr_device_provider_initialize. Default is 6DOF.
 * @param handle Handle to the XRDeviceProvider instance
 * @param is_6dof 1 for 6DOF, 0 for 3DOF
 * @return VITURE_GLASSES_SUCCESS on success, error code on failure
 */
VITURE_API int xr_device_provider_set_dof_type_carina(XRDeviceProviderHandle handle, int is_6dof);

/**
 * @brief Trigger a full VIO re-initialisation.
 *
 * Resets the Carina tracking state, including position and yaw.  Pitch and roll remain
 * gravity-anchored and are not affected.
 *
 * This is a heavyweight operation that briefly interrupts tracking.  Prefer
 * xr_device_provider_reset_origin_carina() for lightweight heading/position recentering.
 *
 * @param handle Handle to the XRDeviceProvider instance
 * @return VITURE_GLASSES_SUCCESS on success, VITURE_GLASSES_ERROR_INVALID_PARAM on failure
 */
VITURE_API int xr_device_provider_reset_pose_carina(XRDeviceProviderHandle handle);

/**
 * @brief Set a new tracking origin in OpenGL coordinate system (x -> right, y -> up, z -> backward).
 *
 * After this call, subsequent xr_device_provider_get_gl_pose_carina() returns poses expressed
 * relative to the specified origin.
 *
 * Typical uses:
 *   - Pass the pose returned by xr_device_provider_get_gl_pose_carina() to reset position and
 *     yaw to the user's current physical state.
 *   - Pass a manually constructed pose to relocate the tracking origin to any desired position
 *     and heading (e.g. teleporting the camera to a fixed point in the scene).
 *
 * Axis reset behaviour:
 *   - Position is reset to the translation encoded in `pose`.
 *   - Yaw is reset to the yaw component of the quaternion in `pose`.
 *   - Pitch and roll are gravity-anchored by the Carina VIO system and are NOT affected by this
 *     call regardless of the quaternion passed.  They always reflect the device's absolute
 *     orientation relative to gravity.
 *
 * @param handle Handle to the XRDeviceProvider instance
 * @param pose   Target origin pose: [px, py, pz, qw, qx, qy, qz].
 *               Pass the result of xr_device_provider_get_gl_pose_carina() to anchor the origin
 *               at the current physical pose, or supply a custom pose to teleport to a specific
 *               location and heading.
 * @return VITURE_GLASSES_SUCCESS on success, error code on failure
 */
VITURE_API int xr_device_provider_reset_origin_carina(XRDeviceProviderHandle handle, float *pose);

/**
 * @brief Get IMU pose data with prediction time (Twb matrix in OpenGL coordinate system: x -> right, y -> up, z -> backward)
 * @param handle Handle to the XRDeviceProvider instance
 * @param pose Array to store pose data (7 floats for position and quaternion)
 * @param predict_time Prediction time in nanoseconds, 0 for current pose data
 * @param pose_status Output pose status: 0 = stable, 1 = unstable (may occur briefly after device start). Can be null.
 * @return VITURE_GLASSES_SUCCESS on success, VITURE_GLASSES_ERROR_INVALID_PARAM on failure
 */
VITURE_API int xr_device_provider_get_gl_pose_carina(XRDeviceProviderHandle handle,
                                                     float *pose, double predict_time, int *pose_status);

#ifdef __cplusplus
}
#endif

#endif // VITURE_DEVICE_CARINA_H
