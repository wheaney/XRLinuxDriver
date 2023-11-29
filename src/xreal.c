#include "device.h"
#include "device3.h"
#include "imu.h"
#include "driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define XREAL_TS_TO_MS_FACTOR 1000000

imu_event_handler event_handler;
void handle_xreal_event(uint64_t timestamp,
		   device3_event_type event,
		   const device3_ahrs_type* ahrs) {
    if (!event_handler) {
        fprintf(stderr, "event_handler not initialized, device_connect should be called first");
        exit(1);
    }

    if (event == DEVICE3_EVENT_UPDATE) {
        device3_quat_type quat = device3_get_orientation(ahrs);
        device3_vec3_type euler = device3_get_euler(quat);
        imu_quat_type imu_quat = { .w = quat.w, .x = quat.x, .y = quat.y, .z = quat.z };
        imu_vector_type imu_vec = { .x = euler.x, .y = euler.y, .z = euler.z };

        event_handler((uint32_t) (timestamp / XREAL_TS_TO_MS_FACTOR), imu_quat, imu_vec);
    }
}

device3_type* glasses_imu;
bool device_connect(imu_event_handler handler) {
    if (!glasses_imu) {
        glasses_imu = malloc(sizeof(device3_type));
    }
    event_handler = handler;

    int device_error = device3_open(glasses_imu, handle_xreal_event);

    return device_error == DEVICE3_ERROR_NO_ERROR;
};

void block_on_device(should_disconnect_callback should_disconnect_func) {
    device3_clear(glasses_imu);
    while (!should_disconnect_func()) {
        if (device3_read(glasses_imu, 1) != DEVICE3_ERROR_NO_ERROR) {
            break;
        }
    }

    device3_close(glasses_imu);
};

bool device_is_sbs_mode() {
    return false;
};

bool device_set_sbs_mode(bool enabled) {
    return false;
};

void device_cleanup() {
    device3_close(glasses_imu);
};

const device_properties_type xreal_air_properties = {
    .resolution_w           = 1920,
    .resolution_h           = 1080,
    .fov                    = 46.0,
    .lens_distance_ratio    = 0.035,
    .calibration_wait_s     = 15,
    .imu_cycles_per_s       = 1000,
    .imu_buffer_size        = 10,
    .look_ahead_constant    = 10.0,
    .look_ahead_frametime_multiplier = 1.25,
    .device_connect_func = device_connect,
    .block_on_device_func = block_on_device,
    .device_is_sbs_mode_func = device_is_sbs_mode,
    .device_set_sbs_mode_func = device_set_sbs_mode,
    .device_cleanup_func = device_cleanup
};
