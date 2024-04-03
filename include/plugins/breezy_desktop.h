#pragma once

#include "plugins.h"

struct breezy_desktop_config_t {
    bool enabled;
    float look_ahead_override;
    float display_zoom;
    float sbs_display_distance;
    float sbs_display_size;
    bool sbs_content;
    bool sbs_mode_stretched;
};
typedef struct breezy_desktop_config_t breezy_desktop_config;

extern const plugin_type breezy_desktop_plugin;