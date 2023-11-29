#pragma once

#include "driver.h"

#include <stdbool.h>

typedef bool (*device_connect_func)(imu_event_handler handler);
typedef void (*block_on_device_func)(should_disconnect_callback should_disconnect_func);
typedef bool (*device_is_sbs_mode_func)();
typedef bool (*device_set_sbs_mode_func)(bool enabled);
typedef void (*device_cleanup_func)();

struct device_properties_t {
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

    device_connect_func device_connect_func;
    block_on_device_func block_on_device_func;
    device_is_sbs_mode_func device_is_sbs_mode_func;
    device_set_sbs_mode_func device_set_sbs_mode_func;
    device_cleanup_func device_cleanup_func;
};

typedef struct device_properties_t device_properties_type;