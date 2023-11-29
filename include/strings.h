#pragma once

#include <stdlib.h>

void copy_string(const char *source, char **destination, size_t size);

void free_and_clear(char **str_ptr);