#include "devices.h"
#include "devices/rayneo.h"
#include "connection_pool.h"
#include "driver.h"
#include "imu.h"
#include "logging.h"
#include "memory.h"
#include "outputs.h"
#include "runtime_context.h"
#include "sdks/rayneo.h"
#include "strings.h"

#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_TO_MS_FACTOR 1000000
#define EXPECTED_CYCLES_PER_S 500
#define EXPECTED_CYCLE_TIME_MS (1000.0 / EXPECTED_CYCLES_PER_S)
#define BUFFER_SIZE_TARGET_MS 10 // smooth IMU data over this period of time

#define RAYNEO_ID_VENDOR 0x1bbb
#define RAYNEO_ID_PRODUCT 0xaf50

// The GT Max is a newer RayNeo device and reports RayNeo's own USB vendor ID instead of the
// TCL/Alcatel ID (0x1bbb) shared by the older NXTWEAR/Air line, while keeping the same HID
// protocol and product ID.
#define RAYNEO_GT_MAX_ID_VENDOR 0x3941
#define RAYNEO_GT_MAX_ID_PRODUCT 0xaf50

#define STATE_EVENT_DEVICE_INFO 0x4000
#define RAYNEO_DRIVER_ID "rayneo"

// how long to wait for the SDK to report the device name before falling back to a static one
#define RAYNEO_DEVICE_NAME_TIMEOUT_S 10

// how the SDK reports head orientation for a given device. The older NXTWEAR/Air line reports
// rotations in an east-up-south frame that needs converting. The GT Max's IMU is mounted rotated
// about the pitch axis relative to the glasses' body frame, so its roll and yaw are mixed and it
// needs a dedicated relabeling.
typedef enum {
    RAYNEO_IMU_FRAME_EUS,
    RAYNEO_IMU_FRAME_Y_ROTATED,
} rayneo_imu_frame_type;

// Devices that share RayNeo's HID protocol. Some properties (FOV, pitch adjustment) differ per
// device, and the USB IDs make each identifiable. The model here is only a fallback; when the
// SDK reports a device name it takes precedence.
typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    const char* model;
    float fov;
    float pitch_adjustment_degrees;
    rayneo_imu_frame_type imu_frame;
} rayneo_device_id_type;

static const rayneo_device_id_type rayneo_device_ids[] = {
    // TCL/RayNeo NXTWEAR S/S+, Air 2, Air 2s, Air 3s/Pro
    { RAYNEO_ID_VENDOR,        RAYNEO_ID_PRODUCT,        NULL,     43.0, -14.0, RAYNEO_IMU_FRAME_EUS },
    // RayNeo GT Max
    { RAYNEO_GT_MAX_ID_VENDOR, RAYNEO_GT_MAX_ID_PRODUCT, "GT Max", 59.0, -14.0, RAYNEO_IMU_FRAME_Y_ROTATED },
};
#define RAYNEO_DEVICE_ID_COUNT (sizeof(rayneo_device_ids) / sizeof(rayneo_device_ids[0]))

// the USB IDs of the device we're currently connected to, recorded when it was matched
static uint16_t active_device_id_vendor = RAYNEO_ID_VENDOR;
static uint16_t active_device_id_product = RAYNEO_ID_PRODUCT;
static rayneo_imu_frame_type active_imu_frame = RAYNEO_IMU_FRAME_EUS;

// bare soname, so ld.so resolves it exactly as it did when this was a DT_NEEDED
#define RAYNEO_SDK_SONAME "libRayNeoXRMiniSDK.so"

#define RAYNEO_SDK_SYMBOLS \
    X(RegisterIMUEventCallback) X(UnregisterIMUEventCallback) \
    X(RegisterStateEventCallback) X(UnregisterStateEventCallback) \
    X(EstablishUsbConnection) X(ResetUsbConnection) \
    X(NotifyDeviceConnected) X(NotifyDeviceDisconnected) \
    X(StartXR) X(StopXR) X(SwitchTo2D) X(SwitchTo3D) X(OpenIMU) X(CloseIMU) X(Recenter) \
    X(GetHeadTrackerPose) X(ConvertHostTime2DeviceTime) \
    X(GetDeviceType) X(AcquireDeviceInfo) X(GetSideBySideStatus)

