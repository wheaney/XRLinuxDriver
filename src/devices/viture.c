#include "devices.h"
#include "devices/viture.h"
#include "driver.h"
#include "epoch.h"
#include "imu.h"
#include "logging.h"
#include "memory.h"
#include "outputs.h"
#include "runtime_context.h"
#include "sdks/viture_device.h"
#include "sdks/viture_device_carina.h"
#include "sdks/viture_glasses_provider.h"
#include "strings.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VITURE_ID_PRODUCT_COUNT 14
#define VITURE_ID_VENDOR 0x35ca
#define VITURE_ONE_MODEL_NAME "One"
#define VITURE_ONE_LITE_MODEL_NAME "One Lite"
#define VITURE_PRO_MODEL_NAME "Pro"
#define VITURE_LUMA_MODEL_NAME "Luma"
#define VITURE_LUMA_PRO_MODEL_NAME "Luma Pro"
#define VITURE_LUMA_ULTRA_MODEL_NAME "Luma Ultra"
#define VITURE_LUMA_CYBER_MODEL_NAME "Luma Cyber"
#define VITURE_BEAST_MODEL_NAME "Beast"

#define VITURE_DISPLAY_MODE_1920_1080_60HZ 0x31
#define VITURE_DISPLAY_MODE_3840_1080_60HZ 0x32
#define VITURE_DISPLAY_MODE_3840_1080_90HZ 0x35
#define VITURE_DISPLAY_MODE_1920_1200_60HZ 0x41
#define VITURE_DISPLAY_MODE_3840_1200_60HZ 0x42
#define VITURE_DISPLAY_MODE_3840_1200_90HZ 0x45

#define VITURE_IMU_MODE_RAW 0
#define VITURE_IMU_MODE_POSE 1

#define VITURE_IMU_FREQ_LOW 0
#define VITURE_IMU_FREQ_MEDIUM_LOW 1
#define VITURE_IMU_FREQ_MEDIUM 2
#define VITURE_IMU_FREQ_MEDIUM_HIGH 3
#define VITURE_IMU_FREQ_HIGH 4
#define VITURE_IMU_FREQ_COUNT 5
#define VITURE_CARINA_CYCLES_PER_S 1000
#define VITURE_CARINA_POLL_INTERVAL_US (1000000 / VITURE_CARINA_CYCLES_PER_S)

#define VITURE_LOG_LEVEL_NONE 0
#define VITURE_LOG_LEVEL_ERROR 1
#define VITURE_LOG_LEVEL_INFO 2
#define VITURE_LOG_LEVEL_DEBUG 3

#define VITURE_STATE_ID_BRIGHTNESS 0
#define VITURE_STATE_ID_VOLUME 1
#define VITURE_STATE_ID_DISPLAY_MODE 2
#define VITURE_STATE_ID_ELECTROCHROMIC_FILM 3

const float VITURE_ONE_PITCH_ADJUSTMENT = 6.0;
const float VITURE_PRO_PITCH_ADJUSTMENT = 3.0;
const float VITURE_LUMA_PITCH_ADJUSTMENT = 3.0;
const float VITURE_LUMA_ULTRA_PITCH_ADJUSTMENT = -8.5;
const float VITURE_BEAST_PITCH_ADJUSTMENT = -8.5; // Placeholder until specs finalized

const float VITURE_ONE_FOV = 40.0;
const float VITURE_PRO_FOV = 43.0;
const float VITURE_LUMA_FOV = 50.0;
const float VITURE_LUMA_PRO_FOV = 52.0;
const float VITURE_LUMA_ULTRA_FOV = 52.0;
const float VITURE_LUMA_CYBER_FOV = 52.0;
const float VITURE_BEAST_FOV = 58.0;

