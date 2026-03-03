#include "devices.h"
#include "runtime_context.h"

#include <pthread.h>
#include <stdlib.h>

runtime_context g_runtime_context;

static int device_ref_count = 0;
pthread_mutex_t device_ref_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static device_properties_type* queued_device = NULL;
static on_device_change_callback on_device_change_callback_func = NULL;

// the mutex must already be locked when calling this function
static bool _check_and_set_queued_device() {
    if (g_runtime_context.device == NULL && queued_device != NULL) {
        g_runtime_context.device = queued_device;
        queued_device = NULL;
        device_ref_count = 1;
        return true;
    }

    return false;
}

void set_device_and_checkout(device_properties_type* device) {
    bool device_changed = false;
    pthread_mutex_lock(&device_ref_count_mutex);
    if (!device_equal(device, g_runtime_context.device)) {
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
        device = g_runtime_context.device;
    }
    pthread_mutex_unlock(&device_ref_count_mutex);

    return device;
}

void device_checkin(device_properties_type* device) {
    bool device_changed = false;
    
    pthread_mutex_lock(&device_ref_count_mutex);
    if (device_ref_count > 0 && device_equal(device, g_runtime_context.device)) {
        device_ref_count--;
        if (device_ref_count == 0) {
            free(g_runtime_context.device);
            g_runtime_context.device = NULL;
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
    return g_runtime_context.device != NULL && queued_device == NULL;
}

void set_on_device_change_callback(on_device_change_callback callback) {
    on_device_change_callback_func = callback;
}