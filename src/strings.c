#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

bool equal(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

bool list_string_contains(const char* str, const char* list_string) {
    if (str == NULL || list_string == NULL) return false;

    const char* p = list_string;
    size_t len = strlen(str);

    while (*p) {
        // Find the end of the current token
        const char* start = p;
        while (*p && *p != ',') p++;
        size_t token_len = (size_t)(p - start);
        // Compare lengths and content
        if (token_len == len && strncmp(start, str, len) == 0) {
            return true;
        }
        // Move to the next token if not at the end
        if (*p == ',') p++;
    }

    return false;
}

bool in_array(const char *str, const char **array, int size) {
    if (array == NULL || str == NULL) return false;

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