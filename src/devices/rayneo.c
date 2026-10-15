#include "connection_pool.h"
#include "device_imu.h"
#include "devices.h"
#include "devices/rayneo.h"
#include "driver.h"
#include "imu.h"
#include "imu_protocol.h"
#include "logging.h"
#include "outputs.h"
#include "rayneo_api.h"
#include "runtime_context.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TS_TO_MS_FACTOR 1000000ULL
#define EXPECTED_CYCLES_PER_S 500
#define EXPECTED_CYCLE_TIME_MS (1000.0 / EXPECTED_CYCLES_PER_S)
#define BUFFER_SIZE_TARGET_MS 10

#define RAYNEO_ID_VENDOR 0x1bbb
#define RAYNEO_ID_PRODUCT 0xaf50
#define RAYNEO_STATIC_ID 0x5241594e

#define RAYNEO_DRIVER_ID "rayneo"

#define RAYNEO_RAW_ACCEL_TO_G 1.0f / 9.80665f

typedef struct {
    uint8_t board_id;
    const char* model_name;
} rayneo_model_mapping;

static const rayneo_model_mapping rayneo_model_mappings[] = {
    {0x21, "Air 1s"},
    {0x22, "Air Plus"},
    {0x23, "Air Plus"},
    {0x24, "Aries 1.8"},
    {0x29, "Aries LX"},
    {0x30, "Air 2"},
    {0x31, "Air 2s"},
    {0x34, "Air 3s Pro"},
    {0x38, "Air 3s Pro"},
    {0x35, "Air 3s"},
    {0x36, "Air 3"},
    {0x37, "Air 3 Pro"},
    {0x39, "Air 4"},
    {0x3A, "Air 4 Pro"},
    {0x40, "GT"},
    {0x41, "GT MAX"},
};

static const char* rayneo_unknown_model_name = "Unknown model";

const device_properties_type rayneo_properties = {
    .brand                              = "RayNeo",
    .model                              = "Unknown model",
    .hid_vendor_id                      = RAYNEO_ID_VENDOR,
    .hid_product_id                     = RAYNEO_ID_PRODUCT,
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .pitch_adjustment_degrees           = -14.0,
    .resolution_w                       = RESOLUTION_1080P_W,
    .resolution_h                       = RESOLUTION_1080P_H,
    .fov                                = 43.0,
    .lens_distance_ratio                = 0.05,
    .calibration_wait_s                 = 5,
    .imu_cycles_per_s                   = EXPECTED_CYCLES_PER_S,
    .imu_buffer_size                    = ceil(BUFFER_SIZE_TARGET_MS / EXPECTED_CYCLE_TIME_MS),
    .look_ahead_constant                = 10.0,
    .look_ahead_frametime_multiplier    = 0.45,
    .look_ahead_scanline_adjust         = 7.0,
    .look_ahead_ms_cap                  = 40.0,
    .sbs_mode_supported                 = true,
    .firmware_update_recommended        = false,
    .provides_orientation               = true,
    .provides_position                  = false
};

static pthread_mutex_t device_connection_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t device_info_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t device_info_cond = PTHREAD_COND_INITIALIZER;

static bool hard_connected = false;
static bool soft_connected = false;
static bool is_sbs_mode = false;
static bool device_info_received = false;
static uint8_t rayneo_board_id = 0;
static const char* rayneo_model_name = NULL;

static RAYNEO_Context rayneo_ctx = NULL;
static device_imu_type rayneo_fusion_imu;
static bool rayneo_fusion_open = false;
static imu_sample rayneo_raw_sample;
static bool rayneo_raw_pending = false;
static uint64_t rayneo_timestamp_origin_ns = 0;

