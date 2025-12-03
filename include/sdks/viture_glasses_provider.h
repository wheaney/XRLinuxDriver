/*
 * Copyright (C) 2025 VITURE Inc. All rights reserved.
 */

#ifndef VITURE_GLASSES_PROVIDER_H
#define VITURE_GLASSES_PROVIDER_H

// Handle type for XRDeviceProvider instances
typedef void* XRDeviceProviderHandle;

// Callback to listen to glass state change
typedef void (*GlassStateCallback)(int glass_state_id, int glass_value);

// Device types
typedef enum {
    XR_DEVICE_TYPE_VITURE_GEN1 = 0,
    XR_DEVICE_TYPE_VITURE_GEN2 = 1,
    XR_DEVICE_TYPE_VITURE_CARINA = 2
} XRDeviceType;

/**
 * Create an XRDeviceProvider instance
 * @param product_id Product ID of the opened usb device
 * @param file_descriptor File descriptor of the opened usb device
 * @param bus_num Bus number for viture carina device, ignored for others
 * @param dev_addr Device address for viture carina device, ignored for others
 * @return Handle to the created instance, or NULL on failure
 */
#ifdef __ANDROID__
XRDeviceProviderHandle xr_device_provider_create(int product_id, int file_descriptor, int bus_num, int dev_addr);
#else
XRDeviceProviderHandle xr_device_provider_create(int product_id);
#endif

/**
 * Initialize the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @param custom_config Optional custom configuration string (can be NULL)
 * @return 0: Success, -1: Param error, -2: Other error
 */
int xr_device_provider_initialize(XRDeviceProviderHandle handle, const char* custom_config);

/**
 * Start the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: Other error
 */
int xr_device_provider_start(XRDeviceProviderHandle handle);

/**
 * Stop the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: Other error
 */
int xr_device_provider_stop(XRDeviceProviderHandle handle);

/**
 * Shutdown the XRDeviceProvider
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: Other error
 */
int xr_device_provider_shutdown(XRDeviceProviderHandle handle);

/**
 * Destroy the XRDeviceProvider instance
 * @param handle Handle to the XRDeviceProvider instance
 */
void xr_device_provider_destroy(XRDeviceProviderHandle handle);

/**
 * Register glass state callback
 * @param handle Handle to the XRDeviceProvider instance
 * @param callback Called when glass state changes
 * @return 0: Success, -1: Param error, -2: USB not available, -3: Other error
 */
int xr_device_provider_register_state_callback(XRDeviceProviderHandle handle,
                                               GlassStateCallback callback);

/**
 * Execute a USB control command synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param msgId Message ID for the command
 * @param data Command data to send
 * @param length Length of the command data
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
int xr_device_provider_execute_usb_command(XRDeviceProviderHandle handle,
                                           int msgId,
                                           const char* data,
                                           int length);

/**
 * Execute a USB control command synchronously and return response data
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
int xr_device_provider_execute_usb_command_with_response(XRDeviceProviderHandle handle,
                                                         int msgId,
                                                         const char* data,
                                                         int length,
                                                         char* response_data,
                                                         int* response_length);

/**
 * Set film mode synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param voltage 0 - 1; 0: Off, Other: On (Gen 1)
 * @param voltage 0 - 1; 0: Off, (0, 0.125]: Level 1, ... (0.875, 1]: Level 8 (Gen 2)
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
int xr_device_provider_set_film_mode(XRDeviceProviderHandle handle, float voltage);

/**
 * Get duty cycle
 * @param handle Handle to the XRDeviceProvider instance
 * @return Duty cycle value, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: No valid data, -5: Other error
 */
int xr_device_provider_get_duty_cycle(XRDeviceProviderHandle handle);

/**
 * Set duty cycle synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param duty_cycle Duty cycle value (0-100)
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Duty cycle value incorrect, -5: Other error
 */
int xr_device_provider_set_duty_cycle(XRDeviceProviderHandle handle, int duty_cycle);

/**
 * Get display mode
 * @param handle Handle to the XRDeviceProvider instance
 * @return Display mode value, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: No valid data, -5: Other error
 */
int xr_device_provider_get_display_mode(XRDeviceProviderHandle handle);

