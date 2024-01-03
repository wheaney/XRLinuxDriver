#pragma once

#include "driver.h"

#include <stdbool.h>

enum calibration_setup_t {
    CALIBRATION_SETUP_AUTOMATIC,
    CALIBRATION_SETUP_INTERACTIVE
};
typedef enum calibration_setup_t calibration_setup_type;

struct device_properties_t {
    char* brand;
    char* model;
    int hid_vendor_id;
    int hid_product_id;
    calibration_setup_type calibration_setup;

    // resolution width and height
    int resolution_w;
    int resolution_h;

    // FOV for diagonal, in degrees
    float fov;

    // ratio representing (from the center of the axes of rotation): lens distance / perceived display distance
    float lens_distance_ratio;

    int calibration_wait_s;

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

// open device connection, expected to perform cleanup on failure
typedef device_properties_type* (*device_connect_func)();

// hold open while device is connected and driver_device_should_disconnect() returns false,
// expected to perform cleanup before exiting
typedef void (*block_on_device_func)();

// return true if device is in SBS mode
typedef bool (*device_is_sbs_mode_func)();

// set SBS mode on device, return true on success
typedef bool (*device_set_sbs_mode_func)(bool enabled);

struct device_driver_t {
    device_connect_func device_connect_func;
    block_on_device_func block_on_device_func;
    device_is_sbs_mode_func device_is_sbs_mode_func;
    device_set_sbs_mode_func device_set_sbs_mode_func;
};

typedef struct device_driver_t device_driver_type;