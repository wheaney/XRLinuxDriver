#pragma once

#include <stdbool.h>
#include "plugins.h"

struct opentrack_listener_config_t {
	bool enabled;
	char *ip;
	int port;
};
typedef struct opentrack_listener_config_t opentrack_listener_config;

extern const plugin_type opentrack_listener_plugin;
