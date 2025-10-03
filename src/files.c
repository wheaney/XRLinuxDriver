#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

const char* XR_DRIVER_DIR = "xr_driver";
const char* XDG_STATE_ENV_VAR = "XDG_STATE_HOME";
const char* XDG_RUNTIME_ENV_VAR = "XDG_RUNTIME_DIR";
const char* XDG_CONFIG_ENV_VAR = "XDG_CONFIG_HOME";
const char* XDG_DATA_ENV_VAR = "XDG_DATA_HOME";
const char* XDG_STATE_FALLBACK_DIR = "/.local/state";
const char* XDG_CONFIG_FALLBACK_DIR = "/.config";
const char* XDG_RUNTIME_FALLBACK_DIR = "/tmp";
const char* XDG_DATA_FALLBACK_DIR = "/.local/share";

// TODO - this uses the parent directories to determine the ownership of the new directory, which can be removed
// when the driver is no longer running as root
FILE* get_or_create_file(const char *full_path, mode_t directory_mode, const char *file_mode, bool *file_created) {
    FILE *fp = fopen(full_path, file_mode ? file_mode : "r");
    if (fp == NULL) {
        char *copypath = strdup(full_path);
        char *pp = copypath;
        char *sp;
        int status = 0;
        struct stat st;

        while (status == 0 && (sp = strchr(pp, '/')) != 0) {
            if (sp != pp) {
                *sp = '\0';
                if (stat(copypath, &st) != 0) {
                    if (mkdir(copypath, directory_mode) != 0) {
                        if (errno != EEXIST) {
                            status = -1;
                            break;
                        }
                    }
                    else {
                        char *parent = strdup(copypath);
                        char *last_slash = strrchr(parent, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                            if (stat(parent, &st) == 0) {
                                if (chown(copypath, st.st_uid, st.st_gid) == -1) {
                                    perror("Error setting directory ownership");
                                }
                            }
                        }
                        free(parent);
                    }
                }
                *sp = '/';
            }
            pp = sp + 1;
        }

        if (status == 0) {
            char *parent = strdup(full_path);
            char *last_slash = strrchr(parent, '/');
            if (last_slash) {
                *last_slash = '\0';
                if (stat(parent, &st) == 0) {
                    fp = fopen(full_path, "w");
                    if (file_created != NULL)
                        *file_created = true;

                    if (chmod(full_path, directory_mode) == -1) {
                        perror("Error setting file permissions");
                    }
                    if (chown(full_path, st.st_uid, st.st_gid) == -1) {
                        perror("Error setting directory ownership");
                    }
                }
            }
            free(parent);
        }

        free(copypath);
    } else if (file_created != NULL) {
        *file_created = false;
    }

    return fp;
}

char* get_xdg_file_path_for_app(const char *app_name, const char *filename, const char *xdg_env_var, const char *xdg_fallback_dir) {
    struct stat st = {0};

    char* base_directory = getenv(xdg_env_var);

    if (base_directory == NULL) {
        char* home = getenv("HOME");
        base_directory = (char*)concat(home, xdg_fallback_dir);
    }

    int path_length = strlen(base_directory) + strlen(app_name) + strlen(filename) + 3;
    char *full_path = (char*)malloc(path_length * sizeof(char));
    snprintf(full_path, path_length, "%s/%s/%s", base_directory, app_name, filename);

    return full_path;
}

char* get_xdg_file_path(const char *filename, const char *xdg_env_var, const char *xdg_fallback_dir) {
    return get_xdg_file_path_for_app(XR_DRIVER_DIR, filename, xdg_env_var, xdg_fallback_dir);
}

char* get_state_file_path(const char *filename) {
    return get_xdg_file_path(filename, XDG_STATE_ENV_VAR, XDG_STATE_FALLBACK_DIR);
}

char* get_runtime_file_path(const char *filename) {
    return get_xdg_file_path(filename, XDG_RUNTIME_ENV_VAR, XDG_RUNTIME_FALLBACK_DIR);
}

char* get_config_file_path(const char *filename) {
    return get_xdg_file_path(filename, XDG_CONFIG_ENV_VAR, XDG_CONFIG_FALLBACK_DIR);
}

FILE* get_or_create_state_file(const char *filename, const char *mode, char **full_path, bool *created) {
    *full_path = get_state_file_path(filename);
    return get_or_create_file(*full_path, 0777, mode, created);
}

FILE* get_or_create_runtime_file(const char *filename, const char *mode, char **full_path, bool *created) {
    *full_path = get_runtime_file_path(filename);
    return get_or_create_file(*full_path, 0700, mode, created);
}

FILE* get_or_create_config_file(const char *filename, const char *mode, char **full_path, bool *created) {
    *full_path = get_config_file_path(filename);
    return get_or_create_file(*full_path, 0777, mode, created);
}