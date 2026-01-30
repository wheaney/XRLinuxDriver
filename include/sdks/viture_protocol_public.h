/**
 * @file
 * @brief Public C API for Viture USB protocol operations and device control.
 * @copyright 2025 VITURE Inc. All rights reserved.
 *
 * This header provides:
 * - Protocol constants: display modes, DOF settings, IMU modes, and callback identifiers
 * - Device control functions: film mode, duty cycle, display mode, brightness, volume, etc.
 * - Forward declaration for XRDeviceProviderHandle
 *
 * These APIs are intended for use by application code to control and query
 * Viture glasses device settings and protocol parameters.
 */

#ifndef VITURE_PROTOCOL_PUBLIC_H
#define VITURE_PROTOCOL_PUBLIC_H

#include <stdint.h>
#include "viture_macros_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to an XRDeviceProvider instance.
 *
 * This handle is used to identify and interact with a specific Viture glasses device.
 * It must be obtained through xr_device_provider_create() before use.
 */
typedef void* XRDeviceProviderHandle;

// ============================================================================
// Product Identifiers
// ============================================================================

/**
 * @brief Product market name identifiers.
 *
 * These constants represent the market names of various Viture product models.
 */
#define VITURE_MARKET_NAME_ONE        "One"
#define VITURE_MARKET_NAME_PRO        "Pro"
#define VITURE_MARKET_NAME_LITE       "Lite"
#define VITURE_MARKET_NAME_LUMA       "Luma"
#define VITURE_MARKET_NAME_LUMA_PRO   "Luma Pro"
#define VITURE_MARKET_NAME_LUMA_ULTRA "Luma Ultra"
#define VITURE_MARKET_NAME_LUMA_CYBER "Luma Cyber"
#define VITURE_MARKET_NAME_BEAST      "Beast"

// ============================================================================
// Result Codes
// ============================================================================

/**
 * @brief Standard result codes used throughout the API.
 */
#define VITURE_RESULT_SUCCESS 0 /**< Operation completed successfully */
#define VITURE_RESULT_FAILURE 1 /**< Operation failed */

// ============================================================================
// Display Configuration Constants
// ============================================================================

/**
 * @brief Display mode identifiers for configuring device display output and refresh rates.
 *
 * Display modes specify both resolution and refresh rate. Standard modes are available
 * on all devices, while frame interpolation modes require native 3DOF support.
 */
#define VITURE_DISPLAY_MODE_1920_1080_60HZ  0x31 /**< 1920x1080 @ 60Hz */
#define VITURE_DISPLAY_MODE_3840_1080_60HZ  0x32 /**< 3840x1080 @ 60Hz (3D mode) */
#define VITURE_DISPLAY_MODE_1920_1080_90HZ  0x33 /**< 1920x1080 @ 90Hz */
#define VITURE_DISPLAY_MODE_1920_1080_120HZ 0x34 /**< 1920x1080 @ 120Hz */
#define VITURE_DISPLAY_MODE_3840_1080_90HZ  0x35 /**< 3840x1080 @ 90Hz (3D mode) */
#define VITURE_DISPLAY_MODE_1920_1200_60HZ  0x41 /**< 1920x1200 @ 60Hz */
#define VITURE_DISPLAY_MODE_3840_1200_60HZ  0x42 /**< 3840x1200 @ 60Hz (3D mode) */
#define VITURE_DISPLAY_MODE_1920_1200_90HZ  0x43 /**< 1920x1200 @ 90Hz */
#define VITURE_DISPLAY_MODE_1920_1200_120HZ 0x44 /**< 1920x1200 @ 120Hz */
#define VITURE_DISPLAY_MODE_3840_1200_90HZ  0x45 /**< 3840x1200 @ 90Hz (3D mode) */

/**
 * @brief Duty cycle presets for display controller.
 *
 * Duty cycle controls the display brightness by adjusting the on-time
 * percentage of the display pixels.
 */
#define VITURE_DUTY_CYCLE_H 98 /**< High duty cycle (brightest) */
#define VITURE_DUTY_CYCLE_M 42 /**< Medium duty cycle */
#define VITURE_DUTY_CYCLE_L 30 /**< Low duty cycle (dimmed) */

// ============================================================================
// Native 3DOF Configuration Constants
// ============================================================================

/**
 * @brief Native 3DOF (Degrees of Freedom) configuration constants.
 *
 * These constants are only available on devices with native 3DOF support (e.g., Viture Beast).
 * Native 3DOF enables head tracking and advanced display features.
 */

/**
 * @brief Display size identifiers for native 3DOF devices.
 *
 * Display size controls the apparent size of the virtual display.
 */
