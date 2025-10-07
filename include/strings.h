#pragma once

#include <stdlib.h>
#include <stdbool.h>

bool equal(const char *a, const char *b);

bool in_array(const char *str, const char **array, int size);

bool list_string_contains(const char* str, const char* list_string);

const char* concat(const char* path, const char* extension);

// Comparison function intended for qsort
int compare_strings(const void* a, const void* b);