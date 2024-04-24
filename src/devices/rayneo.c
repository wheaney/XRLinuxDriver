#include "devices.h"
#include "devices/rayneo.h"
#include "driver.h"
#include "imu.h"
#include "sdks/rayneo.h"

#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_TO_MS_FACTOR 1000000
#define EXPECTED_CYCLES_PER_S 500
#define FORCED_CYCLES_PER_S 250 // let's force 250Hz cycle time so we're doing fewer computations
#define CYCLE_TIME_CHECK_ERROR_FACTOR 0.95 // cycle times won't be exact, check within a 5% margin
#define FORCED_CYCLE_TIME_MS 1000.0 / FORCED_CYCLES_PER_S * CYCLE_TIME_CHECK_ERROR_FACTOR
#define BUFFER_SIZE_TARGET_MS 10 // smooth IMU data over this period of time

#define RAYNEO_ID_PRODUCT_COUNT 1
#define RAYNEO_ID_VENDOR 0x1bbb
const int rayneo_supported_id_product[RAYNEO_ID_PRODUCT_COUNT] = {
    0xaf50
};

static const imu_quat_type adjustment_quat = {
    .w = 0.561,
    .x = -0.430,
    .y = 0.430,
    .z = 0.561
};

const device_properties_type rayneo_properties = {
    .brand                              = "RayNeo",
    .model                              = "Air 2",
    .hid_vendor_id                      = 0x1bbb,
    .hid_product_id                     = 0xaf50,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 46,
    .lens_distance_ratio                = 0.025,
    .calibration_wait_s                 = 5,
    .imu_cycles_per_s                   = FORCED_CYCLES_PER_S,
    .imu_buffer_size                    = ceil(BUFFER_SIZE_TARGET_MS / FORCED_CYCLE_TIME_MS),
    .look_ahead_constant                = 10.0,
    .look_ahead_frametime_multiplier    = 0.3,
    .look_ahead_scanline_adjust         = 8.0,
    .look_ahead_ms_cap                  = 40.0,
    .sbs_mode_supported                 = true,
    .firmware_update_recommended        = false
};

static uint32_t last_utilized_event_ts = 0;
static bool connected = false;
void rayneo_imu_callback(const float acc[3], const float gyro[3], const float mag[3], uint64_t timestamp){
    if (!connected || driver_disabled()) return;

    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    uint32_t elapsed_from_last_utilized = ts - last_utilized_event_ts;
    if (elapsed_from_last_utilized > FORCED_CYCLE_TIME_MS) {
        float rotation[4];
        float position[3];
        uint64_t time;
        GetHeadTrackerPose(rotation, position, &time);

        imu_quat_type imu_quat = { .w = rotation[3], .x = rotation[0], .y = rotation[1], .z = rotation[2] };
        imu_quat_type nwu_quat = multiply_quaternions(imu_quat, adjustment_quat);
        imu_euler_type nwu_euler = quaternion_to_euler(nwu_quat);
        driver_handle_imu_event(ts, nwu_quat, nwu_euler);

        last_utilized_event_ts = ts;
    }
}

static void rayneo_mcu_callback(uint32_t state, uint64_t timestamp, size_t length, const void* data) {
    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    printf("MCU callback: state %d, ts %d, length %d\n", state, ts, length);
    if (!connected || driver_disabled()) return;
}

device_properties_type* rayneo_supported_device(uint16_t vendor_id, uint16_t product_id) {
    if (vendor_id == RAYNEO_ID_VENDOR) {
        for (int i=0; i < RAYNEO_ID_PRODUCT_COUNT; i++) {
            if (product_id == rayneo_supported_id_product[i]) {
                device_properties_type* device = calloc(1, sizeof(device_properties_type));
                *device = rayneo_properties;
                device->hid_vendor_id = vendor_id;
                device->hid_product_id = product_id;

                return device;
            }
        }
    }

    return NULL;
};

bool rayneo_device_connect() {
    if (!connected) {
        RegisterIMUEventCallback(rayneo_imu_callback);
        RegisterStateEventCallback(rayneo_mcu_callback);
        if (EstablishUsbConnection(0x1bbb, 0xaf50) == 0) {
            NotifyDeviceConnected();
            StartXR();
            OpenIMU();
            connected = true;
        }
    }

    return connected;
};

void rayneo_block_on_device() {
    while (connected) {
        sleep(1);
    }
    CloseIMU();
    StopXR();
    NotifyDeviceDisconnected();
    ResetUsbConnection();
    UnregisterIMUEventCallback(rayneo_imu_callback);
    UnregisterStateEventCallback(rayneo_mcu_callback);
};

bool rayneo_device_is_sbs_mode() {
    // const struct XRDeviceInfo info = FetchDeviceInfo();
    // return info.sideBySide;

    return false;
};

bool rayneo_device_set_sbs_mode(bool enabled) {
    if (enabled) {
        SwitchTo3D();
    } else {
        SwitchTo2D();
    }

    return true;
};

bool rayneo_is_connected() {
    return connected;
};

void rayneo_disconnect() {
    connected = false;
};

const device_driver_type rayneo_driver = {
    .supported_device_func              = rayneo_supported_device,
    .device_connect_func                = rayneo_device_connect,
    .block_on_device_func               = rayneo_block_on_device,
    .device_is_sbs_mode_func            = rayneo_device_is_sbs_mode,
    .device_set_sbs_mode_func           = rayneo_device_set_sbs_mode,
    .is_connected_func                  = rayneo_is_connected,
    .disconnect_func                    = rayneo_disconnect
};