static uint64_t rayneo_monotonic_time_ns(void) {
    struct timespec ts = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static uint64_t rayneo_relative_time_ns(void) {
    const uint64_t now_ns = rayneo_monotonic_time_ns();
    if (now_ns == 0) return 0;
    if (rayneo_timestamp_origin_ns == 0) {
        rayneo_timestamp_origin_ns = now_ns;
        return 0;
    }
    return now_ns - rayneo_timestamp_origin_ns;
}

static const char* rayneo_model_from_board_id(uint8_t board_id) {
    const size_t mapping_count = sizeof(rayneo_model_mappings) / sizeof(rayneo_model_mappings[0]);
    for (size_t i = 0; i < mapping_count; i++) {
        if (rayneo_model_mappings[i].board_id == board_id) {
            return rayneo_model_mappings[i].model_name;
        }
    }

    return rayneo_unknown_model_name;
}

static bool rayneo_bridge_open(device_imu_type* dev, const imu_hid_info* info) {
    (void)info;
    if (!dev) return false;
    dev->handle = &rayneo_fusion_imu;
    return true;
}

static void rayneo_bridge_close(device_imu_type* dev) {
    if (dev) dev->handle = NULL;
}

static bool rayneo_bridge_start_stream(device_imu_type* dev) {
    (void)dev;
    return true;
}

static bool rayneo_bridge_stop_stream(device_imu_type* dev) {
    (void)dev;
    return true;
}

static bool rayneo_bridge_get_static_id(device_imu_type* dev, uint32_t* out_id) {
    (void)dev;
    if (out_id) *out_id = RAYNEO_STATIC_ID;
    return true;
}

static bool rayneo_bridge_load_calibration_json(device_imu_type* dev, uint32_t* len, char** data) {
    (void)dev;
    if (len) *len = 0;
    if (data) *data = NULL;
    return false;
}

static int rayneo_bridge_next_sample(device_imu_type* dev, imu_sample* out, int timeout_ms) {
    (void)timeout_ms;
    if (!dev || !out) return -1;
    if (!rayneo_raw_pending) return 0;
    *out = rayneo_raw_sample;
    rayneo_raw_pending = false;
    return 1;
}

static const imu_protocol rayneo_imu_bridge_protocol = {
    .open                  = rayneo_bridge_open,
    .close                 = rayneo_bridge_close,
    .start_stream          = rayneo_bridge_start_stream,
    .stop_stream           = rayneo_bridge_stop_stream,
    .get_static_id         = rayneo_bridge_get_static_id,
    .load_calibration_json = rayneo_bridge_load_calibration_json,
    .next_sample           = rayneo_bridge_next_sample,
};

static void rayneo_fusion_event(uint64_t timestamp, device_imu_event_type event,
                                const device_imu_ahrs_type* ahrs) {
    if (event != DEVICE_IMU_EVENT_UPDATE || !soft_connected || driver_disabled()) return;

    device_imu_quat_type q = device_imu_get_orientation(ahrs);
    // imu_quat_type nwu = {.w = q.w, .x = q.z, .y = q.y, .z = -q.x};
    imu_quat_type nwu = {.w = q.w, .x = q.y, .y = q.z, .z = q.x};

    imu_pose_type pose = {0};
    pose.orientation = nwu;
    pose.has_orientation = true;
    pose.timestamp_ms = (uint32_t)(timestamp / TS_TO_MS_FACTOR);
    connection_pool_ingest_pose(RAYNEO_DRIVER_ID, pose);
}

static bool rayneo_fusion_start_locked(void) {
    if (rayneo_fusion_open) return true;

    memset(&rayneo_fusion_imu, 0, sizeof(rayneo_fusion_imu));
    rayneo_raw_pending = false;
    rayneo_timestamp_origin_ns = 0;

    const imu_hid_info info = {
        .product_id = RAYNEO_ID_PRODUCT,
        .interface_number = -1,
        .path = NULL,
    };
    if (!rayneo_imu_bridge_protocol.open(&rayneo_fusion_imu, &info)) {
        log_error("RayNeo driver, failed to open IMU fusion bridge\n");
        return false;
    }
    rayneo_fusion_imu.protocol = &rayneo_imu_bridge_protocol;
    rayneo_fusion_imu.sample_rate = EXPECTED_CYCLES_PER_S;

    device_imu_error_type err = device_imu_open(&rayneo_fusion_imu, rayneo_fusion_event);
    if (err != DEVICE_IMU_ERROR_NO_ERROR) {
        log_error("RayNeo driver, device_imu_open failed for fusion bridge (%d)\n", err);
        device_imu_close(&rayneo_fusion_imu);
        return false;
    }

    rayneo_fusion_open = true;
    if (config()->debug_device) {
        log_debug("RayNeo driver, IMU fusion bridge started\n");
    }
    return true;
}

static void rayneo_fusion_stop_locked(void) {
    if (!rayneo_fusion_open) return;

    device_imu_close(&rayneo_fusion_imu);
    rayneo_fusion_open = false;
    rayneo_raw_pending = false;
    rayneo_timestamp_origin_ns = 0;
    if (config()->debug_device) {
        log_debug("RayNeo driver, IMU fusion bridge stopped\n");
    }
}

static bool rayneo_command_ok(RAYNEO_Result result, const char* action) {
    if (result == RAYNEO_OK) return true;
    log_error("RayNeo driver, failed to %s (%s)\n", action, Rayneo_ResultToString(result));
    return false;
}

static bool rayneo_request_device_info_locked(void) {
    pthread_mutex_lock(&device_info_mutex);
    device_info_received = false;
    pthread_mutex_unlock(&device_info_mutex);
    return rayneo_command_ok(Rayneo_RequestDeviceInfo(rayneo_ctx), "request device info");
}

static bool rayneo_wait_for_device_info(int timeout_ms) {
    struct timespec deadline = {0};
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return false;

    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&device_info_mutex);
    while (!device_info_received) {
        int wait_result = pthread_cond_timedwait(&device_info_cond, &device_info_mutex, &deadline);
        if (wait_result != 0) break;
    }
    const bool received = device_info_received;
    pthread_mutex_unlock(&device_info_mutex);
    return received;
}

