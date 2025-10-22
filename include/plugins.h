#pragma once

#include "imu.h"
#include "ipc.h"

#include <stdbool.h>
#include <stdint.h>

// config and control flag parsing/handling functions
typedef void* (*default_config_func)();
typedef void (*handle_config_line_func)(void* config, char* key, char* value);
typedef void (*handle_control_flag_line_func)(char* key, char* value);
typedef void (*set_config_func)(void* config);

// hook functions
typedef int (*register_features_func)(char*** features);
typedef void (*start_func)();

typedef bool (*setup_ipc_func)();
typedef void (*handle_ipc_change_func)();
typedef bool (*modify_reference_pose_func)(imu_pose_type pose, imu_pose_type* ref_pose);
typedef void (*modify_pose_func)(imu_pose_type* pose);
typedef void (*handle_pose_data_func)(imu_pose_type pose, imu_euler_type velocities, bool imu_calibrated, ipc_values_type *ipc_values);
typedef void (*reset_pose_data_func)();
typedef void (*handle_state_func)();
typedef void (*handle_device_connect_func)();
typedef void (*handle_device_disconnect_func)();

struct plugin_t {
    char* id;

    register_features_func register_features;
    start_func start;

    default_config_func default_config;
    handle_config_line_func handle_config_line;
    handle_control_flag_line_func handle_control_flag_line;
    set_config_func set_config;

    setup_ipc_func setup_ipc;
    handle_ipc_change_func handle_ipc_change;
    modify_reference_pose_func modify_reference_pose;
    modify_pose_func modify_pose;
    handle_pose_data_func handle_pose_data;
    reset_pose_data_func reset_pose_data;
    handle_state_func handle_state;
    handle_device_connect_func handle_device_connect;
    handle_device_disconnect_func handle_device_disconnect;

    // TODO handle feature access change
};
typedef struct plugin_t plugin_type;

extern const plugin_type plugins;