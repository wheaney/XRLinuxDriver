#pragma once

#include <stdlib.h>

#define free_and_clear(ptr) do { \
	if (ptr != NULL && *ptr != NULL) { \
		free(*ptr); \
		*ptr = NULL; \
	} \
} while(0)

// Free an array of strings
static inline void free_string_array(char** array, int count) {
    if (!array) return;
    for (int i = 0; i < count; i++) {
        free(array[i]);
    }
    free(array);
}