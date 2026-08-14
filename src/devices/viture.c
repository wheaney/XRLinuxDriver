#include "connection_pool.h"
#include "device_imu.h"
#include "devices.h"
#include "devices/viture.h"
#include "driver.h"
#include "epoch.h"
#include "imu.h"
#include "imu_protocol.h"
#include "logging.h"
#include "memory.h"
#include "outputs.h"
#include "runtime_context.h"
#include "sdks/viture_device.h"
#include "sdks/viture_device_carina.h"
#include "sdks/viture_glasses_provider.h"
#include "sdks/viture_protocol_public.h"
#include "sdks/viture_result.h"
#include "strings.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VITURE_MODEL_COUNT 9
#define VITURE_MODEL_NONE -1
#define VITURE_MARKET_NAME_MAX 64
#define VITURE_ID_VENDOR 0x35ca
#define VITURE_DRIVER_ID "viture"
#define VITURE_IMU_FREQUENCY_COUNT 6
#define VITURE_IMU_FREQUENCY_DEFAULT VITURE_IMU_FREQUENCY_MEDIUM_HIGH
#define VITURE_CARINA_CYCLES_PER_S 1000
#define VITURE_CARINA_POLL_INTERVAL_US (1000000 / VITURE_CARINA_CYCLES_PER_S)
#define VITURE_FLOAT_EXACT_MS_LIMIT 16777216.0 // 2^24, largest consecutive int in float
#define VITURE_FLOAT_RESET_MARGIN_MS 1000.0 // avoid running right at the precision edge

#define VITURE_LOG_LEVEL_NONE 0
#define VITURE_LOG_LEVEL_ERROR 1
#define VITURE_LOG_LEVEL_INFO 2
#define VITURE_LOG_LEVEL_DEBUG 3

// gyro raw is rad/s, convert to deg/s for the xrDeviceKit
#define VITURE_RAW_GYRO_TO_DPS 57.29577951308232f // 180/pi
#define VITURE_RAW_ACCEL_TO_G 1.0f

static const char* viture_model_names[VITURE_MODEL_COUNT] = {
    VITURE_MARKET_NAME_ONE,
    VITURE_MARKET_NAME_LITE,
    VITURE_MARKET_NAME_PRO,
    VITURE_MARKET_NAME_PRO2,
    VITURE_MARKET_NAME_LUMA,
    VITURE_MARKET_NAME_LUMA_PRO,
    VITURE_MARKET_NAME_LUMA_ULTRA,
    VITURE_MARKET_NAME_LUMA_CYBER,
    VITURE_MARKET_NAME_BEAST
};
static const float viture_pitch_adjustments[VITURE_MODEL_COUNT] = {
    2.0,  // One
    2.0,  // Lite
    3.0,  // Pro
    3.0,  // Pro 2 (TBD)
    -8.5, // Luma
    -8.5, // Luma Pro
    -8.5, // Luma Ultra
    -8.5, // Luma Cyber
    7.0   // Beast
};
static const float viture_fovs[VITURE_MODEL_COUNT] = {
    40.0, // One
    40.0, // Lite
    43.0, // Pro
    43.0, // Pro 2 (TBD)
    48.5, // Luma
    50.0, // Luma Pro
    52.0, // Luma Ultra
    52.0, // Luma Cyber
    53.0  // Beast
};
static const int viture_resolution_heights[VITURE_MODEL_COUNT] = {
    RESOLUTION_1080P_H, // One
    RESOLUTION_1080P_H, // Lite
    RESOLUTION_1080P_H, // Pro
    RESOLUTION_1080P_H, // Pro 2 (TBD)
    RESOLUTION_1200P_H, // Luma
    RESOLUTION_1200P_H, // Luma Pro
    RESOLUTION_1200P_H, // Luma Ultra
    RESOLUTION_1200P_H, // Luma Cyber
    RESOLUTION_1200P_H  // Beast
};

static const int viture_calibration_wait_s[VITURE_MODEL_COUNT] = {
    1,  // One
    1,  // Lite
    1,  // Pro
    1,  // Pro 2 (TBD)
    1,  // Luma
    5,  // Luma Pro
    10, // Luma Ultra
    5,  // Luma Cyber (TBD)
    15  // Beast
};

static const int viture_look_ahead_constant[VITURE_MODEL_COUNT] = {
    20, // One
    20, // Lite
    20, // Pro
    20, // Pro 2 (TBD)
    20, // Luma
    20, // Luma Pro
    10, // Luma Ultra
    10, // Luma Cyber (TBD)
    20  // Beast
};