#define VITURE_DISPLAY_SIZE_SMALL  0x00 /**< Small display size */
#define VITURE_DISPLAY_SIZE_MEDIUM 0x01 /**< Medium display size */
#define VITURE_DISPLAY_SIZE_LARGE  0x02 /**< Large display size */
#define VITURE_DISPLAY_SIZE_EXTRA  0x03 /**< Extra large display size */
#define VITURE_DISPLAY_SIZE_ULTRA  0x04 /**< Ultra large display size */

/**
 * @brief Native DOF mode identifiers.
 *
 * These modes control the native 3DOF tracking behavior.
 */
#define VITURE_NATIVE_DOF_0             0x00 /**< No native DOF tracking */
#define VITURE_NATIVE_DOF_3             0x01 /**< Native 3DOF tracking enabled */
#define VITURE_NATIVE_DOF_SMOOTH_FOLLOW 0x02 /**< Smooth follow mode */

/**
 * @brief Frame interpolation display modes for native 3DOF devices.
 *
 * These modes use frame interpolation to upscale 60Hz input to 120Hz output,
 * providing smoother motion for head-tracked content.
 */
/**< 3840x1080, 60Hz input upscaled to 120Hz output */
#define VITURE_NATIVE_3DOF_DISPLAY_MODE_3840_1080_60HZ_120HZ 0x32
/**< 1920x1080, 60Hz input upscaled to 120Hz output */
#define VITURE_NATIVE_3DOF_DISPLAY_MODE_1920_1080_60HZ_120HZ 0x36
/**< Ultrawide mode, 60Hz input upscaled to 120Hz output */
#define VITURE_NATIVE_3DOF_DISPLAY_MODE_ULTRAWIDE_60HZ_120HZ 0x51
/**< Side mode, 60Hz input upscaled to 120Hz output */
#define VITURE_NATIVE_3DOF_DISPLAY_MODE_SIDEMODE_60HZ_120HZ 0x61

// ============================================================================
// IMU Configuration Constants
// ============================================================================

/**
 * @brief IMU (Inertial Measurement Unit) configuration options.
 */

/**
 * @brief IMU data reporting modes.
 */
#define VITURE_IMU_MODE_RAW  0 /**< Report raw IMU sensor data */
#define VITURE_IMU_MODE_POSE 1 /**< Report processed pose data */

/**
 * @brief IMU data reporting frequencies.
 */
#define VITURE_IMU_FREQUENCY_LOW         0 /**< 60Hz reporting rate */
#define VITURE_IMU_FREQUENCY_MEDIUM_LOW  1 /**< 90Hz reporting rate */
#define VITURE_IMU_FREQUENCY_MEDIUM      2 /**< 120Hz reporting rate */
#define VITURE_IMU_FREQUENCY_MEDIUM_HIGH 3 /**< 240Hz reporting rate */
#define VITURE_IMU_FREQUENCY_HIGH        4 /**< 500Hz reporting rate */

// ============================================================================
// Callback Identifiers
// ============================================================================

/**
 * @brief Callback identifiers for glass state change notifications.
 *
 * These identifiers are used when reporting state changes through callback
 * mechanisms. Each identifier corresponds to a specific device parameter.
 */
/**< Brightness level change. See @ref ValueRanges for device-specific value ranges. */
#define VITURE_CALLBACK_ID_BRIGHTNESS 0
/**< Volume level change. See @ref ValueRanges for device-specific value ranges. */
#define VITURE_CALLBACK_ID_VOLUME 1
/**< Display mode change. See VITURE_DISPLAY_MODE_* constants for valid values. */
#define VITURE_CALLBACK_ID_DISPLAY_MODE 2
/**< Electrochromic film status change. See @ref ValueRanges for device-specific value ranges. */
#define VITURE_CALLBACK_ID_ELECTROCHROMIC_FILM 3
/**< Native DOF mode change. See VITURE_NATIVE_DOF_* constants for valid values. */
#define VITURE_CALLBACK_ID_NATIVE_DOF 4

// ============================================================================
// Value Ranges
// ============================================================================

/**
 * @brief Brightness level value ranges by device model.
 *
 * | Device Model       | Value Range |
 * |--------------------|-------------|
 * | Viture One         | [0, 6]      |
 * | Viture Pro         | [0, 8]      |
 * | Viture Luma Series | [0, 8]      |
 * | Viture Beast       | [0, 8]      |
 */
#define VITURE_CALLBACK_BRIGHTNESS_VALUE_RANGE

/**
 * @brief Volume level value ranges by device model.
 *
 * | Device Model       | Value Range |
 * |--------------------|-------------|
 * | Viture One         | [0, 7]      |
 * | Viture Pro         | [0, 8]      |
 * | Viture Luma Series | [0, 8]      |
 * | Viture Beast       | [0, 15]     |
 */
#define VITURE_CALLBACK_VOLUME_VALUE_RANGE