const int viture_supported_id_product[VITURE_ID_PRODUCT_COUNT] = {
    0x1011, // One
    0x1013, // One
    0x1017, // One
    0x1015, // One Lite
    0x101b, // One Lite
    0x1019, // Pro
    0x101d, // Pro
    0x1131, // Luma
    0x1121, // Luma Pro
    0x1141, // Luma Pro
    0x1101, // Luma Ultra
    0x1104, // Luma Ultra
    0x1151, // Luma Cyber
    0x1201  // Viture Beast
};
const char* viture_supported_models[VITURE_ID_PRODUCT_COUNT] = {
    VITURE_ONE_MODEL_NAME, 
    VITURE_ONE_MODEL_NAME,
    VITURE_ONE_MODEL_NAME,
    VITURE_ONE_LITE_MODEL_NAME,
    VITURE_ONE_LITE_MODEL_NAME,
    VITURE_PRO_MODEL_NAME,
    VITURE_PRO_MODEL_NAME,
    VITURE_LUMA_MODEL_NAME,
    VITURE_LUMA_PRO_MODEL_NAME,
    VITURE_LUMA_PRO_MODEL_NAME,
    VITURE_LUMA_ULTRA_MODEL_NAME,
    VITURE_LUMA_ULTRA_MODEL_NAME,
    VITURE_LUMA_CYBER_MODEL_NAME,
    VITURE_BEAST_MODEL_NAME
};
const float* viture_pitch_adjustments[VITURE_ID_PRODUCT_COUNT] = {
    &VITURE_ONE_PITCH_ADJUSTMENT,  // One
    &VITURE_ONE_PITCH_ADJUSTMENT,  // One
    &VITURE_ONE_PITCH_ADJUSTMENT,  // One
    &VITURE_ONE_PITCH_ADJUSTMENT,  // One Lite
    &VITURE_ONE_PITCH_ADJUSTMENT,  // One Lite
    &VITURE_PRO_PITCH_ADJUSTMENT,  // Pro
    &VITURE_PRO_PITCH_ADJUSTMENT,  // Pro
    &VITURE_LUMA_PITCH_ADJUSTMENT, // Luma
    &VITURE_LUMA_PITCH_ADJUSTMENT, // Luma Pro
    &VITURE_LUMA_PITCH_ADJUSTMENT, // Luma Pro
    &VITURE_LUMA_ULTRA_PITCH_ADJUSTMENT, // Luma Ultra
    &VITURE_LUMA_ULTRA_PITCH_ADJUSTMENT, // Luma Ultra
    &VITURE_LUMA_PITCH_ADJUSTMENT, // Luma Cyber
    &VITURE_BEAST_PITCH_ADJUSTMENT // Beast
};
const float* viture_fovs[VITURE_ID_PRODUCT_COUNT] = {
    &VITURE_ONE_FOV,        // One
    &VITURE_ONE_FOV,        // One
    &VITURE_ONE_FOV,        // One
    &VITURE_ONE_FOV,        // One Lite
    &VITURE_ONE_FOV,        // One Lite
    &VITURE_PRO_FOV,        // Pro
    &VITURE_PRO_FOV,        // Pro
    &VITURE_LUMA_FOV,       // Luma
    &VITURE_LUMA_PRO_FOV,   // Luma Pro
    &VITURE_LUMA_PRO_FOV,   // Luma Pro
    &VITURE_LUMA_ULTRA_FOV, // Luma Ultra
    &VITURE_LUMA_ULTRA_FOV, // Luma Ultra
    &VITURE_LUMA_CYBER_FOV, // Luma Cyber
    &VITURE_BEAST_FOV       // Beast
};
const int viture_resolution_heights[VITURE_ID_PRODUCT_COUNT] = {
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // One Lite
    RESOLUTION_1080P_H, // One Lite
    RESOLUTION_1080P_H, // Pro
    RESOLUTION_1080P_H, // Pro
    RESOLUTION_1200P_H, // Luma
    RESOLUTION_1200P_H, // Luma Pro
    RESOLUTION_1200P_H, // Luma Pro
    RESOLUTION_1200P_H, // Luma Ultra
    RESOLUTION_1200P_H, // Luma Ultra
    RESOLUTION_1200P_H, // Luma Cyber
    RESOLUTION_1200P_H  // Beast
};

