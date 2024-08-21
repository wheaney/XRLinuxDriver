#include "devices.h"
#include "device_imu.h"
#include "device_mcu.h"
#include "driver.h"
#include "imu.h"
#include "outputs.h"
#include "runtime_context.h"
#include "strings.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TS_TO_MS_FACTOR 1000000
#define EXPECTED_CYCLES_PER_S 1000
#define FORCED_CYCLES_PER_S 250 // glasses may operate at a reduced frequency, let's force a reduced cycle time
#define CYCLE_TIME_CHECK_ERROR_FACTOR 0.95 // cycle times won't be exact, check within a 5% margin
#define FORCED_CYCLE_TIME_MS 1000.0 / FORCED_CYCLES_PER_S * CYCLE_TIME_CHECK_ERROR_FACTOR
#define BUFFER_SIZE_TARGET_MS 10 // smooth IMU data over this period of time

#define MAPPED_DISPLAY_MODE_COUNT 5

// These two arrays are not only used as sets to determine what type a display mode is, but they're also used to
// map back and forth to one another (the index of a display mode in one array is used to find the corresponding
// display mode in the other array). That's way they're ordered the way they are, and some modes are duplicated.
const int sbs_display_modes[MAPPED_DISPLAY_MODE_COUNT] = {
    DEVICE_MCU_DISPLAY_MODE_3840x1080_60_SBS,
    DEVICE_MCU_DISPLAY_MODE_3840x1080_72_SBS,
    DEVICE_MCU_DISPLAY_MODE_3840x1080_90_SBS,
    DEVICE_MCU_DISPLAY_MODE_3840x1080_90_SBS, // no 120Hz SBS mode, map from 120Hz to 90Hz
    DEVICE_MCU_DISPLAY_MODE_1920x1080_60_SBS  // put this last so no non-SBS mode will map to it
};

const int non_sbs_display_modes[MAPPED_DISPLAY_MODE_COUNT] = {
    DEVICE_MCU_DISPLAY_MODE_1920x1080_60,
    DEVICE_MCU_DISPLAY_MODE_1920x1080_72,
    DEVICE_MCU_DISPLAY_MODE_1920x1080_90,
    DEVICE_MCU_DISPLAY_MODE_1920x1080_120, // no SBS mode will be able to map to this
    DEVICE_MCU_DISPLAY_MODE_1920x1080_60   // this duplicates index 0, so the sbs mode mapping here will get remapped
};

#define XREAL_ID_PRODUCT_COUNT 4
#define XREAL_ID_VENDOR 0x3318
#define XREAL_AIR_2_ULTRA_PID 0x0426
#define XREAL_AIR_2_ULTRA_FOV 52.0
const uint16_t xreal_supported_id_product[XREAL_ID_PRODUCT_COUNT] = {0x0424, 0x0428, 0x0432, XREAL_AIR_2_ULTRA_PID};
const char* xreal_supported_models[XREAL_ID_PRODUCT_COUNT] = {"Air", "Air 2", "Air 2 Pro", "Air 2 Ultra"};

const imu_quat_type nwu_conversion_quat = {.x = 1, .y = 0, .z = 0, .w = 0};

