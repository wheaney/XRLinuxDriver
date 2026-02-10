#pragma once

#include "devices.h" // for calibration_setup_type
#include "imu.h" // for imu_quat_type

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
    float connected_device_full_distance_cm;
    float connected_device_full_size_cm;
    bool connected_device_pose_has_position;
    calibration_setup_type calibration_setup;
    calibration_state_type calibration_state;
    bool sbs_mode_supported;
    bool sbs_mode_enabled;
    bool breezy_desktop_smooth_follow_enabled;
    float breezy_desktop_follow_threshold;
    float *smooth_follow_origin;
    bool smooth_follow_origin_ready;
    float breezy_desktop_display_distance;
    bool firmware_update_recommended;
    bool is_gamescope_reshade_ipc_connected;

    int granted_features_count;
    char** granted_features;

    int license_features_count;
    char** license_features;

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
    bool force_quit;
    sbs_control_type sbs_mode;
    char* request_features;
};
typedef struct control_flags_t control_flags_type;

extern const char* state_files_directory;
extern const char* state_filename;
extern const char* control_flags_filename;

FILE* get_driver_state_file(const char *filename, char *mode, char **full_path);
void write_state(driver_state_type *state);
void read_control_flags(FILE *fp, control_flags_type *flags);
void update_state_from_device(driver_state_type *state, device_properties_type *primary_device, device_properties_type *supplemental_device, device_driver_type *device_driver);