static imu_quat_type adjustment_quat;
static XRDeviceProviderHandle viture_provider = NULL;
static XRDeviceType viture_device_type = XR_DEVICE_TYPE_VITURE_GEN1;
static uint16_t viture_last_product_id = 0;
static pthread_mutex_t viture_connection_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool connected = false;
static bool initialized = false;
static bool viture_state_callback_registered = false;
static bool viture_imu_open = false;
static bool viture_use_raw_fusion = false;
static uint8_t viture_requested_frequency = VITURE_IMU_FREQUENCY_DEFAULT;

static device_imu_type viture_fusion_imu;
static bool viture_fusion_open = false;
static imu_sample viture_raw_sample;
static bool viture_raw_pending = false;
static bool sbs_mode_enabled = false;
static int viture_saved_display_mode = -1;
static int viture_saved_native_mode = -1;
static int viture_saved_dof = -1;
static int viture_saved_display_size = -1;
static int viture_callback_logs_remaining = 10;

static const int viture_frequency_hz[VITURE_IMU_FREQUENCY_COUNT] = {60, 90, 120, 240, 500, 1000};

// highest frequency the SDK reports for this product/mode, defaulting to the previous behavior
static uint8_t viture_best_frequency(uint16_t product_id, uint8_t imu_mode) {
    for (int frequency = VITURE_IMU_FREQUENCY_COUNT - 1; frequency >= 0; frequency--) {
        if (xr_device_provider_is_product_support_imu_frequency(product_id, imu_mode, frequency) == 1) {
            return (uint8_t)frequency;
        }
    }

    if (config()->debug_device) {
        log_debug("VITURE: SDK reported no supported IMU frequencies for 0x%04x mode %d, using %dHz\n",
                  product_id,
                  imu_mode,
                  viture_frequency_hz[VITURE_IMU_FREQUENCY_DEFAULT]);
    }
    return VITURE_IMU_FREQUENCY_DEFAULT;
}

static void viture_apply_imu_rate(device_properties_type* device, int cycles_per_s) {
    device->imu_cycles_per_s = cycles_per_s;
    device->imu_buffer_size = cycles_per_s / 60;
    if (device->imu_buffer_size < 1) device->imu_buffer_size = 1;
}

static const char* viture_open_imu_error_reason(int code) {
    switch (code) {
    case VITURE_GLASSES_ERROR_INVALID_PARAM: return "param error";
    case VITURE_GLASSES_ERROR_USB_UNAVAILABLE: return "USB unavailable";
    case VITURE_GLASSES_ERROR_USB_EXEC: return "USB execution error";
    case VITURE_GLASSES_ERROR_NOT_SUPPORTED: return "device type not supported";
    case VITURE_GLASSES_ERROR_UNKNOWN: return "other error";
    default: return "unknown";
    }
}

static const device_properties_type viture_properties = {
    .brand                              = "VITURE",
    .model                              = NULL,
    .hid_vendor_id                      = VITURE_ID_VENDOR,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = RESOLUTION_1080P_W,
    .lens_distance_ratio                = 0.05,
    .imu_cycles_per_s                   = 60,
    .imu_buffer_size                    = 1,
    .look_ahead_frametime_multiplier    = 0.6,
    .look_ahead_scanline_adjust         = 10.0,
    .look_ahead_ms_cap                  = 40.0,
    .sbs_mode_supported                 = true,
    .firmware_update_recommended        = false,
    .provides_orientation               = true,
    .provides_position                  = false
};

static bool viture_supports_native_dof(void) {
    return viture_device_type == XR_DEVICE_TYPE_VITURE_GEN2 &&
           xr_device_provider_is_product_support_native_dof(viture_last_product_id) == 1;
}

static int viture_get_native_mode_locked(void) {
    if (viture_provider == NULL) return VITURE_GLASSES_ERROR_INVALID_PARAM;
    if (!viture_supports_native_dof()) return 0;
    return xr_device_provider_native_get_mode(viture_provider);
}

