#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

extern const char* XDG_STATE_ENV_VAR;
extern const char* XDG_RUNTIME_ENV_VAR;
extern const char* XDG_CONFIG_ENV_VAR;
extern const char* XDG_DATA_ENV_VAR;
extern const char* XDG_STATE_FALLBACK_DIR;
extern const char* XDG_CONFIG_FALLBACK_DIR;
extern const char* XDG_RUNTIME_FALLBACK_DIR;
extern const char* XDG_DATA_FALLBACK_DIR;

char* get_xdg_file_path_for_app(char *app_name, char *filename, const char *xdg_env_var, const char *xdg_fallback_dir);

char* get_state_file_path(char *filename);

char* get_runtime_file_path(char *filename);

char* get_config_file_path(char *filename);

FILE* get_or_create_file(const char *full_path, mode_t directory_mode, char *file_mode, bool *file_created);

FILE* get_or_create_state_file(char *filename, char *mode, char **full_path, bool *created);

FILE* get_or_create_runtime_file(char *filename, char *mode, char **full_path, bool *created);

FILE* get_or_create_config_file(char *filename, char *mode, char **full_path, bool *created);

