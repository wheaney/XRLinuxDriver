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

// Deep copy an array of strings
char** deep_copy_string_array(char** array, int count) {
    if (count == 0 || !array) return NULL;
    
    char** copy = calloc(count, sizeof(char*));
    if (!copy) return NULL;
    
    int i;
    for (i = 0; i < count; i++) {
        if (!array[i]) {
            goto cleanup_and_fail;
        }
        copy[i] = strdup(array[i]);
        if (!copy[i]) {
            goto cleanup_and_fail;
        }
    }
    return copy;

cleanup_and_fail:
    for (int j = 0; j < i; j++) {
        free(copy[j]);
    }
    free(copy);
    return NULL;
}

// Parse comma-separated string into array of strings
int parse_comma_separated_string(const char* str, char*** result) {
    if (!str || strlen(str) == 0) {
        *result = NULL;
        return 0;
    }

    char* str_copy = strdup(str);
    if (!str_copy) {
        *result = NULL;
        return 0;
    }
    
    int count = 0;
    *result = NULL;

    char* token = strtok(str_copy, ",");
    while (token != NULL) {
        // Trim whitespace
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') end--;
        *(end + 1) = '\0';

        if (strlen(token) > 0) {
            char** temp = realloc(*result, (count + 1) * sizeof(char*));
            if (!temp) {
                // Clean up on allocation failure
                for (int i = 0; i < count; i++) {
                    free((*result)[i]);
                }
                free(*result);
                free(str_copy);
                *result = NULL;
                return 0;
            }
            *result = temp;
            (*result)[count] = strdup(token);
            if (!(*result)[count]) {
                // Clean up on strdup failure
                for (int i = 0; i < count; i++) {
                    free((*result)[i]);
                }
                free(*result);
                free(str_copy);
                *result = NULL;
                return 0;
            }
            count++;
        }
        token = strtok(NULL, ",");
    }

    free(str_copy);
    return count;
}