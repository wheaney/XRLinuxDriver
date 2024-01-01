#include "virtual_display_plugin.h"
#include "plugins.h"
#include "sideview_plugin.h"
#include "state.h"

#include <stdlib.h>

#define PLUGIN_COUNT 2
const plugin_type* all_plugins[PLUGIN_COUNT] = {
    &virtual_display_plugin,
    &sideview_plugin
};

void* all_plugins_default_config_func() {
    void** configs = malloc(sizeof(void*) * PLUGIN_COUNT);
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->default_config == NULL) continue;
        configs[i] = all_plugins[i]->default_config();
    }

    return configs;
}
void all_plugins_handle_config_line_func(void* config, char* key, char* value) {
    void **configs = (void**)config;
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_config_line == NULL) continue;
        all_plugins[i]->handle_config_line(configs[i], key, value);
    }
}
void all_plugins_set_config_func(void* config) {
    void **configs = (void**)config;
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->set_config == NULL) continue;
        all_plugins[i]->set_config(configs[i]);
    }
}
bool all_plugins_setup_ipc_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->setup_ipc == NULL) continue;
        if (!all_plugins[i]->setup_ipc()) {
            fprintf(stderr, "Failed to setup IPC for plugin %s\n", all_plugins[i]->id);
            exit(1);
        }
    }

    return true;
}
void all_plugins_handle_imu_data_func(imu_quat_type quat, imu_euler_type velocities, bool ipc_enabled,
                                      bool imu_calibrated, ipc_values_type *ipc_values) {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_imu_data == NULL) continue;
        all_plugins[i]->handle_imu_data(quat, velocities, ipc_enabled, imu_calibrated, ipc_values);
    }
}
void all_plugins_reset_imu_data_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->reset_imu_data == NULL) continue;
        all_plugins[i]->reset_imu_data();
    }
}
void all_plugins_handle_state_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_state == NULL) continue;
        all_plugins[i]->handle_state();
    }
}

const plugin_type plugins = {
    .id = "all_plugins",
    .default_config = all_plugins_default_config_func,
    .handle_config_line = all_plugins_handle_config_line_func,
    .set_config = all_plugins_set_config_func,
    .setup_ipc = all_plugins_setup_ipc_func,
    .handle_imu_data = all_plugins_handle_imu_data_func,
    .reset_imu_data = all_plugins_reset_imu_data_func,
    .handle_state = all_plugins_handle_state_func
};