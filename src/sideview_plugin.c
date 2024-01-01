#include "config.h"
#include "device.h"
#include "ipc.h"
#include "plugins.h"
#include "runtime_context.h"
#include "sideview_plugin.h"

#include <stdbool.h>
#include <stdlib.h>

const char *sideview_position_names[SIDEVIEW_POSITION_COUNT] = {
    "top_left",
    "top_right",
    "bottom_left",
    "bottom_right",
};

void *sideview_default_config_func() {
    sideview_config *config = malloc(sizeof(sideview_config));
    config->enabled = false;
    config->position = 0;
    config->display_size = 0.5;

    return config;
};

void sideview_handle_config_line_func(void* config, char* key, char* value) {
    sideview_config* temp_config = (sideview_config*) config;
    if (equal(key, "external_mode")) {
        temp_config->enabled = equal(value, "sideview");
    } else if (equal(key, "sideview_position")) {
        for (int i = 0; i < SIDEVIEW_POSITION_COUNT; i++) {
            if (equal(value, sideview_position_names[i])) {
                temp_config->position = i;
                return;
            }
        }
    } else if (equal(key, "sideview_display_size")) {
        float_config(key, value, &temp_config->display_size);
    }
};

sideview_config *sv_config;
sideview_ipc_values_type *sideview_ipc_values;
void set_sideview_ipc_values_from_config() {
    if (!sideview_ipc_values) return;
    if (!sv_config) sv_config = sideview_default_config_func();

    *sideview_ipc_values->enabled = sv_config->enabled && !context.config->disabled;
    *sideview_ipc_values->position = sv_config->position;
    *sideview_ipc_values->display_size = sv_config->display_size;
}

void sideview_set_config_func(void* config) {
    if (!context.config || !config) return;
    sideview_config* temp_config = (sideview_config*) config;

    if (sv_config) {
        if (sv_config->enabled != temp_config->enabled)
            printf("Sideview has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        if (sv_config->position != temp_config->position)
            printf("Sideview position has been changed to %s\n", sideview_position_names[temp_config->position]);

        if (sv_config->display_size != temp_config->display_size)
            printf("Sideview display size has been changed to %f\n", temp_config->display_size);

        free(sv_config);
    }
    sv_config = temp_config;

    set_sideview_ipc_values_from_config();
};

const char *sideview_enabled_name = "sideview_enabled";
const char *sideview_position_name = "sideview_position";
const char *sideview_display_size_name = "sideview_display_size";

bool sideview_setup_ipc_func() {
    bool debug = context.config->debug_ipc;
    if (!sideview_ipc_values) sideview_ipc_values = malloc(sizeof(sideview_ipc_values_type));
    setup_ipc_value(sideview_enabled_name, (void**) &sideview_ipc_values->enabled, sizeof(bool), debug);
    setup_ipc_value(sideview_position_name, (void**) &sideview_ipc_values->position, sizeof(int), debug);
    setup_ipc_value(sideview_display_size_name, (void**) &sideview_ipc_values->display_size, sizeof(float), debug);

    set_sideview_ipc_values_from_config();

    return true;
}

const plugin_type sideview_plugin = {
    .id = "sideview",
    .default_config = sideview_default_config_func,
    .handle_config_line = sideview_handle_config_line_func,
    .set_config = sideview_set_config_func,
    .setup_ipc = sideview_setup_ipc_func,
    .handle_imu_data = NULL,
    .reset_imu_data = NULL,
    .handle_state = NULL
};