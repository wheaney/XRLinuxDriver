#include <string.h>
#include <stdio.h>
#include <stdbool.h>

void free_and_clear(char **str_ptr) {
    if (*str_ptr) {
        free(*str_ptr);
        *str_ptr = NULL;
    }
}

bool in_array(const char *str, char **array, int size) {
    for (int i = 0; i < size; i++) {
        if (strcmp(str, array[i]) == 0) {
            return true;
        }
    }
    return false;
}