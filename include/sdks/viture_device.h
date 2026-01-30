/**
 * @file
 * @brief Non-carina device API and callback types.
 * @copyright 2025 VITURE Inc. All rights reserved.
 *
 * These callbacks are only applicable when the provider was created
 * for `XR_DEVICE_TYPE_VITURE_GEN1` or `XR_DEVICE_TYPE_VITURE_GEN2`.
 */

#ifndef VITURE_DEVICE_H
#define VITURE_DEVICE_H

#include "viture_glasses_provider.h"

/**
 * @brief Callback function type for imu raw data.
 *
 * @param data      Pointer to raw data buffer (device-defined layout).
 * @param timestamp Timestamp in device timebase for IMU sample.
 * @param vsync     VSync timestamp associated with the sample.
 *
 * Should be set before calling xr_device_provider_open_imu with VITURE_IMU_MODE_RAW
 * Data format:
 * 1. Viture One / Pro / Lite: [gyroscope_raw_x, gyroscope_raw_y, gyroscope_raw_z,
 *                              accelerometer_raw_x, accelerometer_raw_y, accelerometer_raw_z,
 *                              0, 0, 0,
 *                              temperature]
 * 2. Viture Luma / Luma Pro / Beast:  [gyroscope_raw_x, gyroscope_raw_y, gyroscope_raw_z,
 *                                      accelerometer_raw_x, accelerometer_raw_y, accelerometer_raw_z,
 *                                      magnetometer_raw_x, magnetometer_raw_y, magnetometer_raw_z,
 *                                      temperature]
 */
typedef void (*VitureImuRawCallback)(float* data, uint64_t timestamp, uint64_t vsync);

/**
 * @brief Callback function type for imu pose data.
 *
 * @param data      Pointer to pose data buffer
 * @param timestamp Timestamp in device timebase for IMU sample.
 *
 * Should be set before calling xr_device_provider_open_imu with VITURE_IMU_MODE_POSE
 * Coordinate system: North-West-Up (NWU), X->North, Y->West, Z->Up.
 * Data format: [roll, pitch, yaw, quaternion_w, quaternion_x, quaternion_y, quaternion_z]
 */
typedef void (*VitureImuPoseCallback)(float* data, uint64_t timestamp);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register imu raw data callback.
 *
 * This function associates a user-provided callback with the device
 * identified by `handle`. The callback will be invoked from the internal
 * imu read thread when imu raw data is available.
 *
 * @param handle           Handle to the `XRDeviceProvider` instance.
 * @param imu_raw_callback Callback function pointer.
 * @return 0 on success, -1 on failure.
 */
VITURE_API int xr_device_provider_register_imu_raw_callback(XRDeviceProviderHandle handle,
                                                            VitureImuRawCallback imu_raw_callback);

/**
 * @brief Register a combined IMU + VSync callback for a Viture device.
 *
 * This function associates a user-provided callback with the device
 * identified by `handle`. The callback will be invoked from the internal
 * imu read thread when imu pose data is available.
 *
 * @param handle            Handle to the `XRDeviceProvider` instance.
 * @param imu_pose_callback Callback function pointer.
 * @return 0 on success, -1 on failure.
 */
VITURE_API int xr_device_provider_register_imu_pose_callback(XRDeviceProviderHandle handle,
                                                             VitureImuPoseCallback imu_pose_callback);

/**
 * @brief Open imu (no effect for carina device)
 * @param handle Handle to the XRDeviceProvider instance
 * @param imu_mode viture::protocol::imu::Mode
 * @param imu_report_frequency viture::protocol::imu::Frequency
 * @return 0: Success, -1: Param error, -2: USB execution error
 * @return -3: Device type not supported, -4: Other error
 */
VITURE_API int xr_device_provider_open_imu(XRDeviceProviderHandle handle, uint8_t imu_mode, uint8_t imu_report_frequency);

/**
 * @brief Close IMU (no effect for Carina device)
 * @param handle Handle to the XRDeviceProvider instance
 * @param imu_mode viture::protocol::imu::Mode
 * @return 0: Success, -1: Param error, -2: USB execution error
 * @return -3: Device type not supported, -4: Other error
 */
VITURE_API int xr_device_provider_close_imu(XRDeviceProviderHandle handle, uint8_t imu_mode);

#ifdef __cplusplus
}
#endif

#endif // VITURE_DEVICE_H
