#pragma once

#include <imu.h>
#include <stdbool.h>

#define GYRO_BUFFERS_COUNT 5 // quat values: x, y, z, w, timestamp

struct buffer_t {
    int size;
    float* values;
    int index;
    int count;
};

typedef struct buffer_t buffer_type;

struct imu_buffer_t {
    buffer_type **stage_1;
    buffer_type **stage_2;
};

typedef struct imu_buffer_t imu_buffer_type;

struct imu_buffer_response_t {
    bool ready;
    float *data;
};

typedef struct imu_buffer_response_t imu_buffer_response_type;

buffer_type *create_buffer(int size);

bool is_full(buffer_type *buffer);

// push a new value, pop the oldest value and return it
float push(buffer_type *buffer, float next_value);

imu_buffer_type *create_imu_buffer(int buffer_size);

imu_buffer_response_type *push_to_imu_buffer(imu_buffer_type *gyro_buffer, imu_quat_type quat, float timestamp_ms);