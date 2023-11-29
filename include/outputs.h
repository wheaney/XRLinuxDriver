#pragma once

#include "config.h"
#include "device.h"
#include "imu.h"
#include "ipc.h"

#include <stdbool.h>

void init_outputs(device_properties_type *device, driver_config_type *config);

void deinit_outputs(driver_config_type *config);

imu_vector_type get_euler_deltas(imu_vector_type euler);

imu_vector_type get_euler_velocities(device_properties_type *device, imu_vector_type euler_deltas);

void handle_imu_update(imu_quat_type quat, imu_vector_type euler_deltas, imu_quat_type screen_center,
                       bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values, device_properties_type *device,
                       driver_config_type *config);

void reset_imu_data(ipc_values_type *ipc_values);