void (*RegisterIMUEventCallback)(IMUEventCallback callback) = NULL;
void (*UnregisterIMUEventCallback)(IMUEventCallback callback) = NULL;
void (*RegisterStateEventCallback)(StateEventCallback callback) = NULL;
void (*UnregisterStateEventCallback)(StateEventCallback callback) = NULL;
int (*EstablishUsbConnection)(int32_t vid, int32_t pid) = NULL;
int (*ResetUsbConnection)() = NULL;
void (*NotifyDeviceConnected)() = NULL;
void (*NotifyDeviceDisconnected)() = NULL;
void (*StartXR)() = NULL;
void (*StopXR)() = NULL;
void (*SwitchTo2D)() = NULL;
void (*SwitchTo3D)() = NULL;
void (*OpenIMU)() = NULL;
void (*CloseIMU)() = NULL;
void (*Recenter)() = NULL;
void (*GetHeadTrackerPose)(float rotation[4], float position[3], uint64_t* timeNsInDevice) = NULL;
uint64_t (*ConvertHostTime2DeviceTime)(uint64_t timeNsInHost) = NULL;
void (*GetDeviceType)(char* device) = NULL;
void (*AcquireDeviceInfo)() = NULL;
int8_t (*GetSideBySideStatus)() = NULL;

static void* sdk_handle = NULL;
static bool sdk_loaded = false;
static bool sdk_load_attempted = false;
static pthread_mutex_t sdk_load_mutex = PTHREAD_MUTEX_INITIALIZER;

