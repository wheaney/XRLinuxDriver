#pragma once

#include "plugins.h"

extern const char *virtual_display_look_ahead_cfg_ipc_name;
extern const char *virtual_display_display_zoom_ipc_name;
extern const char *virtual_display_display_north_offset_ipc_name;
extern const char *virtual_display_sbs_enabled_name;
extern const char *virtual_display_sbs_content_name;
extern const char *virtual_display_sbs_mode_stretched_name;

struct virtual_display_ipc_values_t {
    bool *enabled;
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
    float sbs_display_distance;
    float sbs_display_size;
    bool sbs_content;
    bool sbs_mode_stretched;
    bool follow_mode_enabled;
    bool passthrough_smooth_follow_enabled;
};
typedef struct virtual_display_config_t virtual_display_config;

extern const plugin_type virtual_display_plugin;