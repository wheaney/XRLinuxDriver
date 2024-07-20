#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

char* get_state_file_path(char *filename);

char* get_runtime_file_path(char *filename);

char* get_config_file_path(char *filename);

FILE* get_or_create_file(const char *full_path, mode_t directory_mode, char *file_mode, bool *file_created);

FILE* get_or_create_state_file(char *filename, char *mode, char **full_path, bool *created);

FILE* get_or_create_runtime_file(char *filename, char *mode, char **full_path, bool *created);

FILE* get_or_create_config_file(char *filename, char *mode, char **full_path, bool *created);