static bool viture_bypass_display_mode_is_sbs(int mode) {
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

static bool viture_native_display_mode_is_sbs(int mode) {
    switch (mode) {
    case VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_60HZ:
    case VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_90HZ:
    case VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1080_120HZ:
    case VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1200_60HZ:
    case VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1200_90HZ:
    case VITURE_NATIVE_DISPLAY_MODE_3D_SBS_3840_1200_120HZ:
        return true;
    default:
        return false;
    }
}

static bool viture_display_mode_is_sbs(int mode, bool native_mode) {
    return native_mode ? viture_native_display_mode_is_sbs(mode)
                       : viture_bypass_display_mode_is_sbs(mode);
}

static bool viture_get_active_display_state_locked(int* mode, bool* native_mode) {
    if (viture_provider == NULL || mode == NULL) return false;

    int current_native_mode = viture_get_native_mode_locked();
    if (current_native_mode == 1) {
        int current_mode = xr_device_provider_native_get_display_mode(viture_provider);
        if (current_mode < 0) return false;
        *mode = current_mode;
        if (native_mode != NULL) *native_mode = true;
        return true;
    }

    if (current_native_mode < 0 && config()->debug_device) {
        log_debug("VITURE: Failed to query native mode (%d), falling back to bypass display mode\n",
                  current_native_mode);
    }

    int current_mode = xr_device_provider_get_display_mode(viture_provider);
    if (current_mode < 0) return false;
    *mode = current_mode;
    if (native_mode != NULL) *native_mode = false;
    return true;
}

static bool viture_get_display_state_locked(int* native_mode, int* display_mode, int* dof, int* display_size) {
    if (viture_provider == NULL || native_mode == NULL || display_mode == NULL || dof == NULL ||
        display_size == NULL) {
        return false;
    }

    int current_native_mode = viture_get_native_mode_locked();
    if (current_native_mode < 0) return false;

    *native_mode = current_native_mode;
    if (current_native_mode == 1) {
        *display_mode = xr_device_provider_native_get_display_mode(viture_provider);
        if (*display_mode < 0) return false;
        *dof = xr_device_provider_native_get_dof(viture_provider);
        if (*dof < 0) return false;
        *display_size = xr_device_provider_native_get_display_size(viture_provider);
        return *display_size >= 0;
    }

    *display_mode = xr_device_provider_get_display_mode(viture_provider);
    *dof = VITURE_NATIVE_DOF_0;
    *display_size = -1;
    return *display_mode >= 0;
}

static bool viture_restore_display_state_locked(int native_mode, int display_mode, int dof, int display_size) {
    if (viture_provider == NULL) return false;

    if (!viture_supports_native_dof())
        return false;

    bool success = true;
    if (native_mode >= 0) success &= xr_device_provider_native_set_mode(viture_provider, native_mode) == 0;
    if (display_mode >= 0) success &= xr_device_provider_native_set_display_mode(viture_provider, display_mode) == 0;
    if (display_size >= 0) success &= xr_device_provider_native_set_display_size(viture_provider, display_size) == 0;
    if (dof >= 0) success &= xr_device_provider_native_set_dof(viture_provider, dof) == 0;
    return success;
}

static bool viture_switch_dimension_locked(bool enabled) {
    int current_native_mode = viture_get_native_mode_locked();
    if (current_native_mode == 1) {
        return xr_device_provider_native_switch_dimension(viture_provider, enabled) == 0;
    }

    if (current_native_mode < 0 && config()->debug_device) {
        log_debug("VITURE: Failed to query native mode before switching SBS (%d), falling back to bypass path\n",
                  current_native_mode);
    }

    return xr_device_provider_switch_dimension(viture_provider, enabled) == 0;
}

static void viture_refresh_sbs_state_locked() {
    if (viture_provider == NULL) return;
    int mode = 0;
    bool native_mode = false;
    if (viture_get_active_display_state_locked(&mode, &native_mode)) {
        sbs_mode_enabled = viture_display_mode_is_sbs(mode, native_mode);
    } else if (config()->debug_device) {
        log_debug("VITURE: Failed to refresh SBS state\n");
    }
}

static void viture_capture_and_override_display_mode_locked() {
    viture_saved_display_mode = -1;
    viture_saved_native_mode = -1;
    viture_saved_dof = -1;
    viture_saved_display_size = -1;

    if (viture_provider == NULL) {
        if (config()->debug_device) {
            log_debug("VITURE: Cannot override display mode, provider NULL\n");
        }
        return;
    }

    if (!viture_supports_native_dof()) {
        if (config()->debug_device) {
            log_debug("VITURE: Device has no native DoF support, no display override needed\n");
        }
        viture_refresh_sbs_state_locked();
        return;
    }

    int native_mode = 0;
    int mode = 0;
    int dof = VITURE_NATIVE_DOF_0;
    int display_size = -1;
    if (!viture_get_display_state_locked(&native_mode, &mode, &dof, &display_size)) {
        if (config()->debug_device) {
            log_debug("VITURE: Unable to read display state during override\n");
        }
        return;
    }

    viture_saved_native_mode = native_mode;
    viture_saved_display_mode = mode;
    viture_saved_dof = dof;
    viture_saved_display_size = display_size;

    bool success = true;
    int status = xr_device_provider_native_set_dof(viture_provider, VITURE_NATIVE_DOF_0);
    if (status != 0 && config()->debug_device) {
        log_debug("VITURE: Failed to set native DoF to 0 (error %d)\n", status);
    }
    success &= status == 0;

    status = xr_device_provider_native_set_display_size(viture_provider, VITURE_DISPLAY_SIZE_EXTRA);
    if (status != 0 && config()->debug_device) {
        log_debug("VITURE: Failed to set display size to EXTRA (error %d)\n", status);
    }
    success &= status == 0;

    status = xr_device_provider_native_set_display_mode(viture_provider, VITURE_NATIVE_DISPLAY_MODE_1920_1200_120HZ);
    if (status != 0 && config()->debug_device) {
        log_debug("VITURE: Failed to set display mode to 1920x1200@120Hz (error %d)\n", status);
    }
    success &= status == 0;

    status = xr_device_provider_native_set_mode(viture_provider, 0);
    if (status != 0 && config()->debug_device) {
        log_debug("VITURE: Failed to set native mode to 0 (error %d)\n", status);
    }
    success &= status == 0;

    if (success) {
        sbs_mode_enabled = viture_native_display_mode_is_sbs(mode);
        if (config()->debug_device) {
            log_debug("VITURE: Overrode display state to mode=%d size=%d 0DoF (saved native_mode=%d mode=%d dof=%d size=%d)\n",
                      VITURE_NATIVE_DISPLAY_MODE_1920_1200_120HZ,
                      VITURE_DISPLAY_SIZE_EXTRA,
                      native_mode,
                      mode,
                      dof,
                      display_size);
        }
    } else {
        viture_refresh_sbs_state_locked();
        if (config()->debug_device) {
            log_debug("VITURE: Failed to override display state (native_mode=%d mode=%d dof=%d size=%d)\n",
                      native_mode,
                      mode,
                      dof,
                      display_size);
        }
    }
}

static void viture_restore_display_mode_locked() {
    if (viture_provider == NULL) {
        viture_saved_display_mode = -1;
        viture_saved_native_mode = -1;
        viture_saved_dof = -1;
        viture_saved_display_size = -1;
        if (config()->debug_device) {
            log_debug("VITURE: Cannot restore display mode, provider NULL\n");
        }
        return;
    }

    if (!viture_supports_native_dof()) return;

    if (viture_saved_display_mode < 0 || viture_saved_native_mode < 0 || viture_saved_dof < 0) {
        if (config()->debug_device) {
            log_debug("VITURE: No saved display state to restore\n");
        }
        return;
    }

    int restore_native_mode = viture_saved_native_mode;
    int restore_mode = viture_saved_display_mode;
    int restore_dof = viture_saved_dof;
    int restore_display_size = viture_saved_display_size;
    bool result = viture_restore_display_state_locked(restore_native_mode, restore_mode, restore_dof,
                                                      restore_display_size);

    if (result) {
        sbs_mode_enabled = viture_display_mode_is_sbs(restore_mode, restore_native_mode == 1);
        if (config()->debug_device) {
            log_debug("VITURE: Restored display mode=%d native_mode=%d dof=%d size=%d\n",
                      restore_mode,
                      restore_native_mode,
                      restore_dof,
                      restore_display_size);
        }
    } else if (config()->debug_device) {
        log_debug("VITURE: Failed to restore display mode=%d native_mode=%d dof=%d size=%d\n",
                  restore_mode,
                  restore_native_mode,
                  restore_dof,
                  restore_display_size);
    }

    viture_saved_display_mode = -1;
    viture_saved_native_mode = -1;
    viture_saved_dof = -1;
    viture_saved_display_size = -1;
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
    connection_pool_ingest_pose(VITURE_DRIVER_ID, pose);
}

static void viture_legacy_pose_callback(float* pose, uint64_t ts) {
    if (!connected || driver_disabled() || pose == NULL) return;

    // pose received in NWU coordinate system
    imu_quat_type quat = {.x = pose[4], .y = pose[5], .z = pose[6], .w = pose[3]};

    uint32_t timestamp_ms = (uint32_t)(ts / 1000000ULL);
    viture_publish_pose(quat, false, (imu_vec3_type){0}, timestamp_ms);
}

static void viture_carina_imu_callback(float* imu, double timestamp) {
    static double initial_timestamp = -1.0;
    device_properties_type* device = device_checkout();
    if (connected && viture_provider != NULL && device != NULL && imu != NULL) {
        float pose[9] = {0};
        int pose_status = 0;
        int result = xr_device_provider_get_gl_pose_carina(viture_provider, pose, 0.0, &pose_status);
        if (result == 0) {
            // pose received in EUS (GL) coordinate system, convert to NWU
            imu_quat_type quat = {.x = -pose[6], .y = -pose[4], .z = pose[5], .w = pose[3]};

            float full_distance_cm = LENS_TO_PIVOT_CM / device->lens_distance_ratio;
            float meters_to_full_distance_ratio = 100.0f / full_distance_cm;
            imu_vec3_type position = {
                .x = -pose[2] * meters_to_full_distance_ratio, 
                .y = -pose[0] * meters_to_full_distance_ratio,
                .z = pose[1] * meters_to_full_distance_ratio
            };

            if (initial_timestamp < 0.0) initial_timestamp = timestamp;

            double elapsed_ms = (timestamp - initial_timestamp) * 1000.0;
            if (elapsed_ms >= VITURE_FLOAT_EXACT_MS_LIMIT - VITURE_FLOAT_RESET_MARGIN_MS) {
                initial_timestamp = timestamp;
                elapsed_ms = 0.0;
            }

            uint32_t timestamp_ms = (uint32_t)elapsed_ms;
            
            viture_publish_pose(quat, true, position, timestamp_ms);
        } else if (config()->debug_device) {
            log_debug("VITURE: get_gl_pose_carina failed (result=%d pose_status=%d)\n",
                      result,
                      pose_status);
        }
    }
    device_checkin(device);
}

static bool viture_bridge_open(device_imu_type* dev, const imu_hid_info* info) {
    (void)info;
    if (!dev) return false;
    dev->handle = &viture_fusion_imu;
    return true;
}

static void viture_bridge_close(device_imu_type* dev) {
    if (dev) dev->handle = NULL;
}

static bool viture_bridge_start_stream(device_imu_type* dev) {
    (void)dev;
    return true;
}

static bool viture_bridge_stop_stream(device_imu_type* dev) {
    (void)dev;
    return true;
}

static bool viture_bridge_get_static_id(device_imu_type* dev, uint32_t* out_id) {
    (void)dev;
    if (out_id) *out_id = 0x56495455;
    return true;
}

static bool viture_bridge_load_calibration_json(device_imu_type* dev, uint32_t* len, char** data) {
    (void)dev;
    if (len) *len = 0;
    if (data) *data = NULL;
    return false;
}

static int viture_bridge_next_sample(device_imu_type* dev, imu_sample* out, int timeout_ms) {
    (void)timeout_ms;
    if (!dev || !out) return -1;
    if (!viture_raw_pending) return 0;
    *out = viture_raw_sample;
    viture_raw_pending = false;
    return 1;
}

static const imu_protocol viture_imu_bridge_protocol = {
    .open                  = viture_bridge_open,
    .close                 = viture_bridge_close,
    .start_stream          = viture_bridge_start_stream,
    .stop_stream           = viture_bridge_stop_stream,
    .get_static_id         = viture_bridge_get_static_id,
    .load_calibration_json = viture_bridge_load_calibration_json,
    .next_sample           = viture_bridge_next_sample,
};

static void viture_fusion_event(uint64_t timestamp, device_imu_event_type event,
                                const device_imu_ahrs_type* ahrs) {
    if (event != DEVICE_IMU_EVENT_UPDATE || !connected || driver_disabled()) return;

    device_imu_quat_type q = device_imu_get_orientation(ahrs);
    imu_quat_type nwu = {.w = -q.x, .x = q.w, .y = q.z, .z = -q.y};

    uint32_t timestamp_ms = (uint32_t)(timestamp / 1000000ULL);
    viture_publish_pose(nwu, false, (imu_vec3_type){0}, timestamp_ms);
}

// data: [gx, gy, gz, ax, ay, az, mx, my, mz, temperature], each triad in EDN order.
// Remap EDN -> the pre-image of NED so device_imu's fusion sees a proper NED frame.
static void viture_imu_raw_callback(float* data, uint64_t timestamp, uint64_t vsync) {
    (void)vsync;
    if (!connected || driver_disabled() || data == NULL || !viture_fusion_open) return;

    imu_sample s = {0};
    s.gx = -data[0] * VITURE_RAW_GYRO_TO_DPS;
    s.gy = -data[2] * VITURE_RAW_GYRO_TO_DPS;
    s.gz = -data[1] * VITURE_RAW_GYRO_TO_DPS;
    s.ax = -data[3] * VITURE_RAW_ACCEL_TO_G;
    s.ay = -data[5] * VITURE_RAW_ACCEL_TO_G;
    s.az = -data[4] * VITURE_RAW_ACCEL_TO_G;
    s.mx = -data[6];
    s.my = -data[8];
    s.mz = -data[7];
    s.temperature_c = data[9];
    s.timestamp_ns = timestamp;
    s.flags = 0;

    viture_raw_sample = s;
    viture_raw_pending = true;
    device_imu_read(&viture_fusion_imu, 0);
}

static bool viture_fusion_start_locked() {
    if (viture_fusion_open) return true;

    memset(&viture_fusion_imu, 0, sizeof(viture_fusion_imu));
    viture_raw_pending = false;

    const imu_hid_info info = {
        .product_id = viture_last_product_id,
        .interface_number = -1,
        .path = NULL,
    };
    if (!viture_imu_bridge_protocol.open(&viture_fusion_imu, &info)) {
        log_error("VITURE: Failed to open IMU fusion bridge\n");
        return false;
    }
    viture_fusion_imu.protocol = &viture_imu_bridge_protocol;
    viture_fusion_imu.sample_rate = viture_frequency_hz[viture_requested_frequency];

    device_imu_error_type err = device_imu_open(&viture_fusion_imu, viture_fusion_event);
    if (err != DEVICE_IMU_ERROR_NO_ERROR) {
        log_error("VITURE: device_imu_open failed for fusion bridge (%d)\n", err);
        device_imu_close(&viture_fusion_imu);
        return false;
    }

    viture_fusion_open = true;
    if (config()->debug_device) {
        log_debug("VITURE: IMU fusion bridge started\n");
    }
    return true;
}

static void viture_fusion_stop_locked() {
    if (!viture_fusion_open) return;

    device_imu_close(&viture_fusion_imu);
    viture_fusion_open = false;
    viture_raw_pending = false;
    if (config()->debug_device) {
        log_debug("VITURE: IMU fusion bridge stopped\n");
    }
}

static uint8_t viture_active_imu_mode() {
    return viture_use_raw_fusion ? VITURE_IMU_MODE_RAW : VITURE_IMU_MODE_POSE;
}

// TODO - the SDK doesn't actually reliably call this yet
static void viture_state_callback(int glass_state_id, int glass_value) {
    if (glass_state_id == VITURE_CALLBACK_ID_DISPLAY_MODE) {
        bool native_mode = viture_get_native_mode_locked() == 1;
        sbs_mode_enabled = viture_display_mode_is_sbs(glass_value, native_mode);
        if (config()->debug_device) {
            log_debug("VITURE: Display mode changed via callback, mode=%d native_mode=%d sbs_enabled=%d\n",
                      glass_value,
                      native_mode,
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

// index into the device detail arrays for the model the SDK reports for this product,
// VITURE_MODEL_NONE for a model we have no properties for
static int viture_model_index(uint16_t product_id) {
    char market_name[VITURE_MARKET_NAME_MAX] = {0};
    int length = VITURE_MARKET_NAME_MAX - 1;
    if (xr_device_provider_get_market_name(product_id, market_name, &length) != VITURE_GLASSES_SUCCESS) {
        log_message("VITURE: SDK reported no market name for product ID 0x%04x\n", product_id);
        return VITURE_MODEL_NONE;
    }
    market_name[VITURE_MARKET_NAME_MAX - 1] = '\0';

    for (int i = 0; i < VITURE_MODEL_COUNT; i++) {
        if (equal(market_name, viture_model_names[i])) return i;
    }

    log_error("VITURE: No device properties for model '%s' (product ID 0x%04x)\n", market_name, product_id);

    return VITURE_MODEL_NONE;
}

static device_properties_type* viture_supported_device(uint16_t vendor_id, uint16_t product_id,
                                                uint8_t usb_bus, uint8_t usb_address) {
    if (vendor_id != VITURE_ID_VENDOR || xr_device_provider_is_product_id_valid(product_id) != 1) return NULL;

    int model_index = viture_model_index(product_id);
    if (model_index == VITURE_MODEL_NONE) return NULL;

    device_properties_type* device = calloc(1, sizeof(device_properties_type));
    *device = viture_properties;
    device->hid_vendor_id = vendor_id;
    device->hid_product_id = product_id;
    device->model = (char *)viture_model_names[model_index];
    device->resolution_h = viture_resolution_heights[model_index];
    device->fov = viture_fovs[model_index];
    device->calibration_wait_s = viture_calibration_wait_s[model_index];
    device->look_ahead_constant = (float)viture_look_ahead_constant[model_index];

    adjustment_quat = device_pitch_adjustment(viture_pitch_adjustments[model_index]);

    uint8_t predicted_mode = xr_device_provider_is_product_support_native_dof(product_id) == 1
                                 ? VITURE_IMU_MODE_RAW
                                 : VITURE_IMU_MODE_POSE;
    viture_requested_frequency = viture_best_frequency(product_id, predicted_mode);
    viture_apply_imu_rate(device, viture_frequency_hz[viture_requested_frequency]);

    viture_last_product_id = product_id;

    return device;
};

static bool viture_initialize_provider_locked(uint16_t product_id) {
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
    viture_use_raw_fusion = false;
    if (viture_device_type == XR_DEVICE_TYPE_VITURE_CARINA) {
        if (config()->debug_device)
            log_debug("VITURE: Registering Carina callback\n");
        register_result =
            xr_device_provider_register_callbacks_carina(viture_provider, NULL, NULL, viture_carina_imu_callback, NULL);
    } else if (viture_device_type == XR_DEVICE_TYPE_VITURE_GEN1 || viture_device_type == XR_DEVICE_TYPE_VITURE_GEN2) {
        if (viture_supports_native_dof()) {
            if (config()->debug_device)
                log_debug("VITURE: Registering raw IMU callback for native-DoF device 0x%04x\n",
                          viture_last_product_id);
            register_result = xr_device_provider_register_imu_raw_callback(viture_provider, viture_imu_raw_callback);
            viture_use_raw_fusion = (register_result == 0);
        } else {
            if (config()->debug_device)
                log_debug("VITURE: Registering IMU pose callback for device type %d\n", viture_device_type);
            register_result = xr_device_provider_register_imu_pose_callback(viture_provider, viture_legacy_pose_callback);
        }
    } else {
        if (config()->debug_device) 
            log_debug("VITURE: Other device type %d, skipping callback registration\n", viture_device_type);
        register_result = 0;
    }

    bool viture_callbacks_registered = (register_result == 0);
    if (!viture_callbacks_registered) {
        log_error("VITURE: Failed to register SDK callbacks (type=%d)\n", viture_device_type);
        xr_device_provider_destroy(viture_provider);
        viture_provider = NULL;
        return false;
    } else if (config()->debug_device) {
        log_debug("VITURE: Callback registration succeeded for type=%d\n", viture_device_type);
    }

    if (xr_device_provider_initialize(viture_provider, NULL, NULL) != 0) {
        log_error("VITURE: Failed to initialize SDK provider\n");
        xr_device_provider_destroy(viture_provider);
        viture_provider = NULL;
        return false;
    }

    if (config()->debug_device) log_debug("VITURE: SDK provider initialized\n");

    initialized = true;
    return true;
}

static bool viture_open_imu_locked() {
    if (viture_imu_open) return true;
    if (viture_device_type != XR_DEVICE_TYPE_VITURE_GEN1 && viture_device_type != XR_DEVICE_TYPE_VITURE_GEN2) {
        if (config()->debug_device) {
            log_debug("VITURE: skipping open_imu for device type %d\n", viture_device_type);
        }
        return true;
    }

    uint8_t imu_mode = viture_active_imu_mode();
    viture_requested_frequency = viture_best_frequency(viture_last_product_id, imu_mode);
    if (config()->debug_device) {
        log_debug("VITURE: Using IMU frequency %dHz (index %d) in mode %d\n",
                  viture_frequency_hz[viture_requested_frequency],
                  viture_requested_frequency,
                  imu_mode);
    }

    int open_result = VITURE_GLASSES_ERROR_UNKNOWN;
    while (true) {
        open_result = xr_device_provider_open_imu(viture_provider, imu_mode, viture_requested_frequency);
        if (open_result == 0) {
            if (config()->debug_device) {
                log_debug("VITURE: open_imu succeeded at %dHz (mode %d)\n",
                          viture_frequency_hz[viture_requested_frequency],
                          imu_mode);
            }
            break;
        }

        log_error("VITURE: open_imu failed at %dHz (%d: %s)\n",
                  viture_frequency_hz[viture_requested_frequency],
                  open_result,
                  viture_open_imu_error_reason(open_result));

        if (viture_requested_frequency == 0) return false;
        viture_requested_frequency--;
    }
    viture_imu_open = true;

    if (viture_use_raw_fusion && !viture_fusion_start_locked()) {
        log_error("VITURE: Failed to start IMU fusion bridge\n");
        xr_device_provider_close_imu(viture_provider, imu_mode);
        viture_imu_open = false;
        return false;
    }

    return true;
}

static bool viture_start_stream_locked() {
    if (!initialized || viture_provider == NULL) return false;

    sleep(1);

    if (xr_device_provider_start(viture_provider) != 0) {
        log_error("VITURE: Failed to start SDK provider\n");
        return false;
    }

    if (config()->debug_device) {
        log_debug("VITURE: Provider start succeeded\n");
    }

    // viture_register_state_callback_locked();

    if (!viture_open_imu_locked()) {
        log_error("VITURE: Failed to open IMU stream\n");
        return false;
    }

    viture_capture_and_override_display_mode_locked();
    connected = true;
    return true;
}

static void viture_stop_stream_locked() {
    if (viture_provider == NULL) return;

    // viture_unregister_state_callback_locked();
    viture_restore_display_mode_locked();

    if (viture_imu_open) {
        xr_device_provider_close_imu(viture_provider, viture_active_imu_mode());
        viture_imu_open = false;
        if (config()->debug_device) {
            log_debug("VITURE: Closed IMU stream\n");
        }
    }

    viture_fusion_stop_locked();

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

    viture_fusion_stop_locked();

    if (xr_device_provider_shutdown(viture_provider) != 0 && config()->debug_device) {
        log_debug("VITURE: xr_device_provider_shutdown reported an error\n");
    }
    xr_device_provider_destroy(viture_provider);
    viture_provider = NULL;
    initialized = false;
    viture_use_raw_fusion = false;
    viture_device_type = XR_DEVICE_TYPE_VITURE_GEN1;
    viture_state_callback_registered = false;
    viture_saved_display_mode = -1;
    viture_saved_native_mode = -1;
    viture_saved_dof = -1;
    viture_saved_display_size = -1;
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

    viture_apply_imu_rate(device, cycles_per_s);
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
    if (!soft) viture_shutdown_provider_locked();
    pthread_mutex_unlock(&viture_connection_mutex);
}

static bool viture_device_connect() {
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

        // do a hard disconnect even though the device is still physically connected
        disconnect(false);
        
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

static void viture_block_on_device() {
    if (connected) {
        wait_for_imu_start();
        while (connected) {
            if (!is_imu_alive()) break;
            sleep(1);
        }
    }

    disconnect(true);
};

static bool viture_device_is_sbs_mode() {
    if (viture_provider == NULL || !connected) return false;

    pthread_mutex_lock(&viture_connection_mutex);
    viture_refresh_sbs_state_locked();
    pthread_mutex_unlock(&viture_connection_mutex);

    return sbs_mode_enabled;
};

static bool viture_device_set_sbs_mode(bool enabled) {
    pthread_mutex_lock(&viture_connection_mutex);
    bool success = false;
    if (viture_provider != NULL && connected) {
        success = viture_switch_dimension_locked(enabled);
        if (success) {
            viture_refresh_sbs_state_locked();
            if (config()->debug_device) {
                log_debug("VITURE: SBS mode set to %d\n", sbs_mode_enabled);
            }
        } else if (config()->debug_device) {
            log_debug("VITURE: Failed to set SBS mode to %d\n", enabled);
        }
    }
    pthread_mutex_unlock(&viture_connection_mutex);
    return success;
};

static bool viture_is_connected() {
    return connected;
};

static void viture_disconnect(bool soft) {
    disconnect(soft);
};

const device_driver_type viture_driver = {
    .id                                 = VITURE_DRIVER_ID,
    .supported_device_func              = viture_supported_device,
    .device_connect_func                = viture_device_connect,
    .block_on_device_func               = viture_block_on_device,
    .device_is_sbs_mode_func            = viture_device_is_sbs_mode,
    .device_set_sbs_mode_func           = viture_device_set_sbs_mode,
    .is_connected_func                  = viture_is_connected,
    .disconnect_func                    = viture_disconnect
};