/**
 * @brief Electrochromic film value ranges by device model.
 *
 * | Device Model            | Value Range |
 * |-------------------------|-------------|
 * | Viture One & Viture Pro | [0, 1]      |
 * | Viture Luma Series      | [0, 1]      |
 * | Viture Beast            | [0, 8]      |
 */
#define VITURE_CALLBACK_ELECTROCHROMIC_FILM_VALUE_RANGE

// ============================================================================
// Device Control Functions
// ============================================================================

/**
 * @brief Retrieve the current electrochromic film mode.
 *
 * The electrochromic film controls the tint level of the glasses lenses.
 * The voltage value interpretation differs between device generations:
 * - Gen1 devices: Binary mode (0.0 = off, 1.0 = on)
 * - Gen2 devices: Multi-level mode (0.0, 0.125, 0.25, ..., 0.875, 1.0)
 *
 * For device-specific value ranges, see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param voltage Output parameter to store the current voltage value (0.0 to 1.0).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or null voltage pointer)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Other error
 */
VITURE_API int xr_device_provider_get_film_mode(XRDeviceProviderHandle handle, float* voltage);

/**
 * @brief Set the electrochromic film mode.
 *
 * The electrochromic film controls the tint level of the glasses lenses.
 * The voltage parameter interpretation differs between device generations:
 * - Gen1 devices: Any non-zero value enables the film
 * - Gen2 devices: Voltage is mapped to discrete tint levels
 *
 * For device-specific value ranges, see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param voltage Voltage value in range [0.0, 1.0].
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or invalid voltage range)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Other error
 */
VITURE_API int xr_device_provider_set_film_mode(XRDeviceProviderHandle handle, float voltage);

/**
 * @brief Retrieve the current screen duty cycle.
 *
 * Duty cycle controls display brightness by adjusting the percentage of time
 * that pixels are active. Higher values result in brighter displays.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Duty cycle value in range [0, 100] on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: No valid data received
 *         - -5: Other error
 */
VITURE_API int xr_device_provider_get_duty_cycle(XRDeviceProviderHandle handle);

/**
 * @brief Set the screen duty cycle.
 *
 * Duty cycle controls display brightness by adjusting the percentage of time
 * that pixels are active. Higher values result in brighter displays.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param duty_cycle Duty cycle value in range [0, 100].
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or value out of range)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Duty cycle value rejected by device
 *         - -5: Other error
 */
VITURE_API int xr_device_provider_set_duty_cycle(XRDeviceProviderHandle handle, int duty_cycle);

/**
 * @brief Retrieve the current display mode.
 *
 * Display mode determines both the resolution and refresh rate of the device output.
 * See VITURE_DISPLAY_MODE_* constants for valid mode values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display mode value (see VITURE_DISPLAY_MODE_* constants) on success,
 *         negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: No valid data received
 *         - -5: Other error
 */
VITURE_API int xr_device_provider_get_display_mode(XRDeviceProviderHandle handle);

/**
 * @brief Set the display mode.
 *
 * Display mode determines both the resolution and refresh rate of the device output.
 * See VITURE_DISPLAY_MODE_* constants for valid mode values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param display_mode Display mode value (see VITURE_DISPLAY_MODE_* constants).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or invalid mode value)
 *         - -2: USB communication not available
 *         - -3: Display mode not supported by device
 *         - -4: USB execution error
 *         - -5: Other error
 */
VITURE_API int xr_device_provider_set_display_mode(XRDeviceProviderHandle handle, int display_mode);

/**
 * @brief Switch between 2D and 3D display modes.
 *
 * This is a convenience function that switches between:
 * - 2D mode: 1920x1080 @ 60Hz
 * - 3D mode: 3840x1080 @ 60Hz
 *
 * For more advanced display mode options (higher resolutions or refresh rates),
 * use xr_device_provider_set_display_mode() instead.
 *
 * @note If the device is already in the requested mode, the function returns
 *       successfully (0) without performing any action.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param is_3d true to switch to 3D mode, false to switch to 2D mode.
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: Display mode not supported by device
 *         - -4: USB execution error
 *         - -5: Other error
 */
VITURE_API int xr_device_provider_switch_dimension(XRDeviceProviderHandle handle, bool is_3d);

/**
 * @brief Retrieve the current screen brightness level.
 *
 * Brightness levels vary by device model. For device-specific value ranges,
 * see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Brightness level on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Response length incorrect
 *         - -5: Response data parsing error
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_get_brightness_level(XRDeviceProviderHandle handle);

/**
 * @brief Set the screen brightness level.
 *
 * Brightness levels vary by device model. For device-specific value ranges,
 * see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param level Brightness level to set (device-specific range).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or value out of range)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Other error
 */
