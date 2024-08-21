#include "devices.h"
#include "driver.h"
#include "imu.h"
#include "logging.h"
#include "outputs.h"
#include "runtime_context.h"
#include "sdks/viture_one.h"
#include "strings.h"

#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VITURE_ID_PRODUCT_COUNT 7
#define VITURE_ID_VENDOR 0x35ca
#define VITURE_ONE_MODEL_NAME "One"
#define VITURE_ONE_LITE_MODEL_NAME "One Lite"
#define VITURE_PRO_MODEL_NAME "Pro"
const int viture_supported_id_product[VITURE_ID_PRODUCT_COUNT] = {
    0x1011, // One
    0x1013, // One
    0x1017, // One
    0x1015, // One Lite
    0x101b, // One Lite
    0x1019, // Pro
    0x101d, // Pro
};
const char* viture_supported_models[VITURE_ID_PRODUCT_COUNT] = {
    VITURE_ONE_MODEL_NAME, 
    VITURE_ONE_MODEL_NAME,
    VITURE_ONE_MODEL_NAME,
    VITURE_ONE_LITE_MODEL_NAME,
    VITURE_ONE_LITE_MODEL_NAME,
    VITURE_PRO_MODEL_NAME,
    VITURE_PRO_MODEL_NAME,
};


// VITURE rotations seem to be about 6 degrees (about y axis) off of actual,
// which results in slight twisting about the x axis when looking left/right.
// Use this quaternion to adjust the rotation to balance out the error.
const imu_quat_type adjustment_quat = {
    .w = 0.996,
    .x = 0,
    .y = 0.05235,
    .z = 0
};

const device_properties_type viture_one_properties = {
    .brand                              = "VITURE",
    .model                              = "One",
    .hid_vendor_id                      = 0x35ca,
    .hid_product_id                     = 0x1011,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 39.5,
    .lens_distance_ratio                = 0.023,
    .calibration_wait_s                 = 1,
    .imu_cycles_per_s                   = 60,
    .imu_buffer_size                    = 1,
    .look_ahead_constant                = 20.0,
    .look_ahead_frametime_multiplier    = 0.6,
    .look_ahead_scanline_adjust         = 10.0,
    .look_ahead_ms_cap                  = 40.0,
    .sbs_mode_supported                 = true,
    .firmware_update_recommended        = false
};

const int frequency_enum_to_value[] = {
    [IMU_FREQUENCE_60] = 60,
    [IMU_FREQUENCE_90] = 90,
    [IMU_FREQUENCE_120] = 120,
    [IMU_FREQUENCE_240] = 240
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

static bool old_firmware_version = true;
static bool connected = false;
static bool initialized = false;
void handle_viture_event(uint8_t *data, uint16_t len, uint32_t timestamp) {
    if (!connected || driver_disabled()) return;

    imu_quat_type quat;
    if (len >= 36 && !old_firmware_version) {
        quat.w = float_from_imu_data(data + 20);
        quat.x = float_from_imu_data(data + 24);
        quat.y = float_from_imu_data(data + 28);
        quat.z = float_from_imu_data(data + 32);
    } else {
        float euler_roll = float_from_imu_data(data);
        float euler_pitch = float_from_imu_data(data + 4);
        float euler_yaw = float_from_imu_data(data + 8);

        imu_euler_type euler = {
            .roll = euler_roll,
            .pitch = euler_pitch,
            .yaw = euler_yaw
        };
        quat = zxy_euler_to_quaternion(euler);
    }

    quat = multiply_quaternions(quat, adjustment_quat);

    driver_handle_imu_event(timestamp, quat);
}

bool sbs_mode_enabled = false;
void viture_mcu_callback(uint16_t msgid, uint8_t *data, uint16_t len, uint32_t ts) {
    if (msgid == MCU_SBS_ADJUSTMENT_MSG) {
        sbs_mode_enabled = data[0] == MCU_SBS_ADJUSTMENT_ENABLED;
    }
}

device_properties_type* viture_supported_device(uint16_t vendor_id, uint16_t product_id, uint8_t usb_bus, uint8_t usb_address) {
    if (vendor_id == VITURE_ID_VENDOR) {
        for (int i=0; i < VITURE_ID_PRODUCT_COUNT; i++) {
            if (product_id == viture_supported_id_product[i]) {
                device_properties_type* device = calloc(1, sizeof(device_properties_type));
                *device = viture_one_properties;
                device->hid_vendor_id = vendor_id;
                device->hid_product_id = product_id;
                device->model = (char *)viture_supported_models[i];

                if (equal(VITURE_PRO_MODEL_NAME, device->model)) device->fov = 43.0;

                return device;
            }
        }
    }

    return NULL;
};

static void disconnect(bool soft) {
    if (connected) {
        set_imu(false);

        // VITURE SDK freezes if we attempt deinit() while it's still physically connected, so only do this if the device is no longer present
        if (!soft || !device_present()) {
            deinit();
            initialized = false;
        }
        connected = false;
    }
}

bool viture_device_connect() {
    if (!connected || get_imu_state() != STATE_ON) {
        // newer firmware may require a bit of a wait after the device is plugged in before attempting to connect
        sleep(2);

        if (!initialized) initialized = init(handle_viture_event, viture_mcu_callback);
        connected = initialized && set_imu(true) == ERR_SUCCESS;
    }

    if (connected) {
        device_properties_type* device = device_checkout();
        if (device != NULL) {
            set_imu_fq(IMU_FREQUENCE_240);
            int imu_freq = get_imu_fq();
            if (imu_freq < IMU_FREQUENCE_60 || imu_freq > IMU_FREQUENCE_240) {
                imu_freq = IMU_FREQUENCE_60;
            }

            // use the current value in case the frequency we requested isn't supported
            device->imu_cycles_per_s = frequency_enum_to_value[imu_freq];
            device->imu_buffer_size = (int) device->imu_cycles_per_s / 60;

            // not a great way to check the firmware version but it's all we have
            old_firmware_version = equal(VITURE_PRO_MODEL_NAME, device->model) ? false : (device->imu_cycles_per_s == 60);
            if (old_firmware_version) log_message("VITURE: Detected old firmware version\n");

            device->sbs_mode_supported = !old_firmware_version;
            device->firmware_update_recommended = old_firmware_version;

            sbs_mode_enabled = get_3d_state() == STATE_ON;
        } else {
            disconnect(false);
        }
        device_checkin(device);
    }

    return connected;
}

void viture_block_on_device() {
    device_properties_type* device = device_checkout();
    if (device != NULL) {
        int imu_state = get_imu_state();
        if (connected && imu_state != ERR_WRITE_FAIL) wait_for_imu_start();
        while (connected && imu_state != ERR_WRITE_FAIL) {
            sleep(1);
            imu_state = get_imu_state();
        }
    }
    disconnect(false);
    device_checkin(device);
};

bool viture_device_is_sbs_mode() {
    return sbs_mode_enabled;
};

bool viture_device_set_sbs_mode(bool enabled) {
    sbs_mode_enabled = enabled;
    return set_3d(enabled) == ERR_SUCCESS;
};

bool viture_is_connected() {
    return connected;
};

void viture_disconnect(bool soft) {
    disconnect(soft);
};

const device_driver_type viture_driver = {
    .supported_device_func              = viture_supported_device,
    .device_connect_func                = viture_device_connect,
    .block_on_device_func               = viture_block_on_device,
    .device_is_sbs_mode_func            = viture_device_is_sbs_mode,
    .device_set_sbs_mode_func           = viture_device_set_sbs_mode,
    .is_connected_func                  = viture_is_connected,
    .disconnect_func                    = viture_disconnect
};
