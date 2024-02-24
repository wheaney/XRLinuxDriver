#include "device.h"
#include "plugins.h"
#include "state.h"
#include "system.h"

#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>

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

FILE* get_state_file(const char *filename, char *mode, char *full_path) {
    snprintf(full_path, strlen(state_files_directory) + strlen(filename) + 2, "%s/%s", state_files_directory, filename);
    return fopen(full_path, mode ? mode : "r");
}

void write_state(driver_state_type *state) {
    char file_path[1024];
    FILE* fp = get_state_file(state_filename, "w", &file_path[0]);

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
        fprintf(fp, "firmware_update_recommended=%s\n", state->firmware_update_recommended ? "true" : "false");
    }

    fclose(fp);
}

void read_control_flags(FILE *fp, control_flags_type *flags) {
    flags->recenter_screen = false;
    flags->recalibrate = false;
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
                    printf("Invalid sbs_mode value: %s\n", value);
                }
            }
            plugins.handle_control_flag_line(key, value);
        }
    }
}