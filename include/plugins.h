#pragma once

#include "config.h"
#include "device.h"
#include "imu.h"
#include "ipc.h"

#include <stdbool.h>

// config parsing/handling functions
typedef void* (*default_config_func)();
typedef void (*handle_config_line_func)(void* config, char* key, char* value);
typedef void (*set_config_func)(driver_config_type* driver_config, void* config);

// hook functions
typedef bool (*setup_ipc_func)(driver_config_type* driver_config, device_properties_type* device, bool debug);
typedef void (*handle_imu_data_func)(imu_quat_type quat, imu_euler_type velocities, imu_quat_type screen_center,
                                     bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values,
                                     device_properties_type *device, driver_config_type *config);
typedef void (*reset_imu_data_func)();

struct plugin_t {
    char* id;

    default_config_func default_config;
    handle_config_line_func handle_config_line;
    set_config_func set_config;

    setup_ipc_func setup_ipc;
    handle_imu_data_func handle_imu_data;
    reset_imu_data_func reset_imu_data;
};
typedef struct plugin_t plugin_type;

extern const plugin_type plugins;