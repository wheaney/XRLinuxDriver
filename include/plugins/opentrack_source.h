#pragma once

#include <stdbool.h>
#include "plugins.h"

struct opentrack_source_config_t {
    bool enabled;
    char *ip;
    int port;
};
typedef struct opentrack_source_config_t opentrack_source_config;

extern const plugin_type opentrack_source_plugin;
