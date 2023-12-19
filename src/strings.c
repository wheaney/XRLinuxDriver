#include <string.h>
#include <stdio.h>

void free_and_clear(char **str_ptr) {
    if (*str_ptr) {
        free(*str_ptr);
        *str_ptr = NULL;
    }
}