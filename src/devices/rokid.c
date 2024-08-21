#include "devices.h"
#include "driver.h"
#include "imu.h"
#include "logging.h"
#include "outputs.h"
#include "runtime_context.h"
#include "sdks/rokid.h"
#include "strings.h"

#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_TO_MS_FACTOR 1000000
#define ROKID_ID_PRODUCT_COUNT 7
const int rokid_supported_id_product[ROKID_ID_PRODUCT_COUNT] = {
    0x162B, 0x162C, 0x162D, 0x162E, 0x162F, 0x2002, 0x2180
};

#define RESOLUTION_2D_3840_1080_60HZ 0
#define RESOLUTION_3D_3840_1080_60HZ 1
#define RESOLUTION_3D_3840_1200_90HZ 4
#define RESOLUTION_3D_3840_1200_60HZ 5

#define RESOLUTION_H_1200P 1200
#define RESOLUTION_H_1080P 1080

// Rokid SDK is returning rotations relative to an east-up-south coordinate system,
// this converts to to north-west-up, and applies a 5-degree offset based on factory device calibration
static const imu_quat_type adjustment_quat = {
    .w = 0.521,
    .x = -0.478,
    .y = 0.478,
    .z = 0.521
};

const device_properties_type rokid_one_properties = {
    .brand                              = "", // replaced by the supported_device() function
    .model                              = "", // replaced by the supported_device() function
    .hid_vendor_id                      = ROKID_GLASS_VID,
    .hid_product_id                     = -1, // replaced by the supported_device() function
    .calibration_setup                  = CALIBRATION_SETUP_AUTOMATIC,
    .resolution_w                       = 1920,
    .resolution_h                       = RESOLUTION_H_1080P,
    .fov                                = 45,
    .lens_distance_ratio                = 0.02,
    .calibration_wait_s                 = 1,
    .imu_cycles_per_s                   = 90,
    .imu_buffer_size                    = 1,
    .look_ahead_constant                = 20.0,
    .look_ahead_frametime_multiplier    = 0.6,
    .look_ahead_scanline_adjust         = 8.0,
    .look_ahead_ms_cap                  = 40.0,
    .sbs_mode_supported                 = true,
    .firmware_update_recommended        = false
};

static void* event_instance = NULL;
static void* event_handle = NULL;
static void* control_instance = NULL;
static bool sbs_mode_enabled = false;

// hardware connection - device is physically plugged in
static bool hard_connected = false;
// software connection - we're actively in communication, holding open a connection
static bool soft_connected = false;

static void cleanup() {
    if (event_instance && event_handle) {
        GlassUnRegisterEvent(event_instance, event_handle);
        event_handle = NULL;
    }

    if (!hard_connected) {
        if (event_instance) {
            GlassEventClose(event_instance);
            event_instance = NULL;
        }
        if (control_instance) {
            GlassControlClose(control_instance);
            control_instance = NULL;
        }
    }
}

void rokid_disconnect(bool soft) {
    soft_connected = false;
    hard_connected = soft;
}

static void handle_display_mode(device_properties_type* device, int display_mode) {
    sbs_mode_enabled = display_mode != RESOLUTION_2D_3840_1080_60HZ;
    if (sbs_mode_enabled) {
        if (display_mode != RESOLUTION_3D_3840_1080_60HZ) {
            device->resolution_h = RESOLUTION_H_1200P;
        } else {
            device->resolution_h = RESOLUTION_H_1080P;
        }
    }
}

static bool device_connect(device_properties_type* device) {
    hard_connected = true;
    if (!event_instance) event_instance = GlassEventInit();
    if (!event_instance) {
        log_error("Failed to initialize event instance\n");
    } else {
        if (!control_instance) control_instance = GlassControlInit();
        if (!control_instance) {
            log_error("Failed to initialize control instance\n");
        } else {
            soft_connected = GlassEventOpen(event_instance, device->hid_vendor_id, device->hid_product_id) && 
                             GlassControlOpen(control_instance, device->hid_vendor_id, device->hid_product_id);

            if (soft_connected) {
                event_handle = GlassRegisterEventWithSize(event_instance, ROTATION_EVENT, 50);
                if (!event_handle) {
                    log_error("Failed to register event handle\n");
                    soft_connected = false;
                } else {
                    GlassAddFusionEvent(event_instance, true);
                    handle_display_mode(device, GetDisplayMode(control_instance));
                }
            }
        }
    }
    
    if (!soft_connected) {
        cleanup();
    }

    return soft_connected;
}

