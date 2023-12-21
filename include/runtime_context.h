#pragma once

#include "config.h"
#include "device.h"
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

// only the main thread should modify the attributes of this
// everyone else should only read from it
extern runtime_context context;
