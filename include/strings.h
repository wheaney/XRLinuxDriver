#pragma once

#include <stdlib.h>
#include <stdbool.h>

void free_and_clear(char **str_ptr);

bool equal(char *a, const char *b);

bool in_array(const char *str, const char **array, int size);

const char* concat(const char* path, const char* extension);
