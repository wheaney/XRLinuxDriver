#pragma once

#include "plugins.h"

#include <stdbool.h>
#include <stddef.h>

typedef void (*gamescope_reshade_effect_ready_callback)();

bool is_gamescope_reshade_ipc_connected();

void set_gamescope_reshade_effect_uniform_variable(const char *variable_name, const void *data, int entries, size_t size, bool flush);

extern const plugin_type gamescope_reshade_wayland_plugin;