const int viture_calibration_wait_s[VITURE_ID_PRODUCT_COUNT] = {
    1, // One
    1, // One
    1, // One
    1, // One Lite
    1, // One Lite
    1, // Pro
    1, // Pro
    1, // Luma
    1, // Luma Pro
    1, // Luma Pro
    1, // Luma Ultra
    1, // Luma Ultra
    1, // Luma Cyber
    1  // Beast
};

const int viture_look_ahead_constant[VITURE_ID_PRODUCT_COUNT] = {
    20, // One
    20, // One
    20, // One
    20, // One Lite
    20, // One Lite
    20, // Pro
    20, // Pro
    20, // Luma
    20, // Luma Pro
    20, // Luma Pro
    10, // Luma Ultra
    10, // Luma Ultra
    10, // Luma Cyber
    10  // Beast
};

static imu_quat_type adjustment_quat;
static XRDeviceProviderHandle viture_provider = NULL;
static XRDeviceType viture_device_type = XR_DEVICE_TYPE_VITURE_GEN1;
static uint16_t viture_last_product_id = 0;
static pthread_mutex_t viture_connection_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool connected = false;
static bool initialized = false;
static bool viture_callbacks_registered = false;
static bool viture_state_callback_registered = false;
static bool viture_imu_open = false;
static uint8_t viture_requested_frequency = VITURE_IMU_FREQ_HIGH;
static bool sbs_mode_enabled = false;
static int viture_saved_display_mode = -1;
static int viture_saved_dof = -1;
static int viture_callback_logs_remaining = 10;

static const int viture_frequency_hz[VITURE_IMU_FREQ_COUNT] = {60, 90, 120, 240, 500};

static const char* viture_open_imu_error_reason(int code) {
    switch (code) {
    case -1: return "param error";
    case -2: return "USB execution error";
    case -3: return "device type not supported";
    case -4: return "other error";
    default: return "unknown";
    }
}

const device_properties_type viture_one_properties = {
    .brand                              = "VITURE",
    .model                              = NULL,
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
    .firmware_update_recommended        = false,
    .provides_orientation               = true,
    .provides_position                  = false
};

static bool viture_display_mode_is_sbs(int mode) {
    switch (mode) {
        case VITURE_DISPLAY_MODE_3840_1080_60HZ:
        case VITURE_DISPLAY_MODE_3840_1080_90HZ:
        case VITURE_DISPLAY_MODE_3840_1200_60HZ:
        case VITURE_DISPLAY_MODE_3840_1200_90HZ:
            return true;
        default:
            return false;
    }
}

static void viture_refresh_sbs_state_locked() {
    if (viture_provider == NULL) return;
    int mode = xr_device_provider_get_display_mode(viture_provider);
    if (mode >= 0) {
        sbs_mode_enabled = viture_display_mode_is_sbs(mode);
        if (config()->debug_device) {
            log_debug("VITURE: Refreshed SBS state, mode=%d enabled=%d\n", mode, sbs_mode_enabled);
        }
    }
}

// TODO - for eventual Beast integration
static void viture_capture_and_override_display_mode_locked() {
    viture_saved_display_mode = -1;
    viture_saved_dof = -1;

    if (viture_provider == NULL) {
        if (config()->debug_device) {
            log_debug("VITURE: Cannot override display mode, provider NULL\n");
        }
        return;
    }

    int mode = 0;
    int dof = 0;
    if (xr_device_provider_get_display_mode_and_native_dof(viture_provider, &mode, &dof) != 0) {
        if (config()->debug_device) {
            log_debug("VITURE: Unable to read display mode/dof during override\n");
        }
        return;
    }

    viture_saved_display_mode = mode;
    viture_saved_dof = dof;

    if (xr_device_provider_set_display_mode_and_native_dof(viture_provider, mode, 0) == 0) {
        sbs_mode_enabled = viture_display_mode_is_sbs(mode);
        if (config()->debug_device) {
            log_debug("VITURE: Forced display mode=%d to 0DoF\n", mode);
        }
    } else if (config()->debug_device) {
        log_debug("VITURE: Failed to force 0DoF (mode=%d, dof=%d)\n", mode, dof);
    }
}

