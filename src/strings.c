#include <string.h>
#include <stdio.h>
#include <stdbool.h>

bool equal(const char *a, const char *b) {
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

const char* concat(const char* path, const char* extension) {
    char* s = malloc((strlen(path) + strlen(extension) + 1) * sizeof(char));
    strcpy(s, path);
    strcat(s, extension);

    return s;
}

// Comparison function intended for qsort
int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}