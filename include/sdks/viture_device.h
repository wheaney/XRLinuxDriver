/**
 * @file
 * @brief Carina-specific device API and callback types.
 * @copyright 2025 VITURE Inc. All rights reserved.
 *
 * These callbacks are only applicable when the provider was created
 * for `XR_DEVICE_TYPE_VITURE_GEN1` or `XR_DEVICE_TYPE_VITURE_GEN2`.
 */

#ifndef VITURE_DEVICE_H
#define VITURE_DEVICE_H

#include "viture_glasses_provider.h"

/**
 * @brief Callback function type for Viture device combining IMU and VSync data.
 *
 * @param imu   Pointer to IMU data buffer (device-defined layout).
 * @param euler Pointer to Euler angle data (device-defined layout).
 * @param ts    Timestamp in device timebase for IMU sample.
 * @param vsync VSync timestamp associated with the sample.
 */
typedef void (*XRIMUAndVsyncCallback)(float* imu, float* euler, uint64_t ts, uint64_t vsync);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a combined IMU + VSync callback for a Viture device.
 *
 * This function associates a user-provided callback with the device
 * identified by `handle`. The callback will be invoked from the internal
 * IMU read thread when IMU samples and VSync timestamps are available.
 *
 * @param handle             Handle to the `XRDeviceProvider` instance.
 * @param imu_vsync_callback Function pointer invoked for IMU + VSync data.
 * @return 0 on success, -1 on failure.
 */
VITURE_API int register_callback(XRDeviceProviderHandle handle, XRIMUAndVsyncCallback imu_vsync_callback);

/**
 * @brief Open imu (no effect for carina device)
 * @param handle Handle to the XRDeviceProvider instance
 * @param imu_mode viture::protocol::imu::Mode
 * @param imu_report_frequency viture::protocol::imu::Frequency
 * @return 0: Success, -1: Param error, -2: USB execution error
 * @return -3: Device type not supported, -4: Other error
 */
VITURE_API int open_imu(XRDeviceProviderHandle handle, uint8_t imu_mode, uint8_t imu_report_frequency);

/**
 * @brief Close IMU (no effect for Carina device)
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: USB execution error
 * @return -3: Device type not supported, -4: Other error
 */
VITURE_API int close_imu(XRDeviceProviderHandle handle);

#ifdef __cplusplus
}
#endif

#endif // VITURE_DEVICE_H
