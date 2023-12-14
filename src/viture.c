#include "device.h"
#include "driver.h"
#include "imu.h"
#include "viture_one_sdk.h"

#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const device_properties_type viture_one_properties = {
    .name                               = "VITURE One",
    .hid_vendor_id                      = 0x35ca,
    .hid_product_id                     = 0x1011,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 40.0,
    .lens_distance_ratio                = 0.0275,
    .calibration_wait_s                 = 1,
    .imu_cycles_per_s                   = 60,
    .imu_buffer_size                    = 1,
    .look_ahead_constant                = 10.0,
    .look_ahead_frametime_multiplier    = 0.3,
    .sbs_mode_supported                 = true
};

static float float_from_imu_data(uint8_t *data)
{
	float value = 0;
	uint8_t tem[4];
	tem[0] = data[3];
	tem[1] = data[2];
	tem[2] = data[1];
	tem[3] = data[0];
	memcpy(&value, tem, 4);
	return value;
}

// VITURE seems to need yaw/roll/pitch ordering
// used https://github.com/mrdoob/three.js/blob/dev/src/math/Quaternion.js#L222 as reference
imu_quat_type zxy_euler_to_quaternion(imu_euler_type euler) {
    // Convert degrees to radians
    float roll = degree_to_radian(euler.roll);
    float pitch = degree_to_radian(euler.pitch);
    float yaw = degree_to_radian(euler.yaw);

    // Compute the half angles
    float cx = cos(roll * 0.5f);
    float cy = cos(pitch * 0.5f);
    float cz = cos(yaw * 0.5f);
    float sx = sin(roll * 0.5f);
    float sy = sin(pitch * 0.5f);
    float sz = sin(yaw * 0.5f);

    // Compute the quaternion components
    imu_quat_type q = {
        .x = sx * cy * cz - cx * sy * sz,
        .y = cx * sy * cz + sx * cy * sz,
        .z = cx * cy * sz + sx * sy * cz,
        .w = cx * cy * cz - sx * sy * sz
    };

    return normalize_quaternion(q);
}

void handle_viture_event(uint8_t *data, uint16_t len, uint32_t timestamp) {
    if (driver_disabled()) return;

    float euler_roll = float_from_imu_data(data);
    float euler_pitch = float_from_imu_data(data + 4);
    float euler_yaw = float_from_imu_data(data + 8);

    imu_euler_type euler = {
        .roll = euler_roll,
        .pitch = euler_pitch,
        .yaw = euler_yaw
    };

    imu_quat_type quat = zxy_euler_to_quaternion(euler);

    driver_handle_imu_event(timestamp, quat, euler);
}

void viture_mcu_callback(uint16_t msgid, uint8_t *data, uint16_t len, uint32_t ts) {
    // TODO
}

device_properties_type* viture_device_connect() {
    bool success = init(handle_viture_event, viture_mcu_callback);
    if (success) {
        success = set_imu(true) == ERR_SUCCESS;

        if (success) {
            device_properties_type* device = malloc(sizeof(device_properties_type));
            *device = viture_one_properties;

            return device;
        }
    }

    return NULL;
};

void viture_device_cleanup() {
    set_imu(false);
    deinit();
};

void viture_block_on_device() {
    int imu_state = get_imu_state();
    while (!driver_device_should_disconnect() && imu_state != ERR_WRITE_FAIL) {
        sleep(1);
        imu_state = get_imu_state();
    }

    if (imu_state < 0) {
        fprintf(stderr, "VITURE glasses error %d\n", imu_state);
    }

    viture_device_cleanup();
};

bool viture_device_is_sbs_mode() {
    return get_3d_state() == 1;
};

bool viture_device_set_sbs_mode(bool enabled) {
    return set_3d(enabled);
};

const device_driver_type viture_driver = {
    .device_connect_func                = viture_device_connect,
    .block_on_device_func               = viture_block_on_device,
    .device_is_sbs_mode_func            = viture_device_is_sbs_mode,
    .device_set_sbs_mode_func           = viture_device_set_sbs_mode
};