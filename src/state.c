#include "device.h"
#include "state.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

const char* shared_memory_directory = "/dev/shm";
const char* state_filename = "xr_driver_state";
const char* control_filename = "xr_driver_control";

FILE* get_state_file(const char *filename, char *mode, char *full_path) {
    snprintf(full_path, strlen(shared_memory_directory) + strlen(filename) + 2, "%s/%s", shared_memory_directory, filename);
    return fopen(full_path, mode ? mode : "r");
}

void update_state(driver_state_type *state) {
    char file_path[1024];
    FILE* fp = get_state_file(state_filename, "w", &file_path[0]);

    fprintf(fp, "heartbeat=%d\n", state->heartbeat);
    if (state->connected_device_model && state->connected_device_brand) {
        fprintf(fp, "connected_device_brand=%s\n", state->connected_device_brand);
        fprintf(fp, "connected_device_model=%s\n", state->connected_device_model);
        fprintf(fp, "calibration_setup=%s\n", calibration_setup_strings[state->calibration_setup]);
        fprintf(fp, "calibration_state=%s\n", calibration_state_strings[state->calibration_state]);
        fprintf(fp, "sbs_mode_supported=%s\n", state->sbs_mode_supported ? "true" : "false");
        fprintf(fp, "sbs_mode_enabled=%s\n", state->sbs_mode_enabled ? "true" : "false");
    }

    fclose(fp);
}

void read_control_flags(control_flags_type *flags) {
    flags->recenter_screen = false;
    flags->recalibrate = false;
    flags->sbs_mode = SBS_CONTROL_UNSET;

    char file_path[1024];
    FILE* fp = get_state_file(control_filename, "r", &file_path[0]);
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
                    printf("Invalid sbs_mode value: %s\n", value);
                }
            }
        }
        fclose(fp);
        remove(file_path);
    }
}