const device_properties_type xreal_air_properties = {
    .brand                              = "XREAL",
    .model                              = NULL,
    .hid_vendor_id                      = 0,
    .hid_product_id                     = 0,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 46.0,
    .lens_distance_ratio                = 0.025,
    .calibration_wait_s                 = 15,
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
static bool mcu_enabled = false;
void handle_xreal_event(uint64_t timestamp,
                        device_imu_event_type event,
                        const device_imu_ahrs_type* ahrs) {
    if (!connected || driver_disabled()) return;

    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    uint32_t elapsed_from_last_utilized = ts - last_utilized_event_ts;
    if (event == DEVICE_IMU_EVENT_UPDATE && elapsed_from_last_utilized > FORCED_CYCLE_TIME_MS) {
        device_imu_quat_type quat = device_imu_get_orientation(ahrs);
        imu_quat_type imu_quat = { .w = quat.w, .x = quat.x, .y = quat.y, .z = quat.z };
        imu_quat_type nwu_quat = multiply_quaternions(imu_quat, nwu_conversion_quat);
        driver_handle_imu_event(ts, nwu_quat);

        last_utilized_event_ts = ts;
    }
}

void handle_xreal_controller_event(
		uint64_t timestamp,
		device_mcu_event_type event,
		uint8_t brightness,
		const char* msg
) {
    // do nothing
}

device_imu_type* glasses_imu;
device_mcu_type* glasses_controller;
bool xreal_device_connect() {
    sleep(1);
    
    glasses_imu = calloc(1, sizeof(device_imu_type));
    connected = device_imu_open(glasses_imu, handle_xreal_event) == DEVICE_IMU_ERROR_NO_ERROR;
    if (connected) {
        device_imu_clear(glasses_imu);
        device_imu_calibrate(glasses_imu, 1000, true, true, false);

        glasses_controller = calloc(1, sizeof(device_mcu_type));
        mcu_enabled = device_mcu_open(glasses_controller, handle_xreal_controller_event) == DEVICE_MCU_ERROR_NO_ERROR;
        device_mcu_clear(glasses_controller);
    }

    if (!connected && glasses_imu) {
        device_imu_close(glasses_imu);
        free(glasses_imu);
        glasses_imu = NULL;
    }

    if (!mcu_enabled) {
        if (glasses_controller) {
            device_mcu_close(glasses_controller);
            free(glasses_controller);
            glasses_controller = NULL;
        }

        device_properties_type* device = device_checkout();
        device->sbs_mode_supported = false;
        device_checkin(device);
    }

    return connected;
};

device_properties_type* xreal_supported_device(uint16_t vendor_id, uint16_t product_id, uint8_t usb_bus, uint8_t usb_address) {
    if (vendor_id == XREAL_ID_VENDOR) {
        for (int i=0; i < XREAL_ID_PRODUCT_COUNT; i++) {
            if (product_id == xreal_supported_id_product[i]) {
                device_properties_type* device = calloc(1, sizeof(device_properties_type));
                *device = xreal_air_properties;
                device->hid_vendor_id = vendor_id;
                device->hid_product_id = product_id;
                device->model = (char *)xreal_supported_models[i];
                if (product_id == XREAL_AIR_2_ULTRA_PID) {
                    device->fov = XREAL_AIR_2_ULTRA_FOV;
                }

                return device;
            }
        }
    }

    return NULL;
};

void *poll_imu_func(void *arg) {
    while (connected && (!mcu_enabled || glasses_controller) && device_imu_read(glasses_imu, 1) == DEVICE_IMU_ERROR_NO_ERROR);

    device_imu_close(glasses_imu);
    if (glasses_imu) free(glasses_imu);
    glasses_imu = NULL;
};

bool sbs_mode_change_requested = false;
void *poll_controller_func(void *arg) {
    while (connected && glasses_imu && mcu_enabled && device_mcu_read(glasses_controller, 100) == DEVICE_MCU_ERROR_NO_ERROR) {
        if (sbs_mode_change_requested) {
            device_mcu_error_type error = device_mcu_update_display_mode(glasses_controller);
            if (error == DEVICE_MCU_ERROR_NO_ERROR) {
                sbs_mode_change_requested = false;
            }
        } else {
            device_mcu_poll_display_mode(glasses_controller);
        }

        sleep(1);
    }

    device_mcu_close(glasses_controller);
    if (glasses_controller) free(glasses_controller);
    glasses_controller = NULL;
};

void xreal_block_on_device() {
    // we'll hold onto our device refcount until we're done blocking and cleaning up
    device_properties_type* device = device_checkout();
    if (device != NULL) {
        pthread_t imu_thread;
        pthread_create(&imu_thread, NULL, poll_imu_func, NULL);

        pthread_t controller_thread;
        pthread_create(&controller_thread, NULL, poll_controller_func, NULL);

        connected &= wait_for_imu_start();
        while (connected) {
            sleep(1);
            connected &= glasses_imu && (!mcu_enabled || glasses_controller) && is_imu_alive();
        }

        pthread_join(imu_thread, NULL);
        pthread_join(controller_thread, NULL);
    }
    device_checkin(device);
};

int get_display_mode_index(int display_mode, const int* display_modes) {
    for (int i = 0; i < MAPPED_DISPLAY_MODE_COUNT; i++) {
        if (display_mode == display_modes[i]) {
            return i;
        }
    }

    return -1;
}

bool xreal_device_is_sbs_mode() {
    if (connected && mcu_enabled && glasses_controller) {
        if (get_display_mode_index(glasses_controller->disp_mode, sbs_display_modes) != -1) {
            return true;
        }
    }

    return false;
};

bool xreal_device_set_sbs_mode(bool enable) {
    if (!connected || !mcu_enabled || !glasses_controller) return false;

    // check what the current mode is
    int sbs_mode_index = get_display_mode_index(glasses_controller->disp_mode, sbs_display_modes);
    bool is_sbs_mode = sbs_mode_index != -1;

    // if the current mode matches the requested mode, do nothing, return success
    if (enable == is_sbs_mode) return true;

    if (enable) {
        // requesting SBS mode, currently non-SBS, find the corresponding SBS mode and set it
        int non_sbs_mode_index = get_display_mode_index(glasses_controller->disp_mode, non_sbs_display_modes);
        if (non_sbs_mode_index == -1) return false;

        glasses_controller->disp_mode = sbs_display_modes[non_sbs_mode_index];
    } else {
        // requesting non-SBS mode, currently SBS, find the corresponding non-SBS mode and set it
        glasses_controller->disp_mode = non_sbs_display_modes[sbs_mode_index];
    }

    sbs_mode_change_requested = true;

    return true;
};

bool xreal_is_connected() {
    return connected;
};

void xreal_disconnect(bool soft) {
    connected = false;
};

const device_driver_type xreal_driver = {
    .supported_device_func              = xreal_supported_device,
    .device_connect_func                = xreal_device_connect,
    .block_on_device_func               = xreal_block_on_device,
    .device_is_sbs_mode_func            = xreal_device_is_sbs_mode,
    .device_set_sbs_mode_func           = xreal_device_set_sbs_mode,
    .is_connected_func                  = xreal_is_connected,
    .disconnect_func                    = xreal_disconnect
};