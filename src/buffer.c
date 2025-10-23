#include "buffer.h"
#include "logging.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Ring buffer implementation (moved from buffers.h/buffers.c into buffer.*)
// ---------------------------------------------------------------------------

void ring_buffer_init(ring_buffer_t* rb, int capacity) {
    if (!rb || capacity <= 0) return;
    rb->data = (float*)calloc((size_t)capacity, sizeof(float));
    rb->capacity = rb->data ? capacity : 0;
    rb->size = 0;
    rb->head = 0;
}

void ring_buffer_free(ring_buffer_t* rb) {
    if (!rb) return;
    free(rb->data);
    rb->data = NULL;
    rb->capacity = 0;
    rb->size = 0;
    rb->head = 0;
}

void ring_buffer_push(ring_buffer_t* rb, float value) {
    if (!rb || rb->capacity <= 0) return;
    if (rb->size < rb->capacity) {
        // append at tail
        int tail = (rb->head + rb->size) % rb->capacity;
        rb->data[tail] = value;
        rb->size++;
    } else {
        // overwrite oldest and advance head
        rb->data[rb->head] = value;
        rb->head = (rb->head + 1) % rb->capacity;
    }
}

float ring_buffer_get(const ring_buffer_t* rb, int index) {
    if (!rb || index < 0 || index >= rb->size || rb->capacity <= 0) return 0.0f;
    int pos = (rb->head + index) % rb->capacity;
    return rb->data[pos];
}

void ring_buffer_reset(ring_buffer_t* rb) {
    if (!rb) return;
    rb->size = 0;
    rb->head = 0;
}

buffer_type *create_buffer(int size) {
    buffer_type *buffer = calloc(1, sizeof(buffer_type));
    if (buffer == NULL) {
        return NULL;
    }

    buffer->size = size;
    buffer->values = calloc(size, sizeof(float));
    if (buffer->values == NULL) {
        free(buffer);
        buffer = NULL;
        return NULL;
    }
    buffer->index = 0;
    buffer->count = 0;

    return buffer;
}

bool is_full(buffer_type *buffer) {
    return buffer->count == buffer->size;
}

float push(buffer_type *buffer, float next_value) {
    float popped_value = 0;
    if (is_full(buffer)) {
        popped_value = buffer->values[buffer->index];
    } else {
        buffer->count++;
    }
    buffer->values[buffer->index++] = next_value;
    buffer->index %= buffer->size;

    return popped_value;
}

imu_buffer_type *create_imu_buffer(int buffer_size) {
    imu_buffer_type *gyro_buffer = calloc(1, sizeof(imu_buffer_type));
    if (gyro_buffer == NULL) {
        return NULL;
    }
    gyro_buffer->stage_1 = calloc(GYRO_BUFFERS_COUNT, sizeof(buffer_type*));
    gyro_buffer->stage_2 = calloc(GYRO_BUFFERS_COUNT, sizeof(buffer_type*));
    for (int i = 0; i < GYRO_BUFFERS_COUNT; i++) {
        gyro_buffer->stage_1[i] = create_buffer(buffer_size);
        gyro_buffer->stage_2[i] = create_buffer(buffer_size);
        if (gyro_buffer->stage_1[i] == NULL || gyro_buffer->stage_2[i] == NULL) {
            log_error("Error allocating memory\n");
            return NULL;
        }
    }

    return gyro_buffer;
}

imu_buffer_response_type *push_to_imu_buffer(imu_buffer_type *gyro_buffer, imu_quat_type quat, float timestamp_ms) {
    imu_buffer_response_type *response = calloc(1, sizeof(imu_buffer_response_type));
    if (response == NULL) {
        return NULL;
    }
    response->ready = false;

    // the oldest values are zero/unset if the buffer hasn't been filled yet, so we check prior to doing a
    // push/pop, to know if the values that are returned will be relevant to our calculations
    bool was_full = is_full(gyro_buffer->stage_1[0]);
    float stage_1_quat_w = push(gyro_buffer->stage_1[0], quat.w);
    float stage_1_quat_x = push(gyro_buffer->stage_1[1], quat.x);
    float stage_1_quat_y = push(gyro_buffer->stage_1[2], quat.y);
    float stage_1_quat_z = push(gyro_buffer->stage_1[3], quat.z);

    // TODO - timestamp_ms can only get as large as 2^24 before it starts to lose precision as a float,
    //        which is less than 5 hours of usage. Update this to just send two delta times, t0-t1 and t1-t2.
    float stage_1_ts = push(gyro_buffer->stage_1[4], (float)timestamp_ms);

    if (was_full) {
        was_full = is_full(gyro_buffer->stage_2[0]);
        float stage_2_quat_w = push(gyro_buffer->stage_2[0], stage_1_quat_w);
        float stage_2_quat_x = push(gyro_buffer->stage_2[1], stage_1_quat_x);
        float stage_2_quat_y = push(gyro_buffer->stage_2[2], stage_1_quat_y);
        float stage_2_quat_z = push(gyro_buffer->stage_2[3], stage_1_quat_z);
        float stage_2_ts = push(gyro_buffer->stage_2[4], stage_1_ts);

        if (was_full) {
            response->ready = true;
            response->data = calloc(16, sizeof(float));

            // write to shared memory for anyone using the same ipc prefix to consume
            response->data[0] = quat.x;
            response->data[1] = quat.y;
            response->data[2] = quat.z;
            response->data[3] = quat.w;
            response->data[4] = stage_1_quat_x;
            response->data[5] = stage_1_quat_y;
            response->data[6] = stage_1_quat_z;
            response->data[7] = stage_1_quat_w;
            response->data[8] = stage_2_quat_x;
            response->data[9] = stage_2_quat_y;
            response->data[10] = stage_2_quat_z;
            response->data[11] = stage_2_quat_w;
            response->data[12] = (float)timestamp_ms;
            response->data[13] = stage_1_ts;
            response->data[14] = stage_2_ts;
        }
    }

    return response;
}