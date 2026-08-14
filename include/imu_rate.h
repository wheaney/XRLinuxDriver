#pragma once

#include "devices.h"

#include <stdbool.h>

void imu_rate_reset();

// measures the rate poses are actually arriving at, updating the device's rate properties once a
// deviating rate has held steady, returns true if the device was updated
bool imu_rate_observe_pose(device_properties_type* device);
