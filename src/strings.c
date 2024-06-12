#include <string.h>
#include <stdio.h>
#include <stdbool.h>

void free_and_clear(char **str_ptr) {
    if (*str_ptr) {
        free(*str_ptr);
        *str_ptr = NULL;
    }
}
bool equal(char *a, const char *b) {
    return strcmp(a, b) == 0;
}

bool in_array(const char *str, const char **array, int size) {
    for (int i = 0; i < size; i++) {
        if (equal(str, array[i])) {
            return true;
        }
    }
    return false;
}