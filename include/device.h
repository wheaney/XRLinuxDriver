#pragma once

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

    int imu_ts_to_ms_factor;

    // look-ahead = look_ahead_ftm * frametime + look_ahead_constant
    // where frametime is the duration of a frame e.g. 33ms for 30Hz framerate
    float look_ahead_constant;
    float look_ahead_frametime_multiplier;
};

typedef struct device_properties_t device_properties_type;