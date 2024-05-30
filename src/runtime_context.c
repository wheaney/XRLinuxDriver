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

void set_device_and_checkout(device_properties_type* device) {
    pthread_mutex_lock(&device_ref_count_mutex);
    queued_device = device;
    bool device_changed = _check_and_set_queued_device();
    pthread_mutex_unlock(&device_ref_count_mutex);

    if (device_changed && on_device_change_callback_func != NULL) on_device_change_callback_func();     
}

device_properties_type* device_checkout() {
    device_properties_type* device = NULL;

    pthread_mutex_lock(&device_ref_count_mutex);
    if (context.device != NULL) {
        device_ref_count++;
        device = context.device;
    }
    pthread_mutex_unlock(&device_ref_count_mutex);

    return device;
}

void device_checkin(device_properties_type* device) {
    // if the returned device is NULL or doesn't match the current, don't decrement the ref count
    bool same_device = device != NULL && context.device != NULL && 
                       device->hid_product_id == context.device->hid_product_id && 
                       device->hid_vendor_id == context.device->hid_vendor_id;
    bool device_changed = false;
    
    pthread_mutex_lock(&device_ref_count_mutex);
    if (device_ref_count > 0 && same_device) {
        device_ref_count--;
        if (device_ref_count == 0) {
            free(context.device);
            context.device = NULL;
            _check_and_set_queued_device();
            device_changed = true;
        }
    }
    pthread_mutex_unlock(&device_ref_count_mutex);

    if (device_changed && on_device_change_callback_func != NULL) on_device_change_callback_func();
}

device_properties_type* device() {
    return context.device;
}

void set_on_device_change_callback(on_device_change_callback callback) {
    on_device_change_callback_func = callback;
}