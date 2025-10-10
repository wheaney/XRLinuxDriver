#include "connection_pool.h"
#include "devices.h"
#include "devices/rayneo.h"
#include "devices/rokid.h"
#include "devices/viture.h"
#include "devices/xreal.h"
#include "logging.h"
#include "runtime_context.h"

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#if defined(__aarch64__)
    #define DEVICE_DRIVER_COUNT 2
    const device_driver_type* device_drivers[DEVICE_DRIVER_COUNT] = {
        &xreal_driver,
        &viture_driver
    };
#elif defined(__x86_64__)
    #define DEVICE_DRIVER_COUNT 4
    const device_driver_type* device_drivers[DEVICE_DRIVER_COUNT] = {
        &rayneo_driver,
        &rokid_driver,
        &xreal_driver,
        &viture_driver
    };
#else
    #error "Unsupported architecture"
#endif

static connected_device_type* _find_connected_device(libusb_device *usb_device, struct libusb_device_descriptor descriptor) {
    for (int j = 0; j < DEVICE_DRIVER_COUNT; j++) {
        const device_driver_type* driver = device_drivers[j];
        device_properties_type* device = driver->supported_device_func(
            descriptor.idVendor, 
            descriptor.idProduct,
            libusb_get_bus_number(usb_device),
            libusb_get_device_address(usb_device)
        );
        if (device != NULL) {
            log_message("Found device with vendor ID 0x%04x and product ID 0x%04x\n", descriptor.idVendor, descriptor.idProduct);
            connected_device_type* connected_device = calloc(1, sizeof(connected_device_type));
            connected_device->driver = driver;
            connected_device->device = device;
            return connected_device;
        }
    }

    return NULL;
}

int hotplug_callback(libusb_context *ctx, libusb_device *usb_device, libusb_hotplug_event event, void *user_data) {
    struct libusb_device_descriptor descriptor;
    int r = libusb_get_device_descriptor(usb_device, &descriptor);
    if (r < 0) {
        log_error("Failed to get device descriptor\n");
        return 1;
    }

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        connection_t* conn = connection_pool_find_hid_connection(descriptor.idVendor, descriptor.idProduct);
        if (conn) {
            connected_device_type* connected_device = calloc(1, sizeof(connected_device_type));
            connected_device->driver = conn->driver;
            connected_device->device = conn->device;
            handle_device_connection_changed(false, connected_device);
        }
    } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        connected_device_type* connected_device = _find_connected_device(usb_device, descriptor);
        if (connected_device != NULL) {
            handle_device_connection_changed(true, connected_device);
        }
    }

    return 0;
}

libusb_context *ctx = NULL;
libusb_hotplug_callback_handle callback_handle;
void init_devices() {
    int r = libusb_init(&ctx);
    if (r < 0) {
        log_error("Failed to initialize libusb\n");
        return;
    }

    r = libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                        LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY,
                                        LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                                        hotplug_callback, NULL, &callback_handle);
    if (r < 0) {
        log_error("Failed to register hotplug callback\n");
    }

    connected_device_type* connected_device = find_connected_device();
    if (connected_device != NULL) {
        handle_device_connection_changed(true, connected_device);
    }
}

void handle_device_connection_events() {
    struct timeval tv = {5, 0};
    libusb_handle_events_timeout_completed(ctx, &tv, NULL);
}

void deinit_devices() {
    if (callback_handle != 0) libusb_hotplug_deregister_callback(ctx, callback_handle);
    libusb_exit(ctx);
}

connected_device_type* find_connected_device() {
    libusb_device **usb_device_list;
    ssize_t usb_device_count = libusb_get_device_list(ctx, &usb_device_list);
    if (usb_device_count < 0) {
        log_error("Failed to get device list\n");
        libusb_exit(ctx);
        return NULL;
    }

    connected_device_type* connected_device = NULL;
    libusb_device *usb_device;
    int i = 0;
    for (i = 0; i < usb_device_count; i++) {
        usb_device = usb_device_list[i];
        struct libusb_device_descriptor descriptor;
        int r = libusb_get_device_descriptor(usb_device, &descriptor);
        if (r < 0) {
            log_error("Failed to get device descriptor\n");
            continue;
        }

        connected_device = _find_connected_device(usb_device, descriptor);
        if (connected_device != NULL) break;
    }

    libusb_free_device_list(usb_device_list, 1);

    return connected_device;
}

bool device_equal(device_properties_type* device, device_properties_type* device2) {
    return device != NULL && device2 != NULL && 
           device->hid_product_id == device2->hid_product_id && 
           device->hid_vendor_id == device2->hid_vendor_id;
}