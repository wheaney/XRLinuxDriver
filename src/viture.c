#include "device.h"
#include "viture_one_sdk.h"
#include "imu.h"
#include "driver.h"

#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const device_properties_type viture_one_properties = {
    .hid_vendor_id                      = 0,
    .hid_product_id                     = 0,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 46.0,
    .lens_distance_ratio                = 0.035,
    .calibration_wait_s                 = 1,
    .imu_cycles_per_s                   = 60,
    .imu_buffer_size                    = 1,
    .look_ahead_constant                = 10.0,
    .look_ahead_frametime_multiplier    = 0.3
};

const imu_quat_type conversion_quat = {.x = 0.5, .y = -0.5, .z = -0.5, .w = 0.5};

static float makeFloat(uint8_t *data)
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

float degToRad(float deg) {
    return deg * M_PI / 180.0f;
}

imu_quat_type eulerToQuaternion(imu_vector_type euler) {
    // Convert degrees to radians
    float x = degToRad(euler.x);
    float y = degToRad(euler.y);
    float z = degToRad(euler.z);

    imu_quat_type q;

    // Compute the half angles
    float cx = cos(x * 0.5f);
    float cy = cos(y * 0.5f);
    float cz = cos(z * 0.5f);
    float sx = sin(x * 0.5f);
    float sy = sin(y * 0.5f);
    float sz = sin(z * 0.5f);

    // Compute the quaternion components
    q.w = cx * cy * cz + sx * sy * sz;
    q.x = sx * cy * cz - cx * sy * sz;
    q.y = cx * sy * cz + sx * cy * sz;
    q.z = cx * cy * sz - sx * sy * cz;

    return q;
}

imu_event_handler viture_event_handler;
void handle_viture_event(uint8_t *data, uint16_t len, uint32_t timestamp) {
    if (!viture_event_handler) {
        fprintf(stderr, "viture_event_handler not initialized, device_connect should be called first\n");
    } else {
        float eulerRoll = makeFloat(data);
        float eulerPitch = makeFloat(data + 4);
        float eulerYaw = makeFloat(data + 8);

        imu_vector_type euler = {
            .x = eulerRoll,
            .y = eulerPitch,
            .z = eulerYaw
        };

        imu_quat_type imu_quat = eulerToQuaternion(euler);
        imu_quat_type converted_quat = multiply_quaternions(imu_quat, conversion_quat);
        imu_vector_type converted_euler = quaternion_to_euler(converted_quat);

        viture_event_handler(timestamp, converted_quat, converted_euler);
    }
}

void viture_mcu_callback(uint16_t msgid, uint8_t *data, uint16_t len, uint32_t ts) {
    // TODO
}

device_properties_type* viture_device_connect(imu_event_handler handler) {
    bool success = init(handle_viture_event, viture_mcu_callback);
    if (success) {
        viture_event_handler = handler;
        set_imu(true);

        device_properties_type* device = malloc(sizeof(device_properties_type));
        *device = viture_one_properties;

        // TODO - set these to the real values
        device->hid_product_id = 0;
        device->hid_vendor_id = 0;

        return device;
    }

    return NULL;
};

void viture_device_cleanup() {
    set_imu(false);
    deinit();
};

void viture_block_on_device(should_disconnect_callback should_disconnect_func) {
    while (!should_disconnect_func() && get_imu_state() == 1) {
        sleep(1);
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
    .device_set_sbs_mode_func           = viture_device_set_sbs_mode,
    .device_cleanup_func                = viture_device_cleanup
};