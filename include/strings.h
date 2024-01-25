#pragma once

#include <stdlib.h>
#include <stdbool.h>

void free_and_clear(char **str_ptr);

bool in_array(char *str, char **array, int size);