#pragma once

#include "imu.h"
#include "ipc.h"

#include <stdbool.h>

void init_outputs();

void deinit_outputs();

// return the rate-of-change of the euler value against the previous euler value, in degrees/sec
imu_euler_type get_euler_velocities(imu_euler_type euler);

void handle_imu_update(imu_quat_type quat, imu_euler_type velocities, bool ipc_enabled, bool imu_calibrated,
                       ipc_values_type *ipc_values);