// see include/sdks/rayneo.h for why this is dlopen'd rather than linked. Idempotent, thread-safe;
// on failure the entry points stay NULL and the device is reported unsupported.
bool rayneo_sdk_load(void) {
    pthread_mutex_lock(&sdk_load_mutex);
    if (!sdk_load_attempted) {
        sdk_load_attempted = true;

        sdk_handle = dlopen(RAYNEO_SDK_SONAME, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
        if (sdk_handle == NULL) {
            log_error("RayNeo driver, failed to load " RAYNEO_SDK_SONAME ": %s\n", dlerror());
        } else {
            const char* missing = NULL;

            #define X(name) if (!missing) { *(void**)(&name) = dlsym(sdk_handle, #name); if (!name) missing = #name; }
            RAYNEO_SDK_SYMBOLS
            #undef X

            if (missing == NULL) {
                sdk_loaded = true;
                if (config()->debug_device) log_debug("RayNeo driver, loaded " RAYNEO_SDK_SONAME "\n");
            } else {
                log_error("RayNeo driver, " RAYNEO_SDK_SONAME " is missing symbol %s\n", missing);

                #define X(name) *(void**)(&name) = NULL;
                RAYNEO_SDK_SYMBOLS
                #undef X
            }
        }
    }
    bool loaded = sdk_loaded;
    pthread_mutex_unlock(&sdk_load_mutex);

    return loaded;
}

const device_properties_type rayneo_properties = {
    .brand                              = "",
    .model                              = "",
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

// hardware connection - device is physically plugged in
static bool hard_connected = false;
// software connection - we're actively in communication, holding open a connection
static bool soft_connected = false;

static bool is_sbs_mode = false;

// The GT Max's IMU is mounted rotated about the pitch axis relative to the glasses' body frame, so
// the SDK's orientation has roll and yaw mixed together. Relabel the axes by conjugating with a
// rotation about the pitch (y) axis; this rotates the roll/yaw axes into place while leaving pitch
// untouched.
static imu_quat_type rayneo_imu_frame_y_rotated(imu_quat_type q) {
    const float half_angle = -49.0f * (float) M_PI / 360.0f;
    const float c = cosf(half_angle), s = sinf(half_angle);
    const imu_quat_type p     = { .w = c, .x = 0.0f, .y =  s, .z = 0.0f };
    const imu_quat_type p_inv = { .w = c, .x = 0.0f, .y = -s, .z = 0.0f };
    return multiply_quaternions(multiply_quaternions(p, q), p_inv);
}

void rayneo_imu_callback(const float acc[3], const float gyro[3], const float mag[3], uint64_t timestamp){
    if (!soft_connected || driver_disabled()) return;

    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    float rotation[4];
    float position[3];
    uint64_t time;
    GetHeadTrackerPose(rotation, position, &time);

    imu_quat_type imu_quat = { .w = rotation[3], .x = rotation[0], .y = rotation[1], .z = rotation[2] };
    imu_pose_type pose = (imu_pose_type){0};
    switch (active_imu_frame) {
        case RAYNEO_IMU_FRAME_Y_ROTATED:
            pose.orientation = rayneo_imu_frame_y_rotated(imu_quat);
            break;
        case RAYNEO_IMU_FRAME_EUS:
        default:
            pose.orientation = quaternion_eus_to_nwu(imu_quat);
            break;
    }
    pose.has_orientation = true;
    pose.timestamp_ms = ts;
    connection_pool_ingest_pose(RAYNEO_DRIVER_ID, pose);
}

static pthread_mutex_t device_name_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t device_name_cond = PTHREAD_COND_INITIALIZER;
static char* device_brand = NULL;
static char* device_model = NULL;
static void rayneo_mcu_callback(uint32_t state, uint64_t timestamp, size_t length, const void* data) {
    uint32_t ts = (uint32_t) (timestamp / TS_TO_MS_FACTOR);
    if (!soft_connected) return;
    if (state == STATE_EVENT_DEVICE_INFO) {
        pthread_mutex_lock(&device_name_mutex);
        if (device_brand == NULL && device_model == NULL) {
            char device_type[64] = {0};
            GetDeviceType(device_type);
            if (config()->debug_device) log_debug("RayNeo driver, received device type: %s\n", device_type);

            bool device_found = false;
            if (strlen(device_type) > 0) {
                char device_type_copy[64];
                strncpy(device_type_copy, device_type, sizeof(device_type_copy) - 1);
                device_type_copy[sizeof(device_type_copy) - 1] = '\0';
                
                char* brand_part = strtok(device_type_copy, " ");
                char* model_part = strtok(NULL, " ");
                if (brand_part && model_part && strlen(brand_part) > 0 && strlen(model_part) > 0) {
                    device_brand = strdup(brand_part);
                    char* version_part = strtok(NULL, " ");
                    if (version_part && strlen(version_part) > 0) {
                        device_model = malloc(strlen(model_part) + strlen(version_part) + 2);
                        if (device_model) {
                            sprintf(device_model, "%s %s", model_part, version_part);
                            device_found = true;
                        }
                    } else {
                        device_model = strdup(model_part);
                        device_found = true;
                    }
                }
            }
            if (device_found) pthread_cond_signal(&device_name_cond);
        }
        pthread_mutex_unlock(&device_name_mutex);

        is_sbs_mode = GetSideBySideStatus() == 1;
    }
}

static pthread_mutex_t device_connection_mutex = PTHREAD_MUTEX_INITIALIZER;
bool rayneo_device_connect() {
    if (!rayneo_sdk_load()) return false;

    pthread_mutex_lock(&device_connection_mutex);
    if (!soft_connected) {
        if (!hard_connected) {
            RegisterIMUEventCallback(rayneo_imu_callback);
            RegisterStateEventCallback(rayneo_mcu_callback);
            if (EstablishUsbConnection(active_device_id_vendor, active_device_id_product) == 0) {
                NotifyDeviceConnected();
                hard_connected = true;
            }
        }
        if (hard_connected) {
            StartXR();
            OpenIMU();
            
            soft_connected = true;

            // this will trigger the STATE_EVENT_DEVICE_INFO event
            AcquireDeviceInfo();
        } else {
            log_message("RayNeo driver, failed to establish a connection\n");
        }
    }
    pthread_mutex_unlock(&device_connection_mutex);

    return soft_connected;
};

void rayneo_device_disconnect(bool soft, bool is_device_present) {
    pthread_mutex_lock(&device_connection_mutex);
    if (soft_connected) {
        CloseIMU();
        StopXR();
        soft_connected = false;
    }

    bool retain_hard_connection = soft && is_device_present;
    if (hard_connected && !retain_hard_connection) {
        NotifyDeviceDisconnected();
        ResetUsbConnection();
        UnregisterIMUEventCallback(rayneo_imu_callback);
        UnregisterStateEventCallback(rayneo_mcu_callback);
        free_and_clear(&device_brand);
        free_and_clear(&device_model);
        hard_connected = false;
    }
    pthread_mutex_unlock(&device_connection_mutex);
};

device_properties_type* rayneo_supported_device(uint16_t vendor_id, uint16_t product_id, uint8_t usb_bus, uint8_t usb_address) {
    for (int i = 0; i < RAYNEO_DEVICE_ID_COUNT; i++) {
        const rayneo_device_id_type* device_id = &rayneo_device_ids[i];
        if (vendor_id != device_id->vendor_id || product_id != device_id->product_id) continue;

        if (!rayneo_sdk_load()) return NULL;

        active_device_id_vendor = vendor_id;
        active_device_id_product = product_id;
        active_imu_frame = device_id->imu_frame;

        device_properties_type* device = calloc(1, sizeof(device_properties_type));
        *device = rayneo_properties;
        device->hid_vendor_id = vendor_id;
        device->hid_product_id = product_id;
        device->fov = device_id->fov;
        device->pitch_adjustment_degrees = device_id->pitch_adjustment_degrees;

        // trying to connect to the device too quickly seems to cause irrecoverable connection issues
        sleep(2);

        // device_connect is actually out-of-turn here, the driver would normally call connect after we return the device 
        // properties, but we kick this off now so we can acquire the device name, which unfortunately comes from the SDK 
        // only after establishing a connection.
        if (rayneo_device_connect()) {
            // The SDK reports the device name asynchronously via a state event. Wait for it, but don't block
            // forever in case it's a device the bundled SDK doesn't recognize; fall back to a static name.
            pthread_mutex_lock(&device_name_mutex);
            if (device_brand == NULL && device_model == NULL) {
                struct timespec deadline;
                clock_gettime(CLOCK_REALTIME, &deadline);
                deadline.tv_sec += RAYNEO_DEVICE_NAME_TIMEOUT_S;
                while (device_brand == NULL && device_model == NULL) {
                    if (pthread_cond_timedwait(&device_name_cond, &device_name_mutex, &deadline) == ETIMEDOUT) break;
                }
            }
            if (device_brand == NULL) device_brand = strdup("RayNeo");
            if (device_model == NULL) device_model = strdup(device_id->model != NULL ? device_id->model : "Unknown");
            // prefer a statically-known model name over the SDK's generic one (e.g. the SDK reports
            // the GT Max as "RayNeo Glass")
            if (device_id->model != NULL) {
                free_and_clear(&device_model);
                device_model = strdup(device_id->model);
            }
            pthread_mutex_unlock(&device_name_mutex);

            device->brand = device_brand;
            device->model = device_model;

            // Leave the connection open if we think it'll be used, but if the driver is disabled, disconnect now
            if (driver_disabled()) rayneo_device_disconnect(true, true);

            return device;
        }

        free(device);
    }

    return NULL;
};

void rayneo_block_on_device() {
    device_properties_type* device = device_checkout();
    bool imu_started = false;
    if (soft_connected && device != NULL) imu_started = wait_for_imu_start();
    while (soft_connected && device != NULL && imu_started && is_imu_alive()) {
        sleep(1);
    }

    rayneo_device_disconnect(true, device != NULL);
    device_checkin(device);
};

bool rayneo_device_is_sbs_mode() {
    return is_sbs_mode;
};

bool rayneo_device_set_sbs_mode(bool enabled) {
    if (!rayneo_sdk_load()) return false;

    // don't explicitly change the is_sbs_mode value here, wait for it to come back around from the MCU deviceinfo response
    if (enabled) {
        SwitchTo3D();
    } else {
        SwitchTo2D();
    }

    return true;
};

bool rayneo_is_connected() {
    return soft_connected;
};

void rayneo_disconnect(bool soft) {
    rayneo_device_disconnect(soft, device_present());
};

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