static void rayneo_stop_context_locked(void) {
    if (rayneo_ctx != NULL) {
        Rayneo_Stop(rayneo_ctx);
        Rayneo_Destroy(rayneo_ctx);
        rayneo_ctx = NULL;
    }
    hard_connected = false;
    soft_connected = false;
    is_sbs_mode = false;
    rayneo_board_id = 0;
    rayneo_model_name = rayneo_unknown_model_name;
    rayneo_raw_pending = false;
    rayneo_timestamp_origin_ns = 0;
    pthread_mutex_lock(&device_info_mutex);
    device_info_received = false;
    pthread_cond_broadcast(&device_info_cond);
    pthread_mutex_unlock(&device_info_mutex);
}

static void rayneo_event_callback(const RAYNEO_Event* evt, void* user) {
    (void)user;
    if (evt == NULL) return;

    switch (evt->type) {
    case RAYNEO_EVENT_IMU_SAMPLE: {
        if (!rayneo_fusion_open || driver_disabled()) return;

        imu_sample sample = {0};
        sample.gx = evt->data.imu.gyroDps[2];
        sample.gy = -evt->data.imu.gyroDps[1];
        sample.gz = evt->data.imu.gyroDps[0];
        sample.ax = evt->data.imu.acc[2] * RAYNEO_RAW_ACCEL_TO_G;
        sample.ay = -evt->data.imu.acc[1] * RAYNEO_RAW_ACCEL_TO_G;
        sample.az = evt->data.imu.acc[0] * RAYNEO_RAW_ACCEL_TO_G;
        sample.mx = evt->data.imu.magnet[2];
        sample.my = -evt->data.imu.magnet[1];
        sample.mz = evt->data.imu.magnet[0];
        sample.temperature_c = evt->data.imu.temperature;
        sample.timestamp_ns = rayneo_relative_time_ns();
        sample.flags = 0;

        rayneo_raw_sample = sample;
        rayneo_raw_pending = true;
        device_imu_read(&rayneo_fusion_imu, 0);
        return;
    }
    case RAYNEO_EVENT_DEVICE_INFO:
        if (config()->debug_device) log_debug("RayNeo driver, device info received\n");
        pthread_mutex_lock(&device_info_mutex);
        rayneo_board_id = evt->data.info.board_id;
        rayneo_model_name = rayneo_model_from_board_id(rayneo_board_id);
        is_sbs_mode = evt->data.info.side_by_side == 1;
        device_info_received = true;
        pthread_cond_broadcast(&device_info_cond);
        pthread_mutex_unlock(&device_info_mutex);
        if (config()->debug_device) {
            log_debug("RayNeo driver, device info received: board=0x%02x model=%s fps=%u sbs=%u gyro_valid=%u magnet_valid=%u\n",
                      evt->data.info.board_id,
                      rayneo_model_name,
                      evt->data.info.glasses_fps,
                      evt->data.info.side_by_side,
                      evt->data.info.gyro_valid,
                      evt->data.info.magnet_valid);
        }
        return;
    case RAYNEO_EVENT_DEVICE_DETACHED:
        if (config()->debug_device) log_debug("RayNeo driver, device detached\n");
        return;
    case RAYNEO_EVENT_NOTIFY:
        if (config()->debug_device) log_debug("RayNeo driver, notify code=%d\n", evt->data.notify.code);
        return;
    case RAYNEO_EVENT_ERROR:
        log_error("RayNeo driver, SDK error %d\n", evt->data.error.code);
        return;
    default:
        return;
    }
}

