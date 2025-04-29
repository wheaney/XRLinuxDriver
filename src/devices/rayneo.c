#include "devices.h"
#include "devices/rayneo.h"
#include "driver.h"
#include "imu.h"
#include "logging.h"
#include "outputs.h"
#include "runtime_context.h"
#include "sdks/rayneo.h"
#include "strings.h"

#include <math.h>
#include <pthread.h>
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

#define RAYNEO_ID_VENDOR 0x1bbb
#define RAYNEO_ID_PRODUCT 0xaf50

#define STATE_EVENT_DEVICE_INFO 0x4000

// RayNeo SDK is returning rotations relative to an east-up-south coordinate system,
// this converts to to north-west-up, and applies a 15-degree offset based on factory device calibration
static const imu_quat_type adjustment_quat = {
    .w = 0.561,
    .x = -0.430,
    .y = 0.430,
    .z = 0.561
};

const device_properties_type rayneo_properties = {
    .brand                              = "",
    .model                              = "",
    .hid_vendor_id                      = RAYNEO_ID_VENDOR,
    .hid_product_id                     = RAYNEO_ID_PRODUCT,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 43.0,
    .lens_distance_ratio                = 0.05,
    .calibration_wait_s                 = 5,
    .imu_cycles_per_s                   = FORCED_CYCLES_PER_S,
    .imu_buffer_size                    = ceil(BUFFER_SIZE_TARGET_MS / FORCED_CYCLE_TIME_MS),
    .look_ahead_constant                = 15.0,
    .look_ahead_frametime_multiplier    = 0.45,
    .look_ahead_scanline_adjust         = 8.0,
    .look_ahead_ms_cap                  = 40.0,
    .sbs_mode_supported                 = true,
    .firmware_update_recommended        = false
};

static uint32_t last_utilized_event_ts = 0;
static bool connected = false;
static bool initialized = false;
static bool is_sbs_mode = false;
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
        driver_handle_imu_event(ts, nwu_quat);

        last_utilized_event_ts = ts;
    }
}

pthread_mutex_t device_name_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t device_name_cond = PTHREAD_COND_INITIALIZER;
static char* device_brand = NULL;
static char* device_model = NULL;
static void rayneo_mcu_callback(uint32_t state, uint64_t timestamp, size_t length, const void* data) {
    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    if (!connected) return;
    if (state == STATE_EVENT_DEVICE_INFO) {
        pthread_mutex_lock(&device_name_mutex);
        if (device_brand == NULL && device_model == NULL) {
            char device_type[64];
            GetDeviceType(device_type);
            if (config()->debug_device) log_debug("RayNeo driver, received device type: %s\n", device_type);

            bool device_found = false;
            char* brand_part = strtok(device_type, " ");
            char* model_part = strtok(NULL, " ");
            if (brand_part && model_part) {
                char* version_part = strtok(NULL, " ");
                if (version_part) {
                    char full_model_name[strlen(model_part) + strlen(version_part) + 2]; 
                    snprintf(full_model_name, sizeof(full_model_name), "%s %s", model_part, version_part);
                    device_model = strdup(full_model_name);
                } else {
                    device_model = strdup(model_part);
                }
                device_brand = strdup(brand_part);
                device_found = true;
            }
            if (device_found) pthread_cond_signal(&device_name_cond);
        }
        pthread_mutex_unlock(&device_name_mutex);

        is_sbs_mode = GetSideBySideStatus() == 1;
    }
}

bool rayneo_device_connect() {
    if (!connected) {
        if (!initialized) {
            RegisterIMUEventCallback(rayneo_imu_callback);
            RegisterStateEventCallback(rayneo_mcu_callback);
            if (EstablishUsbConnection(RAYNEO_ID_VENDOR, RAYNEO_ID_PRODUCT) == 0) {
                NotifyDeviceConnected();
                initialized = true;
            }
        }
        if (initialized) {
            StartXR();
            OpenIMU();

            // this will trigger the STATE_EVENT_DEVICE_INFO event
            AcquireDeviceInfo();

            connected = true;
        } else {
            log_message("RayNeo driver, failed to establish a connection\n");
        }
    }

    return connected;
};

void rayneo_device_disconnect(bool forced) {
    if (connected) {
        CloseIMU();
        StopXR();
        if (!forced || !device_present()) {
            NotifyDeviceDisconnected();
            ResetUsbConnection();
            UnregisterIMUEventCallback(rayneo_imu_callback);
            UnregisterStateEventCallback(rayneo_mcu_callback);
            initialized = false;
        }
        connected = false;
        device_brand = NULL;
        free_and_clear(&device_model);
    }
};

device_properties_type* rayneo_supported_device(uint16_t vendor_id, uint16_t product_id, uint8_t usb_bus, uint8_t usb_address) {
    if (vendor_id == RAYNEO_ID_VENDOR && product_id == RAYNEO_ID_PRODUCT) {
        device_properties_type* device = calloc(1, sizeof(device_properties_type));
        *device = rayneo_properties;

        // trying to connect to the device too quickly seems to cause irrecoverable connection issues
        sleep(1);

        // device_connect is actually out-of-turn here, the driver would normally call connect after we return the device 
        // properties, but we kick this off now so we can acquire the device name, which unfortunately comes from the SDK 
        // only after establishing a connection.
        if (rayneo_device_connect()) {
            pthread_mutex_lock(&device_name_mutex);
            while (device_brand == NULL && device_model == NULL) {
                pthread_cond_wait(&device_name_cond, &device_name_mutex);
            }
            pthread_mutex_unlock(&device_name_mutex);
            device->brand = device_brand;
            device->model = device_model;

            // Leave the connection open if we think it'll be used, but if the driver is disabled, disconnect now
            if (driver_disabled()) rayneo_device_disconnect(true);

            return device;
        }
    }

    return NULL;
};

void rayneo_block_on_device() {
    device_properties_type* device = device_checkout();
    if (connected && device != NULL) connected = wait_for_imu_start();
    while (connected && device != NULL && is_imu_alive()) {
        sleep(1);
    }

    rayneo_device_disconnect(false);
    device_checkin(device);
};

bool rayneo_device_is_sbs_mode() {
    return is_sbs_mode;
};

bool rayneo_device_set_sbs_mode(bool enabled) {
    // don't explicitly change the is_sbs_mode value here, wait for it to come back around from the MCU deviceinfo response
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

void rayneo_disconnect(bool forced) {
    rayneo_device_disconnect(forced);
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
