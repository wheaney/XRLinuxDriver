#pragma once

#include <imu.h>
#include <stdbool.h>

#define GYRO_BUFFERS_COUNT 5 // quat values: x, y, z, w, timestamp

// ---------------------------------------------------------------------------
// Simple float ring buffer used by signal processing utilities
// ---------------------------------------------------------------------------
typedef struct ring_buffer_t {
    float* data;
    int capacity;
    int size;   // number of valid samples currently stored (<= capacity)
    int head;   // index of the oldest element
} ring_buffer_t;

// Initialize a ring buffer with given capacity (in elements)
void ring_buffer_init(ring_buffer_t* rb, int capacity);

// Free memory allocated for the ring buffer
void ring_buffer_free(ring_buffer_t* rb);

// Push a value into the ring buffer, overwriting the oldest when full
void ring_buffer_push(ring_buffer_t* rb, float value);

// Get the i-th element from the oldest (0-based); undefined if i >= size
float ring_buffer_get(const ring_buffer_t* rb, int index);

// Reset the ring buffer to empty state without freeing memory
void ring_buffer_reset(ring_buffer_t* rb);

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