#include "device.h"
#include "device3.h"
#include "device4.h"
#include "driver.h"
#include "imu.h"
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
#define BUFFER_SIZE_TARGET_MS 10

#define NUM_MAPPED_DISPLAY_MODES 5

// These two arrays are not only used as sets to determine what type a display mode is, but they're also used to
// map back and forth to one another (the index of a display mode in one array is used to find the corresponding
// display mode in the other array). That's way they're ordered the way they are, and some modes are duplicated.
const int sbs_display_modes[NUM_MAPPED_DISPLAY_MODES] = {
    DEVICE4_DISPLAY_MODE_3840x1080_60_SBS,
    DEVICE4_DISPLAY_MODE_3840x1080_72_SBS,
    DEVICE4_DISPLAY_MODE_3840x1080_90_SBS,
    DEVICE4_DISPLAY_MODE_3840x1080_90_SBS, // no 120Hz SBS mode, map from 120Hz to 90Hz
    DEVICE4_DISPLAY_MODE_1920x1080_60_SBS  // put this last so no non-SBS mode will map to it
};

const int non_sbs_display_modes[NUM_MAPPED_DISPLAY_MODES] = {
    DEVICE4_DISPLAY_MODE_1920x1080_60,
    DEVICE4_DISPLAY_MODE_1920x1080_72,
    DEVICE4_DISPLAY_MODE_1920x1080_90,
    DEVICE4_DISPLAY_MODE_1920x1080_120, // no SBS mode will be able to map to this
    DEVICE4_DISPLAY_MODE_1920x1080_60   // this duplicates index 0, so the sbs mode mapping here will get remapped
};

const uint16_t device_pid_air_1 = 0x0424;
const uint16_t device_pid_air_2 = 0x0428;
const uint16_t device_pid_air_2_pro = 0x0432;
const char* device_name_air_1 = "XREAL Air";
const char* device_name_air_2 = "XREAL Air 2";
const char* device_name_air_2_pro = "XREAL Air 2 Pro";

const imu_quat_type nwu_conversion_quat = {.x = 1, .y = 0, .z = 0, .w = 0};

const device_properties_type xreal_air_properties = {
    .name                               = NULL,
    .hid_vendor_id                      = 0,
    .hid_product_id                     = 0,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = 1080,
    .fov                                = 46.0,
    .lens_distance_ratio                = 0.035,
    .calibration_wait_s                 = 15,
    .imu_cycles_per_s                   = FORCED_CYCLES_PER_S,
    .imu_buffer_size                    = ceil(BUFFER_SIZE_TARGET_MS * FORCED_CYCLES_PER_S / EXPECTED_CYCLES_PER_S),
    .look_ahead_constant                = 10.0,
    .look_ahead_frametime_multiplier    = 0.3,
    .sbs_mode_supported                 = true
};

int frequency_downsample_tracker = 0;
uint32_t last_utilized_event_ts = 0;
void handle_xreal_event(uint64_t timestamp,
		   device3_event_type event,
		   const device3_ahrs_type* ahrs) {
    if (driver_disabled()) return;

    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    uint32_t elapsed_from_last_utilized = ts - last_utilized_event_ts;
    if (event == DEVICE3_EVENT_UPDATE && elapsed_from_last_utilized > FORCED_CYCLE_TIME_MS) {
        device3_quat_type quat = device3_get_orientation(ahrs);
        imu_quat_type imu_quat = { .w = quat.w, .x = quat.x, .y = quat.y, .z = quat.z };
        imu_quat_type nwu_quat = multiply_quaternions(imu_quat, nwu_conversion_quat);
        imu_euler_type nwu_euler = quaternion_to_euler(nwu_quat);
        driver_handle_imu_event(ts, nwu_quat, nwu_euler);

        last_utilized_event_ts = ts;
    }
}

void handle_xreal_controller_event(
		uint64_t timestamp,
		device4_event_type event,
		uint8_t brightness,
		const char* msg
) {
    // do nothing
}

device3_type* glasses_imu;
device4_type* glasses_controller;
device_properties_type* xreal_device_connect() {
    glasses_imu = malloc(sizeof(device3_type));
    bool success = device3_open(glasses_imu, handle_xreal_event) == DEVICE3_ERROR_NO_ERROR;
    if (success) {
        device3_clear(glasses_imu);
        device3_calibrate(glasses_imu, 1000, true, true, false);

        glasses_controller = malloc(sizeof(device4_type));
        success = device4_open(glasses_controller, handle_xreal_controller_event) == DEVICE4_ERROR_NO_ERROR;
        device4_clear(glasses_controller);
    }

    if (success) {
        device_properties_type* device = malloc(sizeof(device_properties_type));
        *device = xreal_air_properties;

        device->hid_product_id = glasses_imu->product_id;
        device->hid_vendor_id = glasses_imu->vendor_id;

        if (device->hid_product_id == device_pid_air_1) {
            device->name = strdup(device_name_air_1);
        } else if (device->hid_product_id == device_pid_air_2) {
            device->name = strdup(device_name_air_2);
        } else if (device->hid_product_id == device_pid_air_2_pro) {
            device->name = strdup(device_name_air_2_pro);
        }

        return device;
    }

    return NULL;
};

void *poll_imu_func(void *arg) {
    while (glasses_controller && !driver_device_should_disconnect() && device3_read(glasses_imu, 1) == DEVICE3_ERROR_NO_ERROR);

    device3_close(glasses_imu);
    if (glasses_imu) free(glasses_imu);
    glasses_imu = NULL;
};

bool sbs_mode_change_requested = false;
void *poll_controller_func(void *arg) {
    while (glasses_imu && !driver_device_should_disconnect() && device4_read(glasses_controller, 100) == DEVICE4_ERROR_NO_ERROR) {
        if (sbs_mode_change_requested) {
            device4_error_type error = device4_update_display_mode(glasses_controller);
            if (error == DEVICE4_ERROR_NO_ERROR) {
                sbs_mode_change_requested = false;
            }
        } else {
            device4_poll_display_mode(glasses_controller);
        }

        sleep(1);
    }

    device4_close(glasses_controller);
    if (glasses_controller) free(glasses_controller);
    glasses_controller = NULL;
};

void xreal_block_on_device() {
    pthread_t imu_thread;
    pthread_create(&imu_thread, NULL, poll_imu_func, NULL);

    pthread_t controller_thread;
    pthread_create(&controller_thread, NULL, poll_controller_func, NULL);

    while (!driver_device_should_disconnect() && glasses_controller && glasses_imu);

    pthread_join(imu_thread, NULL);
    pthread_join(controller_thread, NULL);
};

int get_display_mode_index(int display_mode, const int* display_modes) {
    for (int i = 0; i < NUM_MAPPED_DISPLAY_MODES; i++) {
        if (display_mode == display_modes[i]) {
            return i;
        }
    }

    return -1;
}

bool xreal_device_is_sbs_mode() {
    if (glasses_controller) {
        if (get_display_mode_index(glasses_controller->disp_mode, sbs_display_modes) != -1) {
            return true;
        }
    }

    return false;
};

bool xreal_device_set_sbs_mode(bool enable) {
    if (!glasses_controller) return false;

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

const device_driver_type xreal_driver = {
    .device_connect_func                = xreal_device_connect,
    .block_on_device_func               = xreal_block_on_device,
    .device_is_sbs_mode_func            = xreal_device_is_sbs_mode,
    .device_set_sbs_mode_func           = xreal_device_set_sbs_mode
};