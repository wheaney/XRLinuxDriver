#pragma once

#include "imu.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*imu_event_handler)(uint32_t timestamp_ms, imu_quat_type quat, imu_vector_type euler);
typedef bool (*should_disconnect_callback)();