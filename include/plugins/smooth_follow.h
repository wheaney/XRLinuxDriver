#pragma once

#include "plugins.h"

#include <stdint.h>

struct smooth_follow_ipc_values_t {
    bool *enabled;
};
typedef struct smooth_follow_ipc_values_t smooth_follow_ipc_values_type;

struct smooth_follow_config_t {
    bool virtual_display_enabled;
    bool virtual_display_follow_enabled;
    bool sideview_enabled;
    bool sideview_follow_enabled;
    bool breezy_desktop_enabled;
    float virtual_display_size;
};
typedef struct smooth_follow_config_t smooth_follow_config;

struct smooth_follow_params_t {
    // The distance threshold at which a time-delayed trigger will begin counting down
    float lower_angle_threshold;
    uint32_t delay_ms;

    // The distance threshold at which movement is immediately triggered
    float upper_angle_threshold;

    // The angle to slerp to once a threshold is triggered
    float return_to_angle;

    // value is compounding, so 1.0 - pow(1.0 - interpolation_ratio_ms, 1000) is the effective ratio for a full second,
    // closer to 1.0 means faster acceleration towards the current position
    float interpolation_ratio_ms;
};
typedef struct smooth_follow_params_t smooth_follow_params;

extern const plugin_type smooth_follow_plugin;