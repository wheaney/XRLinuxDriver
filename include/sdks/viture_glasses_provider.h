/**
 * @file
 * @brief Public C API for Viture Glasses device providers.
 * @copyright 2025 VITURE Inc. All rights reserved.
 *
 * This header defines the public C API used to create, control and query
 * XR device provider instances. It provides lifecycle functions, command
 * submission entry points, and callback registration for external clients.
 */

#ifndef VITURE_GLASSES_PROVIDER_H
#define VITURE_GLASSES_PROVIDER_H

#include "viture_macros.h"

/**
 * @brief Handle type for XRDeviceProvider instances. This is an opaque pointer
 * returned by `xr_device_provider_create` and consumed by other API calls.
 */
typedef void* XRDeviceProviderHandle;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for reporting glass state changes (brightness, volume, display mode, etc.)
 *
 * See `viture::protocol::Callback::ID` for detailed id and values
 *
 * @param glass_state_id  Identifier for the state being reported.
 * @param glass_value     Integer value associated with the state.
 */
typedef void (*GlassStateCallback)(int glass_state_id, int glass_value);

/**
 * @brief Device types supported by the SDK.
 *
 * Use these values to determine device-specific behavior after creating an
 * `XRDeviceProvider` instance, e.g. registering callbacks.
 */
typedef enum {
    XR_DEVICE_TYPE_VITURE_GEN1 = 0,
    XR_DEVICE_TYPE_VITURE_GEN2 = 1,
    XR_DEVICE_TYPE_VITURE_CARINA = 2
} XRDeviceType;

#ifdef __ANDROID__
/**
 * @brief Android variant: Create an XRDeviceProvider instance
 *
 * Due to Android restrictions, detailed usb information is needed for creation
 *
 * @param product_id Product ID of the opened usb device
 * @param file_descriptor File descriptor of the opened usb device
 * @param bus_num Bus number for viture carina device
 * @param dev_addr Device address for viture carina device
 * @return Handle to the created instance, or NULL on failure
 */
VITURE_API XRDeviceProviderHandle xr_device_provider_create(int product_id,
                                                            int file_descriptor,
                                                            int bus_num,
                                                            int dev_addr);
#else
/**
 * @brief Non-Android variant: Create an XRDeviceProvider instance
 *
 * The non-Android signature accepts only `product_id`
 *
 * @param product_id Product ID of the opened usb device
 * @return Handle to the created instance, or NULL on failure
 */
VITURE_API XRDeviceProviderHandle xr_device_provider_create(int product_id);
#endif

/**
 * @brief Initialize the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @param custom_config Optional custom configuration string (can be NULL)
 * @return 0: Success, -1: Param error, -2: Other error
 */
VITURE_API int xr_device_provider_initialize(XRDeviceProviderHandle handle, const char* custom_config);

/**
 * @brief Start the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: Other error
 */
VITURE_API int xr_device_provider_start(XRDeviceProviderHandle handle);

/**
 * @brief Stop the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: Other error
 */
VITURE_API int xr_device_provider_stop(XRDeviceProviderHandle handle);

/**
 * @brief Shutdown the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: Other error
 */
VITURE_API int xr_device_provider_shutdown(XRDeviceProviderHandle handle);

/**
 * @brief Destroy the XRDeviceProvider instance
 * @param handle Handle to the XRDeviceProvider instance
 */
VITURE_API void xr_device_provider_destroy(XRDeviceProviderHandle handle);

/**
 * @brief Register glass state callback
 * @param handle Handle to the XRDeviceProvider instance
 * @param callback Called when glass state changes
 * @return 0: Success, -1: Param error, -2: USB not available, -3: Other error
 */
VITURE_API int xr_device_provider_register_state_callback(XRDeviceProviderHandle handle, GlassStateCallback callback);

/**
 * @brief Execute a USB control command synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param msgId Message ID for the command
 * @param data Command data to send
 * @param length Length of the command data
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
VITURE_API int xr_device_provider_execute_usb_command(XRDeviceProviderHandle handle,
                                                      int msgId,
                                                      const char* data,
                                                      int length);

/**
 * @brief Execute a USB control command synchronously and return response data
 * @param handle Handle to the XRDeviceProvider instance
 * @param msgId Message ID for the command
 * @param data Command data to send
 * @param length Length of the command data
 * @param response_data Buffer to store response data
 * @param response_length Pointer to store the length of response data
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Result cast success but execution is not success
 * @return -5: Result not casteded and execution is not success, -6: Other error
 */
VITURE_API int xr_device_provider_execute_usb_command_with_response(
    XRDeviceProviderHandle handle, int msgId, const char* data, int length, char* response_data, int* response_length);

/**
 * @brief Set electrochomic film mode synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param voltage Voltage parameter (0.0 - 1.0). Interpretation differs by
 *                generation: on Gen1 a non-zero value enables the film;
 *                Gen2 uses ranges to map to discrete levels.
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
VITURE_API int xr_device_provider_set_film_mode(XRDeviceProviderHandle handle, float voltage);

/**
 * @brief Get screen duty cycle
 * @param handle Handle to the XRDeviceProvider instance
 * @return Duty cycle value, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: No valid data, -5: Other error
 */