// TODO - for eventual Beast integration
static void viture_restore_display_mode_locked() {
    if (viture_provider == NULL) {
        viture_saved_display_mode = -1;
        viture_saved_dof = -1;
        if (config()->debug_device) {
            log_debug("VITURE: Cannot restore display mode, provider NULL\n");
        }
        return;
    }

    if (viture_saved_display_mode < 0 || viture_saved_dof < 0) {
        if (config()->debug_device) {
            log_debug("VITURE: No saved display state to restore\n");
        }
        return;
    }

    int restore_mode = viture_saved_display_mode;
    int restore_dof = viture_saved_dof;
    int result = xr_device_provider_set_display_mode_and_native_dof(
        viture_provider, restore_mode, restore_dof);

    if (result == 0) {
        sbs_mode_enabled = viture_display_mode_is_sbs(restore_mode);
        if (config()->debug_device) {
            log_debug("VITURE: Restored display mode=%d dof=%d\n", restore_mode, restore_dof);
        }
    } else if (config()->debug_device) {
        log_debug("VITURE: Failed to restore display mode=%d dof=%d (err=%d)\n",
                  restore_mode,
                  restore_dof,
                  result);
    }

    viture_saved_display_mode = -1;
    viture_saved_dof = -1;
}

static void viture_publish_pose(imu_quat_type orientation, bool has_position,
                                imu_vec3_type position, uint32_t timestamp_ms) {
    if (driver_disabled()) return;

    orientation = multiply_quaternions(orientation, adjustment_quat);

    imu_pose_type pose = {0};
    pose.orientation = orientation;
    pose.position = has_position ? position : (imu_vec3_type){0};
    pose.has_orientation = true;
    pose.has_position = has_position;
    pose.timestamp_ms = timestamp_ms;
    driver_handle_pose_event(pose);
}

static void viture_legacy_imu_callback(float* imu, float* euler, uint64_t ts, uint64_t vsync) {
    (void)imu;
    (void)vsync;
    if (!connected || driver_disabled()) return;

    imu_euler_type euler_angles = {.roll = euler[0], .pitch = euler[1], .yaw = euler[2]};
    imu_quat_type quat = euler_to_quaternion_zxy(euler_angles);

    uint32_t timestamp_ms = (uint32_t)(ts / 1000000ULL);
    viture_publish_pose(quat, false, (imu_vec3_type){0}, timestamp_ms);
}

static void viture_carina_imu_callback(float* imu, double timestamp) {
    (void)imu;
    device_properties_type* device = device_checkout();
    if (connected && viture_provider != NULL && device != NULL) {
        float pose[9] = {0};
        int result = get_gl_pose_carina(viture_provider, pose, 0.0);
        if (result == 0) {
            // convert to NWU from EUS
            imu_quat_type quat = {.x = -pose[6], .y = -pose[4], .z = pose[5], .w = pose[3]};

            float full_distance_cm = LENS_TO_PIVOT_CM / device->lens_distance_ratio;
            float meters_to_full_distance_ratio = 100.0f / full_distance_cm;
            imu_vec3_type position = {
                .x = -pose[2] * meters_to_full_distance_ratio, 
                .y = -pose[0] * meters_to_full_distance_ratio,
                .z = pose[1] * meters_to_full_distance_ratio
            };

            uint32_t timestamp_ms = (uint32_t)(timestamp * 1000.0);
            viture_publish_pose(quat, true, position, timestamp_ms);
        } else if (config()->debug_device) {
            log_debug("VITURE: get_gl_pose_carina failed (%d)\n", result);
        }
    }
    device_checkin(device);
}

