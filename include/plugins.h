#pragma once

#include "imu.h"
#include "ipc.h"

#include <stdbool.h>

// config parsing/handling functions
typedef void* (*default_config_func)();
typedef void (*handle_config_line_func)(void* config, char* key, char* value);
typedef void (*set_config_func)(void* config);

// hook functions
typedef bool (*setup_ipc_func)();
typedef void (*handle_imu_data_func)(imu_quat_type quat, imu_euler_type velocities, bool ipc_enabled,
                                     bool imu_calibrated, ipc_values_type *ipc_values);
typedef void (*reset_imu_data_func)();
typedef void (*handle_state_func)();
typedef void (*handle_device_connect_func)();
typedef void (*handle_device_disconnect_func)();

struct plugin_t {
    char* id;

    default_config_func default_config;
    handle_config_line_func handle_config_line;
    set_config_func set_config;

    setup_ipc_func setup_ipc;
    handle_imu_data_func handle_imu_data;
    reset_imu_data_func reset_imu_data;
    handle_state_func handle_state;
    handle_device_connect_func handle_device_connect;
    handle_device_disconnect_func handle_device_disconnect;
};
typedef struct plugin_t plugin_type;

extern const plugin_type plugins;