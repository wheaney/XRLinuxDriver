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
// Display Configuration Constants
// ============================================================================

/**
 * @brief Display mode identifiers for configuring device display output and refresh rates.
 *
 * Display modes specify both resolution and refresh rate. Standard modes are available
 * on Gen 1 / Carina devices and Gen 2 device in bypass mode.
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
// Native DOF Configuration Constants
// ============================================================================

/**
 * @brief Native DOF (Degrees of Freedom) configuration constants.
 *
 * These constants are only available on devices with native DOF support (e.g., Viture Beast).
 * Native DOF enables head tracking and advanced display features.
 */

/**
 * @brief Display size identifiers for native DOF devices.
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
 * These modes control the native DOF tracking behavior.
 */
#define VITURE_NATIVE_DOF_0             0x00 /**< No native DOF tracking */
#define VITURE_NATIVE_DOF_3             0x01 /**< Native 3DOF tracking enabled */
#define VITURE_NATIVE_DOF_SMOOTH_FOLLOW 0x02 /**< Smooth follow mode */

/**
 * @brief Display modes for devices with native DOF support (e.g., Viture Beast).
 *
 * These modes are used when native DOF tracking is active. They define a separate
 * opcode space from the standard VITURE_DISPLAY_MODE_* constants.
 */
#define VITURE_NATIVE_DISPLAY_MODE_1920_1080_60HZ            0x31 /**< 1920x1080 @ 60Hz */
#define VITURE_NATIVE_DISPLAY_MODE_1920_1080_90HZ            0x32 /**< 1920x1080 @ 90Hz */
#define VITURE_NATIVE_DISPLAY_MODE_1920_1080_120HZ           0x33 /**< 1920x1080 @ 120Hz */
#define VITURE_NATIVE_DISPLAY_MODE_1920_1200_60HZ            0x34 /**< 1920x1200 @ 60Hz */
#define VITURE_NATIVE_DISPLAY_MODE_1920_1200_90HZ            0x35 /**< 1920x1200 @ 90Hz */
#define VITURE_NATIVE_DISPLAY_MODE_1920_1200_120HZ           0x36 /**< 1920x1200 @ 120Hz */
#define VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_60HZ     0x37 /**< 3D SBS 3840x1080 @ 60Hz */
#define VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_90HZ     0x38 /**< 3D SBS 3840x1080 @ 90Hz */
#define VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_120HZ    0x39 /**< 3D SBS 3840x1080 @ 120Hz */
#define VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1200_60HZ     0x3A /**< 3D SBS 3840x1200 @ 60Hz */
#define VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1200_90HZ     0x3B /**< 3D SBS 3840x1200 @ 90Hz */
#define VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1200_120HZ    0x3C /**< 3D SBS 3840x1200 @ 120Hz */
#define VITURE_NATIVE_DISPLAY_MODE_ULTRAWIDE_3840_1080_60HZ  0x3D /**< Ultrawide 3840x1080 @ 60Hz */
#define VITURE_NATIVE_DISPLAY_MODE_ULTRAWIDE_3840_1080_90HZ  0x3E /**< Ultrawide 3840x1080 @ 90Hz */
#define VITURE_NATIVE_DISPLAY_MODE_ULTRAWIDE_3840_1080_120HZ 0x3F /**< Ultrawide 3840x1080 @ 120Hz */
#define VITURE_NATIVE_DISPLAY_MODE_ULTRAWIDE_3840_1200_60HZ  0x40 /**< Ultrawide 3840x1200 @ 60Hz */
#define VITURE_NATIVE_DISPLAY_MODE_ULTRAWIDE_3840_1200_90HZ  0x41 /**< Ultrawide 3840x1200 @ 90Hz */
#define VITURE_NATIVE_DISPLAY_MODE_ULTRAWIDE_3840_1200_120HZ 0x42 /**< Ultrawide 3840x1200 @ 120Hz */

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
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or null voltage pointer
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
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
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or voltage out of range
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_set_film_mode(XRDeviceProviderHandle handle, float voltage);

