#include <string.h>
#include <stdio.h>

void copy_string(const char *source, char **destination) {
    if (*destination != NULL) free_and_clear(destination);
    *destination = malloc(strlen(source) + 1);
    strcpy(*destination, source);
}

void free_and_clear(char **str_ptr) {
    if (*str_ptr) {
        free(*str_ptr);
        *str_ptr = NULL;
    }
}