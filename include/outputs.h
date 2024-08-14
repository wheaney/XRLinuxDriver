#pragma once

#include "imu.h"
#include "ipc.h"

#include <stdbool.h>
#include <stdint.h>

#define MS_PER_SEC 1000
#define IMU_CHECKPOINT_MS MS_PER_SEC / 4

void init_outputs();

void deinit_outputs();

void reinit_outputs();

// return the rate-of-change of the euler value against the previous euler value, in degrees/sec
imu_euler_type get_euler_velocities(imu_euler_type euler, int imu_cycles_per_sec);

void handle_imu_update(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                       bool imu_calibrated, ipc_values_type *ipc_values);
void reset_imu_data(ipc_values_type *ipc_values);

bool wait_for_imu_start();
bool is_imu_alive();