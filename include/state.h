#pragma once

#include "device.h" // for calibration_setup_type

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

enum calibration_state_t {
    NOT_CALIBRATED,
    CALIBRATED,
    CALIBRATING,
    WAITING_ON_USER
};
typedef enum calibration_state_t calibration_state_type;

struct driver_state_t {
    uint32_t heartbeat;
    char* connected_device_brand;
    char* connected_device_model;
    calibration_setup_type calibration_setup;
    calibration_state_type calibration_state;
    bool sbs_mode_supported;
    bool sbs_mode_enabled;
    bool firmware_update_recommended;

    int registered_features_count;
    char** registered_features;

    int granted_features_count;
    char** granted_features;

    char* device_license;
};
typedef struct driver_state_t driver_state_type;

enum sbs_control_t {
    SBS_CONTROL_UNSET,
    SBS_CONTROL_ENABLE,
    SBS_CONTROL_DISABLE
};
typedef enum sbs_control_t sbs_control_type;
struct control_flags_t {
    bool recenter_screen;
    bool recalibrate;
    sbs_control_type sbs_mode;
};
typedef struct control_flags_t control_flags_type;

extern const char* state_files_directory;
extern const char* state_filename;
extern const char* control_flags_filename;

FILE* get_state_file(const char *filename, char *mode, char *full_path);
void write_state(driver_state_type *state);
void read_control_flags(FILE *fp, control_flags_type *flags);