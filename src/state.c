#include "devices.h"
#include "logging.h"
#include "plugins.h"
#include "state.h"
#include "strings.h"
#include "system.h"

#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/time.h>

const char *calibration_setup_strings[2] = {
    "AUTOMATIC",
    "INTERACTIVE"
};
const char *calibration_state_strings[4] = {
    "NOT_CALIBRATED",
    "CALIBRATED",
    "CALIBRATING",
    "WAITING_ON_USER"
};

const char* state_files_directory = "/dev/shm";
const char* state_filename = "xr_driver_state";
const char* control_flags_filename = "xr_driver_control";

FILE* get_driver_state_file(const char *filename, char *mode, char **full_path) {
    int full_path_length = strlen(state_files_directory) + strlen(filename) + 2;
    *full_path = malloc(full_path_length);
    snprintf(*full_path, full_path_length, "%s/%s", state_files_directory, filename);
    return fopen(*full_path, mode ? mode : "r");
}

pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
void write_state(driver_state_type *state) {
    pthread_mutex_lock(&state_mutex);
    char *full_path = NULL;
    FILE* fp = get_driver_state_file(state_filename, "w", &full_path);

    fprintf(fp, "heartbeat=%d\n", state->heartbeat);
    if (get_hardware_id()) fprintf(fp, "hardware_id=%s\n", get_hardware_id());
    if (state->device_license) fprintf(fp, "device_license=%s\n", state->device_license);
    if (state->connected_device_model && state->connected_device_brand) {
        fprintf(fp, "connected_device_brand=%s\n", state->connected_device_brand);
        fprintf(fp, "connected_device_model=%s\n", state->connected_device_model);
        fprintf(fp, "calibration_setup=%s\n", calibration_setup_strings[state->calibration_setup]);
        fprintf(fp, "calibration_state=%s\n", calibration_state_strings[state->calibration_state]);
        fprintf(fp, "sbs_mode_supported=%s\n", state->sbs_mode_supported ? "true" : "false");
        fprintf(fp, "sbs_mode_enabled=%s\n", state->sbs_mode_enabled ? "true" : "false");
        if (state->breezy_desktop_smooth_follow_enabled)
            fprintf(fp, "breezy_desktop_smooth_follow_enabled=true\n");
        fprintf(fp, "firmware_update_recommended=%s\n", state->firmware_update_recommended ? "true" : "false");
    }

    fclose(fp);
    pthread_mutex_unlock(&state_mutex);
}

void read_control_flags(FILE *fp, control_flags_type *flags) {
    flags->recenter_screen = false;
    flags->recalibrate = false;
    flags->force_quit = false;
    flags->sbs_mode = SBS_CONTROL_UNSET;

    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp) != NULL) {
            char *key = strtok(line, "=");
            char *value = strtok(NULL, "\n");
            if (strcmp(key, "recenter_screen") == 0) {
                flags->recenter_screen = strcmp(value, "true") == 0;
            } else if (strcmp(key, "recalibrate") == 0) {
                flags->recalibrate = strcmp(value, "true") == 0;
            } else if (strcmp(key, "sbs_mode") == 0) {
                if (strcmp(value, "unset") == 0) {
                    flags->sbs_mode = SBS_CONTROL_UNSET;
                } else if (strcmp(value, "disable") == 0) {
                    flags->sbs_mode = SBS_CONTROL_DISABLE;
                } else if (strcmp(value, "enable") == 0) {
                    flags->sbs_mode = SBS_CONTROL_ENABLE;
                } else {
                    log_message("Invalid sbs_mode value: %s\n", value);
                }
            } else if (strcmp(key, "force_quit") == 0) {
                flags->force_quit = strcmp(value, "true") == 0;
            }
            plugins.handle_control_flag_line(key, value);
        }
    }
}

void update_state_from_device(driver_state_type *state, device_properties_type *device, device_driver_type *device_driver) {
    pthread_mutex_lock(&state_mutex);

    bool was_sbs_mode_enabled = state->sbs_mode_enabled;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    state->heartbeat = tv.tv_sec;
    state->calibration_setup = CALIBRATION_SETUP_AUTOMATIC;
    state->sbs_mode_supported = false;
    state->sbs_mode_enabled = false;
    state->firmware_update_recommended = false;
    if (device == NULL) {
        // not connected
        free_and_clear(&state->connected_device_brand);
        free_and_clear(&state->connected_device_model);
    } else {
        state->sbs_mode_enabled = false;
        if (device->sbs_mode_supported && device_driver != NULL && device_driver->is_connected_func()) {
            state->sbs_mode_enabled = device_driver->device_is_sbs_mode_func();
        }
        state->firmware_update_recommended = device->firmware_update_recommended;
        if (state->connected_device_brand == NULL || !equal(state->connected_device_brand, device->brand)) {
            free_and_clear(&state->connected_device_brand);
            state->connected_device_brand = strdup(device->brand);
        }
        if (state->connected_device_model == NULL || !equal(state->connected_device_model, device->model)) {
            free_and_clear(&state->connected_device_model);
            state->connected_device_model = strdup(device->model);
        }
        state->calibration_setup = device->calibration_setup;
        state->sbs_mode_supported = device->sbs_mode_supported;
    }

    if (was_sbs_mode_enabled != state->sbs_mode_enabled) {
        if (state->sbs_mode_enabled) {
            log_message("SBS mode has been enabled\n");
        } else {
            log_message("SBS mode has been disabled\n");
        }
    }

    pthread_mutex_unlock(&state_mutex);
}