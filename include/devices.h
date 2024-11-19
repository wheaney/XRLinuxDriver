#pragma once

#include "driver.h"

#include <stdbool.h>
#include <stdint.h>

enum magnet_calibration_t {
    MAGNET_CALIBRATION_UNSUPPORTED = 0,
    MAGNET_CALIBRATION_NONE = 1,
    MAGNET_CALIBRATION_FIGURE_EIGHT = 2
};
typedef enum magnet_calibration_t magnet_calibration_type;

struct device_properties_t {
    char* brand;
    char* model;

    // USB information
    int hid_vendor_id;
    int hid_product_id;
    uint8_t usb_bus;
    uint8_t usb_address;

    bool magnet_supported;
    magnet_calibration_type magnet_calibration_type;
    bool magnet_stale;

    // resolution width and height
    int resolution_w;
    int resolution_h;

    // FOV for diagonal, in degrees
    float fov;

    // ratio representing (from the center of the axes of rotation): lens distance / perceived display distance
    float lens_distance_ratio;

    int gyro_calibration_wait_s;
    int accel_calibration_wait_s;
    int magnet_calibration_wait_s;

    int imu_cycles_per_s;

    // how many events to buffer for velocity smoothing
    int imu_buffer_size;

    // look-ahead = look_ahead_ftm * frametime + look_ahead_constant
    // where frametime is the duration of a frame e.g. 33ms for 30Hz framerate
    float look_ahead_constant;
    float look_ahead_frametime_multiplier;

    // if top and bottom rows of the display require different look-ahead
    float look_ahead_scanline_adjust;

    // maximum look-ahead for best experience, the shader may also provide an upper bound
    float look_ahead_ms_cap;

    bool sbs_mode_supported;
    bool firmware_update_recommended;
};

typedef struct device_properties_t device_properties_type;

// if this driver supports the device, return the device properties, otherwise NULL
typedef device_properties_type* (*supported_device_func)(uint16_t id_vendor, uint16_t id_product, uint8_t usb_bus, uint8_t usb_address);

// open device connection, expected to perform cleanup on failure
typedef bool (*device_connect_func)();

// hold open the connection while the device is present and disconnect_func has not been called
typedef void (*block_on_device_func)();

// return true if device is in SBS mode
typedef bool (*device_is_sbs_mode_func)();

// set SBS mode on device, return true on success
typedef bool (*device_set_sbs_mode_func)(bool enabled);

// kick off different calibration processes
typedef void (*device_calibrate_magnet_func)();
typedef void (*device_disable_magnet_func)();
typedef void (*device_calibrate_gyro_func)();
typedef void (*device_calibrate_accel_func)();

// whether the driver is currently holding open a connection to the device
typedef bool (*is_connected_func)();

// tells the driver to release its connection to the device, `soft` describes whether this is a 
// software-only disconnect, true means the device is still physically connected
typedef void (*disconnect_func)(bool soft);

struct device_driver_t {
    supported_device_func supported_device_func;
    device_connect_func device_connect_func;
    block_on_device_func block_on_device_func;
    device_is_sbs_mode_func device_is_sbs_mode_func;
    device_set_sbs_mode_func device_set_sbs_mode_func;
    device_calibrate_magnet_func device_calibrate_magnet_func;
    device_disable_magnet_func device_disable_magnet_func;
    device_calibrate_gyro_func device_calibrate_gyro_func;
    device_calibrate_accel_func device_calibrate_accel_func;
    is_connected_func is_connected_func;
    disconnect_func disconnect_func;
};

typedef struct device_driver_t device_driver_type;

struct connected_device_t {
    device_driver_type* driver;
    device_properties_type* device;
};

typedef struct connected_device_t connected_device_type;

typedef void (*handle_device_update_func)(connected_device_type* device);

void init_devices(handle_device_update_func callback);

void deinit_devices();

connected_device_type* find_connected_device();

void handle_device_connection_events();