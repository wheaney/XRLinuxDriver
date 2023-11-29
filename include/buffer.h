#pragma once

#include <stdbool.h>

struct buffer_t {
    int size;
    float* values;
    int index;
    int count;
};

typedef struct buffer_t buffer_type;

buffer_type *create_buffer(int size);

bool is_full(buffer_type *buffer);

// push a new value, pop the oldest value and return it
float push(buffer_type *buffer, float next_value);