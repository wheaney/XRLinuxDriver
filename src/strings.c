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
        if (equal((char*)str, array[i])) {
            return true;
        }
    }
    return false;
}

const char* concat(const char* path, const char* extension) {
    char* s = malloc((strlen(path) + strlen(extension) + 1) * sizeof(char));
    strcpy(s, path);
    strcat(s, extension);

    return s;
}