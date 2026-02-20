#pragma once

#include "devices.h"
#include "imu.h"

#include <stdbool.h>
#include <stdint.h>


bool driver_reference_pose(imu_pose_type* out_pose, bool* pose_updated);
void driver_handle_pose(imu_pose_type pose);
bool driver_disabled();