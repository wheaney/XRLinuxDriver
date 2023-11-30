#pragma once

#include "imu.h"

#include <stdbool.h>
#include <stdint.h>

void driver_handle_imu_event(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type euler);
bool driver_device_should_disconnect();
bool driver_disabled();