VITURE_API int xr_device_provider_get_duty_cycle(XRDeviceProviderHandle handle);

/**
 * @brief Set screen duty cycle
 * @param handle Handle to the XRDeviceProvider instance
 * @param duty_cycle Duty cycle value (0-100)
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Duty cycle value incorrect, -5: Other error
 */
VITURE_API int xr_device_provider_set_duty_cycle(XRDeviceProviderHandle handle, int duty_cycle);

/**
 * @brief Get display mode
 * @param handle Handle to the XRDeviceProvider instance
 * @return Display mode value (see viture::protocol::DisplayMode), -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: No valid data, -5: Other error
 */
VITURE_API int xr_device_provider_get_display_mode(XRDeviceProviderHandle handle);

/**
 * @brief Set display mode synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param display_mode Display mode value (see viture::protocol::DisplayMode)
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Display mode value incorrect, -4: USB execution error
 * @return -5: Other error
 */
VITURE_API int xr_device_provider_set_display_mode(XRDeviceProviderHandle handle, int display_mode);

/**
 * @brief Get display mode and native dof type (only for devices with native 3DOF, e.g. Viture Beast)
 * @param handle Handle to the XRDeviceProvider instance
 * @param display_mode Pointer to store display mode data
 * @param dof_type Pointer to store dof data
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Interface not support for device, -4: USB execution error
 * @return -5: No valid data, -6: Other error
 */
VITURE_API int xr_device_provider_get_display_mode_and_native_dof(XRDeviceProviderHandle handle,
                                                                  int* display_mode,
                                                                  int* dof_type);

/**
 * @brief Set display mode and dof type (only for devices with native 3DOF, e.g. Viture Beast)
 * @param handle Handle to the XRDeviceProvider instance
 * @param display_mode Display mode value
 * @param dof_type Dof value
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Interface not support for device, -4: Display mode value incorrect
 * @return -5: USB execution error, -6: Other error
 */
VITURE_API int xr_device_provider_set_display_mode_and_native_dof(XRDeviceProviderHandle handle,
                                                                  int display_mode,
                                                                  int dof_type);

/**
 * @brief Recenter display (only for devices with native 3DOF, e.g. Viture Beast)
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Interface not support for device, -4: USB execution error
 * @return -5: Other error
 */
VITURE_API int xr_device_provider_native_dof_recenter(XRDeviceProviderHandle handle);

/**
 * @brief Switch between 2D (1920 x 1080 @ 60Hz) / 3D (3840 x 1080 @ 60Hz)
 *
 * If higher resolution or refresh rate is needed, use xr_device_provider_set_display_mode
 * Note that when already at the same display mode and refresh rate, the return
 * is 0 and nothing will happen
 *
 * @param handle Handle to the XRDeviceProvider instance
 * @param is_3d true: switch to 3D, false: switch to 2D
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Display mode value incorrect, -4: USB execution error
 * @return -5: Other error
 */
VITURE_API int xr_device_provider_switch_dimension(XRDeviceProviderHandle handle, bool is_3d);

/**
 * @brief Get screen brightness level
 * @param handle Handle to the XRDeviceProvider instance
 * @return Brightness level, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Response length incorrect
 * @return -5: Response cast error, -6: Other error
 */
VITURE_API int xr_device_provider_get_brightness_level(XRDeviceProviderHandle handle);

/**
 * @brief Set screen brightness level
 * @param handle Handle to the XRDeviceProvider instance
 * @param level Brightness level to set
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
VITURE_API int xr_device_provider_set_brightness_level(XRDeviceProviderHandle handle, int level);

/**
 * @brief Get speaker volume level
 * @param handle Handle to the XRDeviceProvider instance
 * @return Volume level, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
VITURE_API int xr_device_provider_get_volume_level(XRDeviceProviderHandle handle);

/**
 * @brief Set speaker volume level
 * @param handle Handle to the XRDeviceProvider instance
 * @param level Volume level to set
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Response length incorrect,
 * @return -5: Response cast error, -6: Other error
 */
VITURE_API int xr_device_provider_set_volume_level(XRDeviceProviderHandle handle, int level);

/**
 * @brief Get the device type
 * @param handle Handle to the XRDeviceProvider instance
 * @return XRDeviceType enum value, or -1 on failure
 */
VITURE_API int xr_device_provider_get_device_type(XRDeviceProviderHandle handle);

/**
 * Check if product id is valid
 * @param product_id Product id to check
 * @return Product id is valid or not
 */
VITURE_API bool xr_device_provider_is_product_id_valid(int product_id);

/**
 * Get glasses market name
 * @param product_id Viture product id
 * @param response_data Buffer to store market name
 * @param response_length Pointer to store the length of market name
 */
VITURE_API int xr_device_provider_get_market_name(int product_id, char* market_name, int* length);

#ifdef __cplusplus
}
#endif

#endif // VITURE_GLASSES_PROVIDER_H
