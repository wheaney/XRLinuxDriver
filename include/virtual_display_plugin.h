#pragma once

#include "plugins.h"

extern const char *virtual_display_imu_data_ipc_name;
extern const char *virtual_display_imu_data_mutex_ipc_name;
extern const char *virtual_display_imu_data_period_name;
extern const char *virtual_display_look_ahead_cfg_ipc_name;
extern const char *virtual_display_display_zoom_ipc_name;
extern const char *virtual_display_display_north_offset_ipc_name;
extern const char *virtual_display_sbs_enabled_name;
extern const char *virtual_display_sbs_content_name;
extern const char *virtual_display_sbs_mode_stretched_name;

struct virtual_display_ipc_values_t {
    bool *enabled;
    float *imu_data;
    pthread_mutex_t *imu_data_mutex;
    float *imu_data_period;
    float *look_ahead_cfg;
    float *display_zoom;
    float *display_north_offset;
    bool *sbs_enabled;
    bool *sbs_content;
    bool *sbs_mode_stretched;
};
typedef struct virtual_display_ipc_values_t virtual_display_ipc_values_type;

struct virtual_display_config_t {
    bool enabled;
    float look_ahead_override;
    float display_zoom;
    float display_distance;
    float sbs_display_size;
    bool sbs_content;
    bool sbs_mode_stretched;
};
typedef struct virtual_display_config_t virtual_display_config;

extern const plugin_type virtual_display_plugin;