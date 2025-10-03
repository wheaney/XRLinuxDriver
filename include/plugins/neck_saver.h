#pragma once
#include "plugins.h"

typedef struct neck_saver_config_t {
    float horizontal_multiplier; // applied to yaw (rotation about up axis)
    float vertical_multiplier;   // applied to pitch (rotation about east/west axis)
} neck_saver_config_type;

extern const plugin_type neck_saver_plugin;