// static const char* viture_get_model_name(uint16_t product_id) {
//     char* model_name = calloc(VITURE_MARKET_NAME_MAX, sizeof(char));
//     int requested_len = VITURE_MARKET_NAME_MAX;
//     int result = xr_device_provider_get_market_name(product_id, model_name, &requested_len);
//     if (result != 0) {
//         snprintf(model_name, VITURE_MARKET_NAME_MAX, "VITURE 0x%04X", product_id);
//     }

//     return model_name;
// }

// TODO - the SDK doesn't actually reliably call this yet
static void viture_state_callback(int glass_state_id, int glass_value) {
    if (glass_state_id == VITURE_STATE_ID_DISPLAY_MODE) {
        sbs_mode_enabled = viture_display_mode_is_sbs(glass_value);
        if (config()->debug_device) {
            log_debug("VITURE: Display mode changed via callback, mode=%d sbs_enabled=%d\n",
                      glass_value,
                      sbs_mode_enabled);
        }
    } else if (config()->debug_device) {
        log_debug("VITURE: Glass state callback id=%d value=%d\n", glass_state_id, glass_value);
    }
}

static void viture_register_state_callback_locked() {
    if (viture_provider == NULL || viture_state_callback_registered) return;

    int result = xr_device_provider_register_state_callback(viture_provider, viture_state_callback);
    if (result == 0) {
        viture_state_callback_registered = true;
        if (config()->debug_device) {
            log_debug("VITURE: State callback registered\n");
        }
    } else {
        log_error("VITURE: Failed to register state callback (%d)\n", result);
    }
}

static void viture_unregister_state_callback_locked() {
    if (viture_provider == NULL || !viture_state_callback_registered) return;

    int result = xr_device_provider_register_state_callback(viture_provider, NULL);
    if (result != 0 && config()->debug_device) {
        log_debug("VITURE: Failed to unregister state callback (%d)\n", result);
    }
    viture_state_callback_registered = false;
}

device_properties_type* viture_supported_device(uint16_t vendor_id, uint16_t product_id,
                                                uint8_t usb_bus, uint8_t usb_address) {
    if (vendor_id == VITURE_ID_VENDOR) {
        for (int i = 0; i < VITURE_ID_PRODUCT_COUNT; i++) {
            if (product_id == viture_supported_id_product[i]) {
                if (!xr_device_provider_is_product_id_valid(product_id)) {
                    log_message("VITURE: Product ID 0x%04x rejected by SDK\n", product_id);
                    continue;
                }

                device_properties_type* device = calloc(1, sizeof(device_properties_type));
                *device = viture_one_properties;
                device->hid_vendor_id = vendor_id;
                device->hid_product_id = product_id;
                device->model = (char *)viture_supported_models[i];
                device->resolution_h = viture_resolution_heights[i];
                device->fov = *viture_fovs[i];
                device->calibration_wait_s = viture_calibration_wait_s[i];
                device->look_ahead_constant = (float)viture_look_ahead_constant[i];

                adjustment_quat = device_pitch_adjustment(*viture_pitch_adjustments[i]);

                viture_last_product_id = product_id;

                return device;
            }
        }
    }

    return NULL;
};

