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

#define VITURE_ID_PRODUCT_COUNT 10
#define VITURE_ID_VENDOR 0x35ca
#define VITURE_ONE_MODEL_NAME "One"
#define VITURE_ONE_LITE_MODEL_NAME "One Lite"
#define VITURE_PRO_MODEL_NAME "Pro"
#define VITURE_LUMA_MODEL_NAME "Luma"
#define VITURE_LUMA_PRO_MODEL_NAME "Luma Pro"

const float VITURE_ONE_PITCH_ADJUSTMENT = 6.0;
const float VITURE_PRO_PITCH_ADJUSTMENT = 3.0;
const float VITURE_LUMA_PITCH_ADJUSTMENT = -8.5;

const float VITURE_ONE_FOV = 40.0;
const float VITURE_PRO_FOV = 43.0;
const float VITURE_LUMA_FOV = 47.0;
const float VITURE_LUMA_PRO_FOV = 49.0;

const int viture_supported_id_product[VITURE_ID_PRODUCT_COUNT] = {
    0x1011, // One
    0x1013, // One
    0x1017, // One
    0x1015, // One Lite
    0x101b, // One Lite
    0x1019, // Pro
    0x101d, // Pro
    0x1121, // Luma Pro
    0x1131, // Luma
    0x1141  // Luma Pro
};
const char* viture_supported_models[VITURE_ID_PRODUCT_COUNT] = {
    VITURE_ONE_MODEL_NAME, 
    VITURE_ONE_MODEL_NAME,
    VITURE_ONE_MODEL_NAME,
    VITURE_ONE_LITE_MODEL_NAME,
    VITURE_ONE_LITE_MODEL_NAME,
    VITURE_PRO_MODEL_NAME,
    VITURE_PRO_MODEL_NAME,
    VITURE_LUMA_PRO_MODEL_NAME,
    VITURE_LUMA_MODEL_NAME,
    VITURE_LUMA_PRO_MODEL_NAME
};
const float* viture_pitch_adjustments[VITURE_ID_PRODUCT_COUNT] = {
    &VITURE_ONE_PITCH_ADJUSTMENT, // One
    &VITURE_ONE_PITCH_ADJUSTMENT, // One
    &VITURE_ONE_PITCH_ADJUSTMENT, // One
    &VITURE_ONE_PITCH_ADJUSTMENT, // One Lite
    &VITURE_ONE_PITCH_ADJUSTMENT, // One Lite
    &VITURE_PRO_PITCH_ADJUSTMENT, // Pro
    &VITURE_PRO_PITCH_ADJUSTMENT, // Pro
    &VITURE_LUMA_PITCH_ADJUSTMENT, // Luma Pro
    &VITURE_LUMA_PITCH_ADJUSTMENT, // Luma
    &VITURE_LUMA_PITCH_ADJUSTMENT  // Luma Pro
};
const float* viture_fovs[VITURE_ID_PRODUCT_COUNT] = {
    &VITURE_ONE_FOV, // One
    &VITURE_ONE_FOV, // One
    &VITURE_ONE_FOV, // One
    &VITURE_ONE_FOV, // One Lite
    &VITURE_ONE_FOV, // One Lite
    &VITURE_PRO_FOV, // Pro
    &VITURE_PRO_FOV, // Pro
    &VITURE_LUMA_PRO_FOV, // Luma Pro
    &VITURE_LUMA_FOV, // Luma
    &VITURE_LUMA_PRO_FOV  // Luma Pro
};
const int viture_resolution_heights[VITURE_ID_PRODUCT_COUNT] = {
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // One Lite
    RESOLUTION_1080P_H, // One Lite
    RESOLUTION_1080P_H, // Pro
    RESOLUTION_1080P_H, // Pro
    RESOLUTION_1200P_H, // Luma Pro
    RESOLUTION_1200P_H, // Luma
    RESOLUTION_1200P_H  // Luma Pro
};

static imu_quat_type adjustment_quat;

const device_properties_type viture_one_properties = {
    .brand                              = "VITURE",
    .model                              = "One",
    .hid_vendor_id                      = 0x35ca,
    .hid_product_id                     = 0x1011,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = RESOLUTION_1080P_W,
    .resolution_h                       = RESOLUTION_1080P_H,
    .fov                                = VITURE_ONE_FOV,
    .lens_distance_ratio                = 0.05,
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

const char* frequency_to_string[] = {
    [IMU_FREQUENCE_60] = "60Hz",
    [IMU_FREQUENCE_90] = "90Hz",
    [IMU_FREQUENCE_120] = "120Hz",
    [IMU_FREQUENCE_240] = "240Hz"
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
        quat = euler_to_quaternion_zxy(euler);
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
                device->resolution_h = viture_resolution_heights[i];
                device->fov = *viture_fovs[i];

                adjustment_quat = device_pitch_adjustment(*viture_pitch_adjustments[i]);

                return device;
            }
        }
    }

    return NULL;
};

static pthread_mutex_t viture_connection_mutex = PTHREAD_MUTEX_INITIALIZER;
static void disconnect(bool soft) {
    pthread_mutex_lock(&viture_connection_mutex);
    if (connected) {
        set_imu(false);
        connected = false;
    }

    // VITURE SDK freezes if we attempt deinit() while it's still physically connected, so only do this if the device is no longer present
    if (initialized && (!soft || !device_present())) {
        deinit();
        initialized = false;
    }
    pthread_mutex_unlock(&viture_connection_mutex);
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

            if (config()->debug_device) log_debug("VITURE: IMU frequency set to %s\n", frequency_to_string[imu_freq]);

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
    disconnect(true);
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
