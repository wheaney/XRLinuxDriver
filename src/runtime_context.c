#include "devices.h"
#include "runtime_context.h"

#include <pthread.h>
#include <stdlib.h>

static runtime_context context;

void set_state(driver_state_type* state) {
    context.state = state;
}

driver_state_type* state() {
    return context.state;
}

void set_config(driver_config_type* config) {
    context.config = config;
}

driver_config_type* config() {
    return context.config;
}

static int device_ref_count = 0;
pthread_mutex_t device_ref_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static device_properties_type* queued_device = NULL;
static on_device_change_callback on_device_change_callback_func = NULL;

// the mutex must already be locked when calling this function
static bool _check_and_set_queued_device() {
    if (context.device == NULL && queued_device != NULL) {
        context.device = queued_device;
        queued_device = NULL;
        device_ref_count = 1;
        return true;
    }

    return false;
}

static bool device_equal(device_properties_type* device, device_properties_type* device2) {
    return device != NULL && device2 != NULL && 
           device->hid_product_id == device2->hid_product_id && 
           device->hid_vendor_id == device2->hid_vendor_id;
}

void set_device_and_checkout(device_properties_type* device) {
    bool device_changed = false;
    pthread_mutex_lock(&device_ref_count_mutex);
    if (!device_equal(device, context.device)) {
        queued_device = device;
        device_changed = _check_and_set_queued_device();
    } else {
        device_ref_count++;
    }
    pthread_mutex_unlock(&device_ref_count_mutex);

    if (device_changed && on_device_change_callback_func != NULL) on_device_change_callback_func();     
}

device_properties_type* device_checkout() {
    device_properties_type* device = NULL;

    pthread_mutex_lock(&device_ref_count_mutex);
    if (device_present()) {
        device_ref_count++;
        device = context.device;
    }
    pthread_mutex_unlock(&device_ref_count_mutex);

    return device;
}

void device_checkin(device_properties_type* device) {
    bool device_changed = false;
    
    pthread_mutex_lock(&device_ref_count_mutex);
    if (device_ref_count > 0 && device_equal(device, context.device)) {
        device_ref_count--;
        if (device_ref_count == 0) {
            free(context.device);
            context.device = NULL;
            _check_and_set_queued_device();
            device_changed = true;
        }
    } else if (device_equal(device, queued_device)) {
        free(queued_device);
        queued_device = NULL;
    }
    pthread_mutex_unlock(&device_ref_count_mutex);

    if (device_changed && on_device_change_callback_func != NULL) on_device_change_callback_func();
}

// if a device is queued, the current device is already disconnected, return false until queued device takes over
bool device_present() {
    return context.device != NULL && queued_device == NULL;
}

void set_on_device_change_callback(on_device_change_callback callback) {
    on_device_change_callback_func = callback;
}