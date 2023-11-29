#include <string.h>

void copy_string(const char *source, char **destination, size_t size) {
    if (*destination != NULL) free_and_clear(destination);
    *destination = malloc(size);
    strcpy(*destination, source);
}

void free_and_clear(char **str_ptr) {
    free(*str_ptr);
    *str_ptr = NULL;
}