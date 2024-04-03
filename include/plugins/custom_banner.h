#pragma once

#include "plugins.h"

struct custom_banner_ipc_values_t {
    bool *enabled;
};
typedef struct custom_banner_ipc_values_t custom_banner_ipc_values_type;

extern custom_banner_ipc_values_type *custom_banner_ipc_values;
extern const plugin_type custom_banner_plugin;