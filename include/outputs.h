#pragma once

#include "config.h"
#include "device.h"
#include "device3.h"
#include "ipc.h"

#include <stdbool.h>

void init_outputs(device_properties_type *device, driver_config_type *config);

void deinit_outputs(driver_config_type *config);

device3_vec3_type get_euler_deltas(device3_vec3_type euler);

device3_vec3_type get_euler_velocities(device_properties_type *device, device3_vec3_type euler_deltas);

void handle_imu_update(device3_quat_type quat, device3_vec3_type euler_deltas, device3_quat_type screen_center,
                       bool ipc_enabled, bool send_imu_data, ipc_values_type *ipc_values, device_properties_type *device,
                       driver_config_type *config);

void reset_imu_data(ipc_values_type *ipc_values);