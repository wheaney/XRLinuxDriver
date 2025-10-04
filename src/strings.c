#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

bool equal(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

const char** split(const char* str, char delimiter, int* size) {
    // Initialize size and handle null/empty input
    if (size) *size = 0;
    if (str == NULL || *str == '\0') return NULL;

    // First pass: count tokens and total chars (with trimming), skipping empty tokens
    int count = 0;
    size_t total_chars = 0; // sum of token lengths including null terminators
    const char *p = str;
    const char *token_start = str;

    while (1) {
        if (*p == delimiter || *p == '\0') {
            // compute trimmed [start, end)
            const char *start = token_start;
            const char *end = p; // one past last
            // trim leading spaces/tabs
            while (start < end && (*start == ' ' || *start == '\t')) start++;
            // trim trailing spaces/tabs
            while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) end--;
            size_t len = (size_t)(end - start);
            if (len > 0) {
                count++;
                total_chars += len + 1; // include null terminator
            }
            if (*p == '\0') break;
            token_start = p + 1;
        }
        p++;
    }

    if (count == 0) return NULL;

    // Allocate one contiguous block: [pointers][strings]
    size_t header_size = (size_t)count * sizeof(char*);
    char **array = (char**) malloc(header_size + total_chars);
    if (array == NULL) return NULL;
    char *storage = (char*)array + header_size;

    // Second pass: fill pointers and copy strings
    int idx = 0;
    p = str;
    token_start = str;
    while (1) {
        if (*p == delimiter || *p == '\0') {
            const char *start = token_start;
            const char *end = p;
            while (start < end && (*start == ' ' || *start == '\t')) start++;
            while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) end--;
            size_t len = (size_t)(end - start);
            if (len > 0) {
                array[idx] = storage;
                memcpy(storage, start, len);
                storage[len] = '\0';
                storage += len + 1;
                idx++;
            }
            if (*p == '\0') break;
            token_start = p + 1;
        }
        p++;
    }

    if (size) *size = idx;
    return (const char**)array;
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