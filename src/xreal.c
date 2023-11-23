#include "device.h"

const device_properties_type xreal_air_properties = {
    .resolution_w           = 1920,
    .resolution_h           = 1080,
    .fov                    = 46.0,
    .lens_distance_ratio    = 0.035,
    .calibration_wait_s     = 15,
    .imu_cycles_per_s       = 1000,
    .imu_buffer_size        = 10,
    .imu_ts_to_ms_factor    = 1000000,
    .look_ahead_constant    = 10.0,
    .look_ahead_frametime_multiplier = 1.25
};