/**
 * Set display mode synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @param display_mode Display mode value (see UsbProtocolCommands for valid
 * values)
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Display mode value incorrect, -4: USB execution error
 * @return -5: Other error
 */
int xr_device_provider_set_display_mode(XRDeviceProviderHandle handle, int display_mode);

/**
 * Get display mode and dof type (only for devices with native 3DOF)
 * @param handle Handle to the XRDeviceProvider instance
 * @param display_mode Pointer to store display mode data
 * @param dof_type Pointer to store dof data
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Interface not support for device, -4: USB execution error
 * @return -5: No valid data, -6: Other error
 */
int xr_device_provider_get_display_mode_and_native_dof(XRDeviceProviderHandle handle,
                                                       int* display_mode,
                                                       int* dof_type);

/**
 * Recenter display (only for devices with native 3DOF)
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Interface not support for device, -4: USB execution error
 * @return -5: Other error
 */
int xr_device_provider_native_dof_recenter(XRDeviceProviderHandle handle);

/**
 * Set display mode and dof type (only for devices with native 3DOF)
 * @param handle Handle to the XRDeviceProvider instance
 * @param display_mode Display mode value
 * @param dof_type Dof value
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Interface not support for device, -4: Display mode value incorrect
 * @return -5: USB execution error, -6: Other error
 */
int xr_device_provider_set_display_mode_and_native_dof(XRDeviceProviderHandle handle,
                                                       int display_mode,
                                                       int dof_type);

/**
 * Switch between 2D (1920 x 1080 @ 60Hz) / 3D (3840 x 1080 @ 60Hz)
 * If higher resolution or refresh rate is needed, use xr_device_provider_set_display_mode
 * Note that when already at the same display mode & refresh rate, the return
 * is 0 and nothing will happen (no black screen)
 * @param is_3d true: switch to 3D, false: switch to 2D
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: Display mode value incorrect, -4: USB execution error
 * @return -5: Other error
 */
int xr_device_provider_switch_dimension(XRDeviceProviderHandle handle, bool is_3d);

/**
 * Open imu (no effect for carina device (Luma Ultra)
 * @param handle Handle to the XRDeviceProvider instance
 * @param imu_mode viture::protocol::imu::Mode
 * @param imu_report_frequency viture::protocol::imu::Frequency
 * @return 0: Success, -1: Param error, -2: USB execution error
 * @return -3: Device type not supported, -4: Other error
 */
int xr_device_provider_open_imu(XRDeviceProviderHandle handle,
                                uint8_t imu_mode,
                                uint8_t imu_report_frequency);

/**
 * Close imu (no effect for carina device (Luma Ultra)
 * @param toggle
 * @return 0: Success, -1: Param error, -2: USB execution error
 * @return -3: Device type not supported, -4: Other error
 */
int xr_device_provider_close_imu(XRDeviceProviderHandle handle);

/**
 * Get brightness level
 * @param handle Handle to the XRDeviceProvider instance
 * @return Brightness level, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Response length incorrect
 * @return -5: Response cast error, -6: Other error
 */
int xr_device_provider_get_brightness_level(XRDeviceProviderHandle handle);

/**
 * Set brightness level
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
int xr_device_provider_set_brightness_level(XRDeviceProviderHandle handle, int level);

/**
 * Get volume level
 * @param handle Handle to the XRDeviceProvider instance
 * @return Volume level, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Other error
 */
int xr_device_provider_get_volume_level(XRDeviceProviderHandle handle);

/**
 * Set brightness level
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0: Success, -1: Param error, -2: USB not available
 * @return -3: USB execution error, -4: Response length incorrect,
 * @return -5: Response cast error, -6: Other error
 */
int xr_device_provider_set_volume_level(XRDeviceProviderHandle handle, int level);

/**
 * Get the device type
 * @param handle Handle to the XRDeviceProvider instance
 * @return XRDeviceType enum value, or -1 on failure
 */
int xr_device_provider_get_device_type(XRDeviceProviderHandle handle);

/**
 * Check if product id is valid
 * @param product_id Product id to check
 * @return Product id is valid or not
 */
bool xr_device_provider_is_product_id_valid(int product_id);

#endif // VITURE_GLASSES_PROVIDER_H
