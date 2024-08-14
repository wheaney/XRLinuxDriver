#include "logging.h"
#include "plugins.h"
#include "plugins/custom_banner.h"
#include "plugins/breezy_desktop.h"
#include "plugins/device_license.h"
#include "plugins/metrics.h"
#include "plugins/sideview.h"
#include "plugins/smooth_follow.h"
#include "plugins/virtual_display.h"
#include "state.h"

#include <stdlib.h>

#define PLUGIN_COUNT 7
const plugin_type* all_plugins[PLUGIN_COUNT] = {
    &device_license_plugin,
    &virtual_display_plugin,
    &sideview_plugin,
    &metrics_plugin,
    &custom_banner_plugin,
    &smooth_follow_plugin,
    &breezy_desktop_plugin
};


void all_plugins_start_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->start == NULL) continue;
        all_plugins[i]->start();
    }
}
int all_plugins_register_features_func(char*** features) {
    int feature_count = 0;
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->register_features == NULL) continue;

        char** plugin_features = NULL;
        int plugin_features_count = all_plugins[i]->register_features(&plugin_features);

        // append plugin_features
        *features = realloc(*features, sizeof(char*) * (feature_count + plugin_features_count));
        for (int j = 0; j < plugin_features_count; j++) {
            (*features)[feature_count + j] = plugin_features[j];
        }
        free(plugin_features);
        feature_count += plugin_features_count;
    }

    return feature_count;
}
void* all_plugins_default_config_func() {
    void** configs = calloc(PLUGIN_COUNT, sizeof(void*));
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
void all_plugins_handle_control_flag_line_func(char* key, char* value) {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_control_flag_line == NULL) continue;
        all_plugins[i]->handle_control_flag_line(key, value);
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
            log_error("Failed to setup IPC for plugin %s\n", all_plugins[i]->id);
            exit(1);
        }
    }

    return true;
}
imu_quat_type all_plugins_modify_screen_center_func(uint32_t timestamp_ms, imu_quat_type quat, imu_quat_type screen_center) {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->modify_screen_center == NULL) continue;
        screen_center = all_plugins[i]->modify_screen_center(timestamp_ms, quat, screen_center);
    }

    return screen_center;
}
void all_plugins_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                      bool imu_calibrated, ipc_values_type *ipc_values) {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_imu_data == NULL) continue;
        all_plugins[i]->handle_imu_data(timestamp_ms, quat, velocities, imu_calibrated, ipc_values);
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
void all_plugins_handle_device_connect_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_device_connect == NULL) continue;
        all_plugins[i]->handle_device_connect();
    }
}
void all_plugins_handle_device_disconnect_func() {
    for (int i = 0; i < PLUGIN_COUNT; i++) {
        if (all_plugins[i]->handle_device_disconnect == NULL) continue;
        all_plugins[i]->handle_device_disconnect();
    }
}

const plugin_type plugins = {
    .id = "all_plugins",
    .start = all_plugins_start_func,
    .register_features = all_plugins_register_features_func,
    .default_config = all_plugins_default_config_func,
    .handle_config_line = all_plugins_handle_config_line_func,
    .handle_control_flag_line = all_plugins_handle_control_flag_line_func,
    .set_config = all_plugins_set_config_func,
    .setup_ipc = all_plugins_setup_ipc_func,
    .modify_screen_center = all_plugins_modify_screen_center_func,
    .handle_imu_data = all_plugins_handle_imu_data_func,
    .reset_imu_data = all_plugins_reset_imu_data_func,
    .handle_state = all_plugins_handle_state_func,
    .handle_device_connect = all_plugins_handle_device_connect_func,
    .handle_device_disconnect = all_plugins_handle_device_disconnect_func
};