static bool rayneo_start_context_locked(void) {
    if (rayneo_ctx != NULL) return true;

    if (!rayneo_command_ok(Rayneo_Create(&rayneo_ctx), "create SDK context")) return false;
    if (!rayneo_command_ok(Rayneo_SetTargetVidPid(rayneo_ctx, RAYNEO_ID_VENDOR, RAYNEO_ID_PRODUCT),
                           "set VID/PID")) {
        Rayneo_Destroy(rayneo_ctx);
        rayneo_ctx = NULL;
        return false;
    }
    if (!rayneo_command_ok(Rayneo_SetEventCallback(rayneo_ctx, rayneo_event_callback, NULL),
                           "register event callback")) {
        Rayneo_Destroy(rayneo_ctx);
        rayneo_ctx = NULL;
        return false;
    }
    if (!rayneo_command_ok(Rayneo_Start(rayneo_ctx, 0), "start SDK service")) {
        Rayneo_Destroy(rayneo_ctx);
        rayneo_ctx = NULL;
        return false;
    }

    hard_connected = true;
    return true;
}

bool rayneo_device_connect() {
    pthread_mutex_lock(&device_connection_mutex);
    if (!soft_connected) {
        const bool created_context = !hard_connected;
        if ((!hard_connected && !rayneo_start_context_locked()) || !hard_connected) {
            pthread_mutex_unlock(&device_connection_mutex);
            return false;
        }

        if (!rayneo_fusion_start_locked()) {
            if (created_context) rayneo_stop_context_locked();
            pthread_mutex_unlock(&device_connection_mutex);
            return false;
        }

        soft_connected = true;
        if (!rayneo_command_ok(Rayneo_EnableImu(rayneo_ctx), "enable IMU")) {
            soft_connected = false;
            rayneo_fusion_stop_locked();
            if (created_context) rayneo_stop_context_locked();
            pthread_mutex_unlock(&device_connection_mutex);
            return false;
        }

        if (!rayneo_request_device_info_locked() && config()->debug_device) {
            log_debug("RayNeo driver, continuing without fresh device info\n");
        }
    }
    pthread_mutex_unlock(&device_connection_mutex);

    return soft_connected;
}

void rayneo_disconnect(bool soft) {
    pthread_mutex_lock(&device_connection_mutex);
    if (soft_connected) {
        rayneo_command_ok(Rayneo_DisableImu(rayneo_ctx), "disable IMU");
        rayneo_fusion_stop_locked();
        soft_connected = false;
    }

    if (!soft) {
        // device physically unplugged, kill the context too
        rayneo_stop_context_locked();
    }
    pthread_mutex_unlock(&device_connection_mutex);
}

device_properties_type* rayneo_supported_device(uint16_t vendor_id, uint16_t product_id, uint8_t usb_bus, uint8_t usb_address) {
    (void)usb_bus;
    (void)usb_address;
    if (vendor_id != RAYNEO_ID_VENDOR || product_id != RAYNEO_ID_PRODUCT) return NULL;

    device_properties_type* device = calloc(1, sizeof(device_properties_type));
    if (device == NULL) return NULL;
    *device = rayneo_properties;
    if (rayneo_model_name == NULL) rayneo_model_name = rayneo_unknown_model_name;

    sleep(2);

    if (rayneo_device_connect()) {
        rayneo_wait_for_device_info(1500);
        device->model = (char*)rayneo_model_name;

        if (driver_disabled()) rayneo_disconnect(true);
        return device;
    }

    free(device);
    return NULL;
}

void rayneo_block_on_device() {
    bool imu_started = soft_connected && device_present() && wait_for_imu_start();
    while (imu_started && is_imu_alive()) {
        sleep(1);
    }

    rayneo_disconnect(true);
}

bool rayneo_device_is_sbs_mode() {
    return is_sbs_mode;
}

bool rayneo_device_set_sbs_mode(bool enabled) {
    pthread_mutex_lock(&device_connection_mutex);
    bool success = false;
    if (rayneo_ctx != NULL && hard_connected) {
        success = enabled ? rayneo_command_ok(Rayneo_DisplaySet3D(rayneo_ctx), "switch to 3D mode")
                          : rayneo_command_ok(Rayneo_DisplaySet2D(rayneo_ctx), "switch to 2D mode");
        if (success) rayneo_request_device_info_locked();
    }
    pthread_mutex_unlock(&device_connection_mutex);

    return success;
}

bool rayneo_is_connected() {
    return soft_connected;
}

const device_driver_type rayneo_driver = {
    .id                                 = RAYNEO_DRIVER_ID,
    .supported_device_func              = rayneo_supported_device,
    .device_connect_func                = rayneo_device_connect,
    .block_on_device_func               = rayneo_block_on_device,
    .device_is_sbs_mode_func            = rayneo_device_is_sbs_mode,
    .device_set_sbs_mode_func           = rayneo_device_set_sbs_mode,
    .is_connected_func                  = rayneo_is_connected,
    .disconnect_func                    = rayneo_disconnect
};