static bool viture_initialize_provider_locked(uint16_t product_id) {
    if (product_id == 0) return false;

    xr_device_provider_set_log_level(VITURE_LOG_LEVEL_ERROR);

    viture_provider = xr_device_provider_create(product_id);
    if (viture_provider == NULL) {
        log_error("VITURE: Failed to create provider handle for product 0x%04x\n", product_id);
        return false;
    }

    if (config()->debug_device) {
        log_debug("VITURE: Provider handle created for product 0x%04x\n", product_id);
    }

    int sdk_device_type = xr_device_provider_get_device_type(viture_provider);
    viture_device_type =
        sdk_device_type >= 0 ? (XRDeviceType)sdk_device_type : XR_DEVICE_TYPE_VITURE_GEN1;

    if (config()->debug_device) {
        log_debug("VITURE: SDK device type reported as %d\n", sdk_device_type);
    }

    int register_result = -1;
    if (viture_device_type == XR_DEVICE_TYPE_VITURE_CARINA) {
        // TODO try removing this entirely since we don't need callbacks
        register_result =
            register_callbacks_carina(viture_provider, NULL, NULL, viture_carina_imu_callback, NULL);
    } else {
        register_result = register_callback(viture_provider, viture_legacy_imu_callback);
    }

    viture_callbacks_registered = (register_result == 0);
    if (!viture_callbacks_registered) {
        log_error("VITURE: Failed to register SDK callbacks (type=%d)\n", viture_device_type);
        xr_device_provider_destroy(viture_provider);
        viture_provider = NULL;
        return false;
    } else if (config()->debug_device) {
        log_debug("VITURE: Callback registration succeeded for type=%d\n", viture_device_type);
    }

    if (xr_device_provider_initialize(viture_provider, NULL) != 0) {
        log_error("VITURE: Failed to initialize SDK provider\n");
        xr_device_provider_destroy(viture_provider);
        viture_provider = NULL;
        return false;
    }

    if (config()->debug_device) {
        log_debug("VITURE: SDK provider initialized\n");
    }

    initialized = true;
    return true;
}

static bool viture_open_imu_locked() {
    if (viture_imu_open || viture_device_type == XR_DEVICE_TYPE_VITURE_CARINA) {
        return true;
    }

    const uint8_t fallback_order[] = {
        VITURE_IMU_FREQ_HIGH,
        VITURE_IMU_FREQ_MEDIUM_HIGH,
        VITURE_IMU_FREQ_MEDIUM,
        VITURE_IMU_FREQ_LOW
    };

    bool attempted[VITURE_IMU_FREQ_COUNT] = {0};
    size_t fallback_count = 4;

    for (size_t i = 0; i < fallback_count; i++) {
        uint8_t freq = fallback_order[i];
        if (freq >= VITURE_IMU_FREQ_COUNT) continue;
        if (attempted[freq]) continue;

        attempted[freq] = true;
        if (config()->debug_device) {
            log_debug("VITURE: Attempting IMU open at freq index %u\n", freq);
        }
        int open_result = open_imu(viture_provider, VITURE_IMU_MODE_POSE, freq);
        viture_imu_open = open_result == 0;
        if (viture_imu_open) {
            viture_requested_frequency = freq;
            return true;
        } else {
            log_error("VITURE: open_imu(%u) failed (%d: %s)\n", freq, open_result,
                      viture_open_imu_error_reason(open_result));
        }
    }

    if (config()->debug_device) {
        log_debug("VITURE: All IMU frequency attempts failed\n");
    }
    return false;
}

static bool viture_start_stream_locked() {
    if (!initialized || viture_provider == NULL) return false;

    if (xr_device_provider_start(viture_provider) != 0) {
        log_error("VITURE: Failed to start SDK provider\n");
        return false;
    }

    if (config()->debug_device) {
        log_debug("VITURE: Provider start succeeded\n");
    }

    sleep(1);

    // viture_register_state_callback_locked();

    if (!viture_open_imu_locked()) {
        log_error("VITURE: Failed to open IMU stream\n");
        // viture_unregister_state_callback_locked();
        xr_device_provider_stop(viture_provider);
        return false;
    }

    // viture_capture_and_override_display_mode_locked();
    connected = true;
    viture_refresh_sbs_state_locked();
    return true;
}

static void viture_stop_stream_locked() {
    if (viture_provider == NULL) return;

    // viture_unregister_state_callback_locked();
    // viture_restore_display_mode_locked();

    if (viture_imu_open) {
        close_imu(viture_provider);
        viture_imu_open = false;
        if (config()->debug_device) {
            log_debug("VITURE: Closed IMU stream\n");
        }
    }

    if (connected) {
        int stop_result = xr_device_provider_stop(viture_provider);
        if (stop_result != 0 && config()->debug_device) {
            log_debug("VITURE: xr_device_provider_stop returned %d\n", stop_result);
        } else if (config()->debug_device && stop_result == 0) {
            log_debug("VITURE: Provider stop succeeded\n");
        }
        connected = false;
    }
}

