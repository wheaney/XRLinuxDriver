#include "buffer.h"

#include <stdbool.h>
#include <stdlib.h>

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