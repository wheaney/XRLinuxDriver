#pragma once

#include "plugins.h"

extern const char *virtual_display_look_ahead_cfg_ipc_name;
extern const char *virtual_display_display_size_ipc_name;
extern const char *virtual_display_display_north_offset_ipc_name;
extern const char *virtual_display_sbs_enabled_ipc_name;
extern const char *virtual_display_sbs_content_ipc_name;
extern const char *virtual_display_sbs_mode_stretched_ipc_name;
extern const char *virtual_display_curved_display_ipc_name;
extern const char *virtual_display_half_fov_z_rads_ipc_name;
extern const char *virtual_display_half_fov_y_rads_ipc_name;
extern const char *virtual_display_fov_half_widths_ipc_name;
extern const char *virtual_display_fov_widths_ipc_name;
extern const char *virtual_display_texcoord_x_limits_ipc_name;
extern const char *virtual_display_texcoord_x_limits_r_ipc_name;
extern const char *virtual_display_lens_vector_ipc_name;
extern const char *virtual_display_lens_vector_r_ipc_name;

struct virtual_display_ipc_values_t {
    bool *enabled;
    bool *show_banner;
    float *look_ahead_cfg;
    float *display_size;
    float *display_north_offset;
    bool *sbs_enabled;
    bool *sbs_content;
    bool *sbs_mode_stretched;
    bool *curved_display;
    float *half_fov_z_rads;
    float *half_fov_y_rads;
    float *fov_half_widths;
    float *fov_widths;
    float *texcoord_x_limits;
    float *texcoord_x_limits_r;
    float *lens_vector;
    float *lens_vector_r;
};
typedef struct virtual_display_ipc_values_t virtual_display_ipc_values_type;

struct virtual_display_config_t {
    bool enabled;
    float look_ahead_override;
    float display_size;
    float display_distance;
    bool sbs_content;
    bool sbs_mode_stretched;
    bool follow_mode_enabled;
    bool passthrough_smooth_follow_enabled;
    bool curved_display;
};
typedef struct virtual_display_config_t virtual_display_config;

extern const plugin_type virtual_display_plugin;