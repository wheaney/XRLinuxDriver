#pragma once

#include "devices.h"
#include "imu.h"

#include <stdbool.h>
#include <stdint.h>

void driver_handle_pose_event(uint32_t timestamp_ms, imu_quat_type quat, imu_vec3_type position);
bool driver_disabled();