#include "plugins.h"
#include "sideview_plugin.h"

#include <stdlib.h>

#define PLUGIN_COUNT 1
const plugin_type* all_plugins[PLUGIN_COUNT] = {
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
void all_plugins_handle_config_func(void* config, char* key, char* value) {
    void **configs = (void**)config;
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_config_line == NULL) continue;
        all_plugins[i]->handle_config_line(configs[i], key, value);
    }
}
void all_plugins_set_config_func(driver_config_type* driver_config, void* config) {
    void **configs = (void**)config;
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->set_config == NULL) continue;
        all_plugins[i]->set_config(driver_config, configs[i]);
    }
}
bool all_plugins_setup_ipc_func(driver_config_type* driver_config, device_properties_type* device, bool debug) {
    bool success = true;
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->setup_ipc == NULL) continue;
        success &= all_plugins[i]->setup_ipc(driver_config, device, debug);
    }

    return success;
}
void all_plugins_handle_imu_data_func(imu_quat_type quat, imu_euler_type velocities, imu_quat_type screen_center,
                                     bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values,
                                     device_properties_type *device, driver_config_type *config) {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_imu_data == NULL) continue;
        all_plugins[i]->handle_imu_data(quat, velocities, screen_center, ipc_enabled, imu_calibrated, ipc_values, device, config);
    }
}
void all_plugins_reset_imu_data_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->reset_imu_data == NULL) continue;
        all_plugins[i]->reset_imu_data();
    }
}

const plugin_type plugins = {
    .id = "all_plugins",
    .default_config = all_plugins_default_config_func,
    .handle_config_line = all_plugins_handle_config_func,
    .set_config = all_plugins_set_config_func,
    .setup_ipc = all_plugins_setup_ipc_func,
    .handle_imu_data = all_plugins_handle_imu_data_func,
    .reset_imu_data = all_plugins_reset_imu_data_func
};