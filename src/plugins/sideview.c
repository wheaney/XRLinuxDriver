#include "config.h"
#include "devices.h"
#include "features/smooth_follow.h"
#include "ipc.h"
#include "logging.h"
#include "plugins.h"
#include "plugins/gamescope_reshade_wayland.h"
#include "plugins/sideview.h"
#include "runtime_context.h"
#include "strings.h"

#include <stdbool.h>
#include <stdlib.h>

const char *sideview_position_names[SIDEVIEW_POSITION_COUNT] = {
    "top_left",
    "top_right",
    "bottom_left",
    "bottom_right",
    "center"
};

sideview_config *sv_config;
sideview_ipc_values_type *sideview_ipc_values;

void *sideview_default_config_func() {
    sideview_config *config = calloc(1, sizeof(sideview_config));
    config->enabled = false;
    config->position = 4; // center
    config->display_size = 1.0;

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
    } else if (equal(key, "sideview_smooth_follow_enabled") && is_smooth_follow_granted()) {
        boolean_config(key, value, &temp_config->smooth_follow_enabled);
    }
};

void sideview_handle_device_disconnect_func() {
    bool enabled = false;
    if (sideview_ipc_values) *sideview_ipc_values->enabled = enabled;
    set_gamescope_reshade_effect_uniform_variable("sideview_enabled", &enabled, 1, sizeof(bool), false);
};

void set_sideview_ipc_values_from_config() {
    if (!sv_config) sv_config = sideview_default_config_func();

    if (device_present()) {
        bool enabled = sv_config->enabled && !config()->disabled;
        float position = (float) sv_config->position;
        if (sideview_ipc_values) {
            *sideview_ipc_values->enabled = enabled && !is_gamescope_reshade_ipc_connected();
            *sideview_ipc_values->position = position;
            *sideview_ipc_values->display_size = sv_config->display_size;
        }
        set_gamescope_reshade_effect_uniform_variable("sideview_enabled", &enabled, 1, sizeof(bool), false);
        set_gamescope_reshade_effect_uniform_variable("sideview_position", &position, 1, sizeof(float), false);
        set_gamescope_reshade_effect_uniform_variable("sideview_display_size", &sv_config->display_size, 1, sizeof(float), false);
    } else {
        sideview_handle_device_disconnect_func();
    }
}

void sideview_set_config_func(void* config) {
    if (!config) return;
    sideview_config* temp_config = (sideview_config*) config;

    if (sv_config) {
        if (sv_config->enabled != temp_config->enabled)
            log_message("Sideview has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        if (sv_config->position != temp_config->position)
            log_message("Sideview position has been changed to %s\n", sideview_position_names[temp_config->position]);

        // smooth follow mode allows sizes > 1, so we need to clamp it if in sideview mode
        if (!temp_config->smooth_follow_enabled && temp_config->display_size > 1.0) temp_config->display_size = 1.0;
        if (sv_config->display_size != temp_config->display_size)
            log_message("Sideview display size has been changed to %f\n", temp_config->display_size);

        free(sv_config);
    }
    sv_config = temp_config;

    set_sideview_ipc_values_from_config();
};

const char *sideview_enabled_name = "sideview_enabled";
const char *sideview_position_name = "sideview_position";
const char *sideview_display_size_name = "sideview_display_size";

bool sideview_setup_ipc_func() {
    bool debug = config()->debug_ipc;
    if (!sideview_ipc_values) sideview_ipc_values = calloc(1, sizeof(sideview_ipc_values_type));
    setup_ipc_value(sideview_enabled_name, (void**) &sideview_ipc_values->enabled, sizeof(bool), debug);
    setup_ipc_value(sideview_position_name, (void**) &sideview_ipc_values->position, sizeof(float), debug);
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
    .handle_device_connect = set_sideview_ipc_values_from_config,
    .handle_device_disconnect = sideview_handle_device_disconnect_func
};