VITURE_API int xr_device_provider_set_brightness_level(XRDeviceProviderHandle handle, int level);

/**
 * @brief Retrieve the current speaker volume level.
 *
 * Volume levels vary by device model. For device-specific value ranges,
 * see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Volume level on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Response length incorrect
 *         - -5: Response data parsing error
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_get_volume_level(XRDeviceProviderHandle handle);

/**
 * @brief Set the speaker volume level.
 *
 * Volume levels vary by device model. For device-specific value ranges,
 * see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param level Volume level to set (device-specific range).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or value out of range)
 *         - -2: USB communication not available
 *         - -3: USB execution error
 *         - -4: Other error
 */
VITURE_API int xr_device_provider_set_volume_level(XRDeviceProviderHandle handle, int level);

/**
 * @brief Retrieve the glasses firmware version string.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param response Output buffer to store the version string. Must be pre-allocated.
 * @param length Input/output parameter: on input, specifies the buffer size;
 *              on output, contains the actual length of the version string.
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle, null response, or null length)
 *         - -2: USB protocol not available
 *         - -3: USB response error
 *         - -4: No valid data received
 *         - -5: Exception occurred during execution
 */
VITURE_API int xr_device_provider_get_glasses_version(XRDeviceProviderHandle handle, char* response, int* length);

// ============================================================================
// Native 3DOF Functions
// ============================================================================

/**
 * @brief Retrieve the current display mode and native DOF type.
 *
 * This function is only available on devices with native 3DOF support (e.g., Viture Beast).
 * It retrieves both the display mode and the current DOF (Degrees of Freedom) configuration.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param display_mode Output parameter to store the display mode value.
 * @param dof_type Output parameter to store the DOF type value (see VITURE_NATIVE_DOF_* constants).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or null output pointers)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: USB execution error
 *         - -5: No valid data received
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_get_display_mode_and_native_dof(XRDeviceProviderHandle handle,
                                                                  int* display_mode,
                                                                  int* dof_type);

/**
 * @brief Set the display mode and native DOF type.
 *
 * This function is only available on devices with native 3DOF support (e.g., Viture Beast).
 * It configures both the display mode and the DOF (Degrees of Freedom) setting.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param display_mode Display mode value (see VITURE_NATIVE_3DOF_DISPLAY_MODE_* constants).
 * @param dof_type DOF type value (see VITURE_NATIVE_DOF_* constants).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or invalid values)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: Display mode not supported by device
 *         - -5: USB execution error
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_set_display_mode_and_native_dof(XRDeviceProviderHandle handle,
                                                                  int display_mode,
                                                                  int dof_type);

/**
 * @brief Retrieve the current display distance setting.
 *
 * This function is only available on Viture Beast devices.
 * Display distance controls the virtual distance of the display content.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display distance value in range [1, 10] on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: USB execution error
 *         - -5: No valid data received
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_get_display_distance(XRDeviceProviderHandle handle);

/**
 * @brief Set the display distance.
 *
 * This function is only available on Viture Beast devices.
 * Display distance controls the virtual distance of the display content.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param distance Display distance value in range [1, 10].
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or value out of range)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: Distance value rejected by device
 *         - -5: USB execution error
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_set_display_distance(XRDeviceProviderHandle handle, int distance);

/**
 * @brief Retrieve the current display size setting.
 *
 * This function is only available on devices with native 3DOF support (e.g., Viture Beast).
 * Display size controls the apparent size of the virtual display.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display size value (see VITURE_DISPLAY_SIZE_* constants) in range [0, 4] on success,
 *         negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: USB execution error
 *         - -5: No valid data received
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_get_display_size(XRDeviceProviderHandle handle);

/**
 * @brief Set the display size.
 *
 * This function is only available on Viture Beast devices.
 * Display size controls the apparent size of the virtual display.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param size Display size value (see VITURE_DISPLAY_SIZE_* constants).
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle or invalid size value)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: Size value rejected by device
 *         - -5: USB execution error
 *         - -6: Other error
 */
VITURE_API int xr_device_provider_set_display_size(XRDeviceProviderHandle handle, int size);

/**
 * @brief Recenter the display for native DOF tracking.
 *
 * This function is only available on devices with native 3DOF support (e.g., Viture Beast).
 * It resets the current head pose to the center position for DOF tracking.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return 0 on success, negative error code on failure:
 *         - -1: Invalid parameter (null handle)
 *         - -2: USB communication not available
 *         - -3: Feature not supported by device
 *         - -4: USB execution error
 *         - -5: Other error
 */
VITURE_API int xr_device_provider_native_dof_recenter(XRDeviceProviderHandle handle);

#ifdef __cplusplus
}
#endif

#endif // VITURE_PROTOCOL_PUBLIC_H
