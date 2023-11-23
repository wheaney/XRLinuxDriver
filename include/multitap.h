#pragma once

#include "device3.h"

#include <stdbool.h>

void init_multi_tap(int init_imu_cycles_per_s);

int detect_multi_tap(device3_vec3_type velocities, uint32_t timestamp, bool debug);