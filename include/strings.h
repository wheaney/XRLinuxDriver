#pragma once

#include <stdlib.h>
#include <stdbool.h>

void free_and_clear(char **str_ptr);

bool in_array(const char *str, const char **array, int size);