// this will already be connected if the driver is enabled
bool rokid_device_connect() {
    device_properties_type* device = device_checkout();
    if (device != NULL) device_connect(device);
    device_checkin(device);

    return soft_connected;
}

device_properties_type* rokid_supported_device(uint16_t vendor_id, uint16_t product_id, uint8_t usb_bus, uint8_t usb_address) {
    if (vendor_id == ROKID_GLASS_VID) {
        for (int i=0; i < ROKID_ID_PRODUCT_COUNT; i++) {
            if (product_id == rokid_supported_id_product[i]) {
                device_properties_type* device = calloc(1, sizeof(device_properties_type));
                *device = rokid_one_properties;
                device->hid_vendor_id = vendor_id;
                device->hid_product_id = product_id;
                device->usb_bus = usb_bus;
                device->usb_address = usb_address;

                sleep(1);

                if (device_connect(device)) {
                    // split GetProductName result on the first space to separate brand and model
                    char* product_name = GetProductName(control_instance);
                    char* space = strchr(product_name, ' ');
                    char* version_start = strchr(product_name, '(');
                    if (space) {
                        *space = '\0';
                        if (version_start) {
                            *version_start = '\0';
                        }
                        device->model = strdup(space + 1);
                    }
                    device->brand = strdup(product_name);
                    if (driver_disabled()) rokid_disconnect(true);
                    return device;
                }
            }
        }
    }

    return NULL;
};

static int imu_counter = 0;
void rokid_block_on_device() {
    device_properties_type* device = device_checkout();
    if (device != NULL) {
        struct EventData ed;
        while (soft_connected) {
            if (GlassWaitEvent(event_instance, event_handle, &ed, 1000)) {
                struct SensorData sd = ed.acc;
                uint32_t timestamp = (uint32_t) (sd.sensor_timestamp_ns / TS_TO_MS_FACTOR);
                struct RotationData rd = ed.rotation;
                imu_quat_type imu_quat = {
                    .w = rd.Q[3],
                    .x = rd.Q[0],
                    .y = rd.Q[1],
                    .z = rd.Q[2]
                };
                imu_quat_type nwu_quat = multiply_quaternions(imu_quat, adjustment_quat);

                if (++imu_counter % device->imu_cycles_per_s == 0) {
                    imu_counter = 0;

                    // do this every second in the same thread as GlassWaitEvent, it seemed like the
                    // USB interactions between the two calls may not be thread safe
                    handle_display_mode(device, GetDisplayMode(control_instance));
                }

                driver_handle_imu_event(timestamp, nwu_quat);
            } else {
                rokid_disconnect(false);
            }
        }
        cleanup();
    }
    device_checkin(device);
};

bool rokid_device_is_sbs_mode() {
    return sbs_mode_enabled;
};

bool rokid_device_set_sbs_mode(bool enabled) {
    // 3D mode offers 1080p and 1200p options, might as well go with the higher resolution
    sbs_mode_enabled = GlassSetDisplayMode(
        control_instance, enabled ? RESOLUTION_3D_3840_1200_60HZ : RESOLUTION_2D_3840_1080_60HZ);
    return sbs_mode_enabled;
};

bool rokid_is_connected() {
    return soft_connected;
};

const device_driver_type rokid_driver = {
    .supported_device_func              = rokid_supported_device,
    .device_connect_func                = rokid_device_connect,
    .block_on_device_func               = rokid_block_on_device,
    .device_is_sbs_mode_func            = rokid_device_is_sbs_mode,
    .device_set_sbs_mode_func           = rokid_device_set_sbs_mode,
    .is_connected_func                  = rokid_is_connected,
    .disconnect_func                    = rokid_disconnect
};
