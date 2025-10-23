#pragma once

#include "devices.h"
#include "imu.h"

#include <stdbool.h>
#include <stdint.h>

void driver_handle_pose(imu_pose_type pose);
bool driver_disabled();