/**
 * @brief Retrieve the current screen duty cycle.
 *
 * Duty cycle controls display brightness by adjusting the percentage of time
 * that pixels are active. Higher values result in brighter displays.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Duty cycle value in range [0, 100] on success, or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
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
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or value out of range
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_set_duty_cycle(XRDeviceProviderHandle handle, int duty_cycle);

/**
 * @brief Retrieve the current display mode.
 *
 * Display mode determines both the resolution and refresh rate of the device output.
 * For Gen2 devices (Beast), use this interface when in bypass mode.
 * See VITURE_DISPLAY_MODE_* constants for valid mode values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display mode value (see VITURE_DISPLAY_MODE_* constants) on success,
 *         or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   Gen2 device is in native mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_get_display_mode(XRDeviceProviderHandle handle);

/**
 * @brief Set the display mode.
 *
 * Display mode determines both the resolution and refresh rate of the device output.
 * For Gen2 devices (Beast), use this interface when in bypass mode.
 * See VITURE_DISPLAY_MODE_* constants for valid mode values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param display_mode Display mode value (see VITURE_DISPLAY_MODE_* constants).
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or invalid mode value
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   Gen2 device is in native mode, or mode value unsupported
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_set_display_mode(XRDeviceProviderHandle handle, int display_mode);

/**
 * @brief Switch between 2D and 3D display modes (Gen1 and Gen2 bypass mode only).
 *
 * This is a convenience function that switches between:
 * - 2D mode: 1920x1080 @ 60Hz
 * - 3D mode: 3840x1080 @ 60Hz
 *
 * For Gen2 devices (Beast) in native mode, use xr_device_provider_native_switch_dimension()
 * instead. Calling this function in such state returns VITURE_GLASSES_ERROR_NOT_SUPPORTED.
 *
 * @note If the device is already in the requested mode, the function returns
 *       successfully (0) without performing any action.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param is_3d 1 to switch to 3D mode, 0 to switch to 2D mode.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device is Gen2 (use native_switch_dimension instead)
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_switch_dimension(XRDeviceProviderHandle handle, int is_3d);

/**
 * @brief Retrieve the current screen brightness level.
 *
 * Brightness levels vary by device model. For device-specific value ranges,
 * see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Brightness level on success, or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DATA_PARSE      response length or format mismatch
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
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
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or value out of range
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_set_brightness_level(XRDeviceProviderHandle handle, int level);

/**
 * @brief Retrieve the current speaker volume level.
 *
 * Volume levels vary by device model. For device-specific value ranges,
 * see @ref ValueRanges.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Volume level on success, or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DATA_PARSE      response length or format mismatch
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
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
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or value out of range
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_set_volume_level(XRDeviceProviderHandle handle, int level);

/**
 * @brief Retrieve the glasses firmware version string.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param response Output buffer to store the version string. Must be pre-allocated.
 * @param length Input/output parameter: on input, specifies the buffer size;
 *              on output, contains the actual length of the version string.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle, null response, or null length
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_get_glasses_version(XRDeviceProviderHandle handle, char* response, int* length);

/**
 * @brief Retrieve the SHA-256 hash of the board serial number.
 *
 * The raw serial number is never exposed. The caller receives a 32-byte
 * SHA-256 digest that uniquely identifies the device and can be used for
 * device binding or license validation without leaking the actual SN.
 *
 * @param handle    Valid XRDeviceProvider handle.
 * @param hash_out  Caller-allocated buffer of at least 32 bytes to receive the digest.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or null hash_out
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_get_sn_hash(XRDeviceProviderHandle handle, uint8_t* hash_out);

// ============================================================================
// Native DOF Functions
// ============================================================================

/**
 * @brief Retrieve the current mode for Gen2 devices (bypass / native).
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Returns whether the device is operating in bypass mode or native mode:
 * - 0 (bypass mode): native DOF tracking is disabled; use xr_device_provider_get/set_display_mode
 *   for display mode control.
 * - 1 (native mode): native DOF tracking is active; use xr_device_provider_native_get/set_display_mode
 *   for display mode control.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return 0 (bypass) or 1 (native) on success, or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   feature not supported by device
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_get_mode(XRDeviceProviderHandle handle);

/**
 * @brief Set the native mode for Gen2 devices (bypass / native).
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * After calling this function, call xr_device_provider_native_set_display_mode() to complete
 * the mode switch by setting an appropriate display mode for the new mode.
 * Controls whether the device operates in bypass mode or native mode:
 * - 0 (bypass mode): disables native DOF tracking; display mode is controlled via
 *   xr_device_provider_get/set_display_mode using standard VITURE_DISPLAY_MODE_* values.
 * - 1 (native mode): enables native DOF tracking; display mode is controlled via
 *   xr_device_provider_native_get/set_display_mode using VITURE_NATIVE_DISPLAY_MODE_* values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param mode 0 for bypass mode, 1 for native mode.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   feature not supported by device
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device rejected the command
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_set_mode(XRDeviceProviderHandle handle, int mode);

/**
 * @brief Retrieve the current native DOF tracking type on Gen2 devices.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Returns the active DOF tracking type when the device is in native mode.
 * See VITURE_NATIVE_DOF_* constants for valid values (e.g., VITURE_NATIVE_DOF_3,
 * VITURE_NATIVE_DOF_SMOOTH_FOLLOW).
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return DOF tracking type (see VITURE_NATIVE_DOF_* constants) on success,
 *         or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_get_dof(XRDeviceProviderHandle handle);

/**
 * @brief Set the native DOF tracking type on Gen2 devices.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Sets the active DOF tracking type when the device is in native mode.
 * See VITURE_NATIVE_DOF_* constants for valid values (e.g., VITURE_NATIVE_DOF_3,
 * VITURE_NATIVE_DOF_SMOOTH_FOLLOW).
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param dof DOF tracking type to set (see VITURE_NATIVE_DOF_* constants).
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device rejected the command
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_set_dof(XRDeviceProviderHandle handle, int dof);

/**
 * @brief Recenter the display for native DOF tracking.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * It resets the current head pose to the center position for DOF tracking when the device is in native mode.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_recenter_dof(XRDeviceProviderHandle handle);

/**
 * @brief Retrieve the current display mode for Gen2 devices.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Use this when the device is operating in native DOF mode (not bypass).
 * See VITURE_NATIVE_DISPLAY_MODE_* constants for valid mode values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display mode value (see VITURE_NATIVE_DISPLAY_MODE_* constants) on success,
 *         or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_get_display_mode(XRDeviceProviderHandle handle);

/**
 * @brief Set the display mode for native DOF operation on Gen2 devices.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Use this when the device is operating in native DOF mode (not bypass).
 * See VITURE_NATIVE_DISPLAY_MODE_* constants for valid mode values.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param display_mode Display mode value (see VITURE_NATIVE_DISPLAY_MODE_* constants).
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or invalid mode value
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_set_display_mode(XRDeviceProviderHandle handle, int display_mode);

/**
 * @brief Retrieve the current side mode for Gen2 devices in native mode.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast)
 * and requires the device to be in native mode (mode == 1).
 * Side mode shifts the displayed image to one side of the glasses.
 * - 0: side mode disabled
 * - 1: side mode enabled
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return 0 (disabled) or 1 (enabled) on success, or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   feature not supported, or device is not in native mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_get_side_mode(XRDeviceProviderHandle handle);

/**
 * @brief Set the side mode for Gen2 devices in native mode.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast)
 * and requires the device to be in native mode (mode == 1).
 * Side mode shifts the displayed image to one side of the glasses.
 * - 0: disable side mode
 * - 1: enable side mode
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param side_mode 0 to disable side mode, 1 to enable side mode.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   feature not supported, or device is not in native mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device rejected the command
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_set_side_mode(XRDeviceProviderHandle handle, int side_mode);

/**
 * @brief Switch between 2D and 3D display modes in native DOF mode on Gen2 devices.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Switches between:
 * - 2D mode: VITURE_NATIVE_DISPLAY_MODE_1920_1080_60HZ
 * - 3D mode: VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_60HZ
 * when the device is operating in native mode.
 *
 * @note If the device is already in the requested mode, the function returns
 *       successfully (0) without performing any action.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param is_3d 1 to switch to 3D mode, 0 to switch to 2D mode.
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_switch_dimension(XRDeviceProviderHandle handle, int is_3d);

/**
 * @brief Retrieve the current display distance setting.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Display distance controls the virtual distance of the display content when the device is operating in native mode.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display distance value in range [1, 10] on success, or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_get_display_distance(XRDeviceProviderHandle handle);

/**
 * @brief Set the display distance.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Display distance controls the virtual distance of the display content when the device is operating in native mode.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param distance Display distance value in range [1, 10].
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or value out of range
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_set_display_distance(XRDeviceProviderHandle handle, int distance);

/**
 * @brief Retrieve the current display size setting.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Display size controls the apparent size of the virtual display when the device is operating in native mode.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @return Display size value (see VITURE_DISPLAY_SIZE_* constants) in range [0, 4] on success,
 *         or a negative error code:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_DEVICE_REJECTED device returned error status
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_get_display_size(XRDeviceProviderHandle handle);

/**
 * @brief Set the display size.
 *
 * This function is only available on devices with native DOF support (e.g., Viture Beast).
 * Display size controls the apparent size of the virtual display when the device is operating in native mode.
 *
 * @param handle Valid XRDeviceProvider handle.
 * @param size Display size value (see VITURE_DISPLAY_SIZE_* constants).
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM   null handle or invalid size value
 *         - VITURE_GLASSES_ERROR_USB_UNAVAILABLE USB not available
 *         - VITURE_GLASSES_ERROR_NOT_SUPPORTED   device does not support native DOF, or device is in bypass mode
 *         - VITURE_GLASSES_ERROR_USB_EXEC        USB execution error
 *         - VITURE_GLASSES_ERROR_UNKNOWN         other error
 */
VITURE_API int xr_device_provider_native_set_display_size(XRDeviceProviderHandle handle, int size);

#ifdef __cplusplus
}
#endif

#endif // VITURE_PROTOCOL_PUBLIC_H
