#pragma once

#include "devices.h"
#include "imu.h"

#include <stdbool.h>
#include <stdint.h>

void driver_handle_imu_event(const char* driver_id, uint32_t timestamp_ms, imu_quat_type quat);
bool driver_disabled();