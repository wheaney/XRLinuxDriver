#pragma once

#include "config.h"
#include "devices.h"
#include "state.h"

struct runtime_context_t {
    // user-controlled, read-only driver configurations (plugin configs not present), persistent
    driver_config_type *config;

    // properties of the currently connected device, modified only by the device driver module
    device_properties_type *device;

    // live view of the state of the driver, reflects real-world state, not intentions
    driver_state_type *state;
};

typedef struct runtime_context_t runtime_context;

void set_state(driver_state_type *state);

driver_state_type* state();

void set_config(driver_config_type *config);

driver_config_type* config();

// device is so heavily used across threads that it becomes difficult to find a good time to free() it,
// so the below functions keep a reference count and free it when the count reaches 0
//
// if a device is already set, this will queue it and set it after the current device is released.
// once set, it's considered checked out by the setting thread and the reference count is initialized at 1
void set_device_and_checkout(device_properties_type *device);

// returns the current device properties and increments the reference count
device_properties_type* device_checkout();

// decrements the reference count for the current device properties, releasing the device if the count reaches 0
void device_checkin(device_properties_type* device);

bool device_present();

typedef void (*on_device_change_callback)();

void set_on_device_change_callback(on_device_change_callback callback);