#pragma once

#include "imu.h"

#include <stdbool.h>
#include <stdint.h>

void init_multi_tap(int init_imu_cycles_per_s);

int detect_multi_tap(imu_vector_type velocities, uint32_t timestamp, bool debug);