static void viture_shutdown_provider_locked() {
    if (!initialized || viture_provider == NULL) return;

    if (xr_device_provider_shutdown(viture_provider) != 0 && config()->debug_device) {
        log_debug("VITURE: xr_device_provider_shutdown reported an error\n");
    }
    xr_device_provider_destroy(viture_provider);
    viture_provider = NULL;
    initialized = false;
    viture_callbacks_registered = false;
    viture_state_callback_registered = false;
    viture_saved_display_mode = -1;
    viture_saved_dof = -1;
    if (config()->debug_device) {
        log_debug("VITURE: Provider shutdown complete\n");
    }
}

static void viture_update_device_properties(device_properties_type* device) {
    if (device == NULL) return;

    int cycles_per_s;
    bool provides_position = false;
    if (viture_device_type == XR_DEVICE_TYPE_VITURE_CARINA) {
        cycles_per_s = VITURE_CARINA_CYCLES_PER_S;
        provides_position = true;
    } else {
        cycles_per_s = viture_frequency_hz[viture_requested_frequency];
    }

    device->imu_cycles_per_s = cycles_per_s;
    device->imu_buffer_size = cycles_per_s / 60;
    if (device->imu_buffer_size < 1) device->imu_buffer_size = 1;
    device->provides_position = provides_position;
    device->sbs_mode_supported = true;
    device->firmware_update_recommended = false;
}

static void disconnect(bool soft) {
    if (config()->debug_device) {
        log_debug("VITURE: Disconnect requested (soft=%d)\n", soft);
    }
    pthread_mutex_lock(&viture_connection_mutex);
    viture_stop_stream_locked();
    viture_shutdown_provider_locked();
    pthread_mutex_unlock(&viture_connection_mutex);
}

bool viture_device_connect() {
    if (connected) return true;

    device_properties_type* device = device_checkout();
    uint16_t product_id = device ? device->hid_product_id : viture_last_product_id;
    bool success = true;

    pthread_mutex_lock(&viture_connection_mutex);
    if (!initialized) {
        success = viture_initialize_provider_locked(product_id);
    }

    if (success) {
        success = viture_start_stream_locked();
    }
    pthread_mutex_unlock(&viture_connection_mutex);

    if (!success) {
        if (config()->debug_device) {
            log_debug("VITURE: Connection attempt failed, cleaning up\n");
        }
        if (device != NULL) device_checkin(device);
        disconnect(true);
        return false;
    }

    if (device != NULL) {
        viture_update_device_properties(device);
        device_checkin(device);
    }

    if (config()->debug_device) {
        log_debug("VITURE: viture_device_connect completed (connected=%d)\n", connected);
    }

    return connected;
}

void viture_block_on_device() {
    if (connected) {
        wait_for_imu_start();
        while (connected) {
            if (!is_imu_alive()) break;
            sleep(1);
        }
    }

    disconnect(true);
};

bool viture_device_is_sbs_mode() {
    if (viture_provider == NULL || !connected) return false;

    pthread_mutex_lock(&viture_connection_mutex);
    int mode = xr_device_provider_get_display_mode(viture_provider);
    if (mode >= 0) sbs_mode_enabled = viture_display_mode_is_sbs(mode);
    pthread_mutex_unlock(&viture_connection_mutex);

    return sbs_mode_enabled;
};

bool viture_device_set_sbs_mode(bool enabled) {
    pthread_mutex_lock(&viture_connection_mutex);
    bool success = false;
    if (viture_provider != NULL && connected) {
        success = xr_device_provider_switch_dimension(viture_provider, enabled) == 0;
        if (success) {
            sbs_mode_enabled = enabled;
            if (config()->debug_device) {
                log_debug("VITURE: SBS mode set to %d\n", enabled);
            }
        } else if (config()->debug_device) {
            log_debug("VITURE: Failed to set SBS mode to %d\n", enabled);
        }
    }
    pthread_mutex_unlock(&viture_connection_mutex);
    return success;
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
