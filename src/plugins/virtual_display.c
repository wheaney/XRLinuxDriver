#include "config.h"
#include "devices.h"
#include "epoch.h"
#include "features/smooth_follow.h"
#include "features/sbs.h"
#include "imu.h"
#include "ipc.h"
#include "logging.h"
#include "plugins.h"
#include "plugins/custom_banner.h"
#include "plugins/gamescope_reshade_wl.h"
#include "plugins/smooth_follow.h"
#include "plugins/virtual_display.h"
#include "runtime_context.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#define GAMESCOPE_RESHADE_WAIT_TIME_MS 500

virtual_display_config *vd_config;
virtual_display_ipc_values_type *virtual_display_ipc_values;
static bool using_gamescope_wayland = false;
static uint64_t gamescope_reshade_effect_request_time = 0;

const int virtual_display_feature_count = 2;

void virtual_display_reset_config(virtual_display_config *config) {
    config->enabled = false;
    config->look_ahead_override = 0.0;
    config->display_zoom = 1.0;
    config->sbs_display_distance = 1.0;
    config->sbs_display_size = 1.0;
    config->sbs_content = false;
    config->sbs_mode_stretched = false;
    config->passthrough_smooth_follow_enabled = false;
    config->follow_mode_enabled = false;
};

void *virtual_display_default_config_func() {
    virtual_display_config *config = calloc(1, sizeof(virtual_display_config));
    virtual_display_reset_config(config);

    return config;
};

void virtual_display_handle_config_line_func(void* config, char* key, char* value) {
    virtual_display_config* temp_config = (virtual_display_config*) config;

    if (equal(key, "external_mode")) {
        temp_config->enabled = equal(value, "virtual_display");
        temp_config->follow_mode_enabled = equal(value, "sideview");
    } else if (equal(key, "look_ahead")) {
        float_config(key, value, &temp_config->look_ahead_override);
    } else if (equal(key, "external_zoom") || equal(key, "display_zoom")) {
        float_config(key, value, &temp_config->display_zoom);
    } else if (equal(key, "sbs_display_distance")) {
        float_config(key, value, &temp_config->sbs_display_distance);
    } else if (equal(key, "sbs_display_size")) {
        float_config(key, value, &temp_config->sbs_display_size);
    } else if (equal(key, "sbs_content")) {
        boolean_config(key, value, &temp_config->sbs_content);
    } else if (equal(key, "sbs_mode_stretched")) {
        boolean_config(key, value, &temp_config->sbs_mode_stretched);
    } else if (equal(key, "sideview_smooth_follow_enabled") && is_smooth_follow_granted()) {
        boolean_config(key, value, &temp_config->passthrough_smooth_follow_enabled);
    }
};

static void wayland_cleanup() {
    bool enabled = false;
    set_gamescope_reshade_effect_uniform_variable("virtual_display_enabled", &enabled, 1, sizeof(bool), true);
    gamescope_reshade_wl_server_disconnect();
    using_gamescope_wayland = false;
    gamescope_reshade_effect_request_time = 0;
}

void virtual_display_handle_device_disconnect_func() {
    if (using_gamescope_wayland) wayland_cleanup();
    if (virtual_display_ipc_values) *virtual_display_ipc_values->enabled = false;
};

void set_virtual_display_ipc_values() {
    if (!vd_config) vd_config = virtual_display_default_config_func();

    device_properties_type* device = device_checkout();
    if (device != NULL) {
        bool enabled = !config()->disabled && 
                            (vd_config->enabled ||
                            vd_config->follow_mode_enabled &&
                            vd_config->passthrough_smooth_follow_enabled);
        float display_zoom = state()->sbs_mode_enabled ? vd_config->sbs_display_size : vd_config->display_zoom;
        float look_ahead_constant = vd_config->enabled ? 
                                        (vd_config->look_ahead_override == 0 ?
                                            device->look_ahead_constant :
                                            vd_config->look_ahead_override) : 
                                        0.0;
        float look_ahead_ftm =  vd_config->enabled ? 
                                    (vd_config->look_ahead_override == 0 ? 
                                        device->look_ahead_frametime_multiplier : 
                                        0.0) :
                                    0.0;
        if (virtual_display_ipc_values) {
            *virtual_display_ipc_values->enabled = enabled && !using_gamescope_wayland;
        }
        if (using_gamescope_wayland) {
            // don't set the "flush" flag here, we'll let the IMU data trigger the flush
            set_gamescope_reshade_effect_uniform_variable("virtual_display_enabled", &enabled, 1, sizeof(bool), false);
            set_gamescope_reshade_effect_uniform_variable("display_zoom", &display_zoom, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("display_north_offset", &vd_config->sbs_display_distance, 1, sizeof(float), false);
            float look_ahead_cfg[4] = {look_ahead_constant, look_ahead_ftm, device->look_ahead_scanline_adjust, device->look_ahead_ms_cap};
            set_gamescope_reshade_effect_uniform_variable("look_ahead_cfg", (void*) look_ahead_cfg, 4, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("sbs_content", &vd_config->sbs_content, 1, sizeof(bool), false);
            set_gamescope_reshade_effect_uniform_variable("sbs_mode_stretched", &vd_config->sbs_mode_stretched, 1, sizeof(bool), false);
        } else if (virtual_display_ipc_values) {
            *virtual_display_ipc_values->display_zoom          = display_zoom;
            *virtual_display_ipc_values->display_north_offset  = vd_config->sbs_display_distance;
            virtual_display_ipc_values->look_ahead_cfg[0]      = look_ahead_constant;
            virtual_display_ipc_values->look_ahead_cfg[1]      = look_ahead_ftm;
            virtual_display_ipc_values->look_ahead_cfg[2]      = device->look_ahead_scanline_adjust;
            virtual_display_ipc_values->look_ahead_cfg[3]      = device->look_ahead_ms_cap;
            *virtual_display_ipc_values->sbs_content           = vd_config->sbs_content;
            *virtual_display_ipc_values->sbs_mode_stretched    = vd_config->sbs_mode_stretched;
        }
    } else {
        virtual_display_handle_device_disconnect_func();
    }
    device_checkin(device);
}

static void _gamescope_reshade_effect_ready() {
    gamescope_reshade_effect_request_time = 0;
}

void virtual_display_handle_device_connect_func() {
    set_virtual_display_ipc_values();
    using_gamescope_wayland = gamescope_reshade_wl_server_connect();
    if (using_gamescope_wayland) {
        if (config()->debug_ipc) log_debug("virtual_display_handle_device_connect_func using wayland\n");
        gamescope_reshade_effect_request_time = get_epoch_time_ms();
        add_gamescope_effect_ready_listener(_gamescope_reshade_effect_ready);
        enable_gamescope_virtual_display_effect();
    } else {
        if (config()->debug_ipc) log_debug("virtual_display_handle_device_connect_func using shared memory\n");
    }
};

void virtual_display_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                          bool imu_calibrated, ipc_values_type *ipc_values) {
    if (using_gamescope_wayland) {
        if (gamescope_reshade_effect_request_time != 0 && 
                get_epoch_time_ms() - gamescope_reshade_effect_request_time > 
                GAMESCOPE_RESHADE_WAIT_TIME_MS) {
            log_error("gamescope effect_ready event never received, falling back to shared memory IPC\n");
            wayland_cleanup();
            set_virtual_display_ipc_values();
        } else {
            set_gamescope_reshade_effect_uniform_variable("display_res", ipc_values->display_res, 2, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("display_fov", ipc_values->display_fov, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("lens_distance_ratio", ipc_values->lens_distance_ratio, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("custom_banner_enabled", custom_banner_ipc_values->enabled, 1, sizeof(bool), false);
            set_gamescope_reshade_effect_uniform_variable("keepalive_date", ipc_values->date, 4, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("imu_quat_data", ipc_values->imu_data, 16, sizeof(float), true);
        }
    }
}

void virtual_display_reset_imu_data_func() {
    if (using_gamescope_wayland) {
        set_gamescope_reshade_effect_uniform_variable("imu_quat_data", &imu_reset_data, 16, sizeof(float), true);
    }
}

void virtual_display_set_config_func(void* config) {
    if (!config) return;
    virtual_display_config* temp_config = (virtual_display_config*) config;

    if (vd_config) {
        if (vd_config->enabled != temp_config->enabled)
            log_message("Virtual display has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        if (!temp_config->enabled) {
            if (temp_config->passthrough_smooth_follow_enabled && temp_config->follow_mode_enabled) {
                // passthrough mode should use the default configs
                virtual_display_reset_config(temp_config);
                temp_config->passthrough_smooth_follow_enabled = true;
                temp_config->follow_mode_enabled = true;
            }
        } else {
            if (vd_config->look_ahead_override != temp_config->look_ahead_override)
                log_message("Look ahead override has changed to %f\n", temp_config->look_ahead_override);

            if (vd_config->display_zoom != temp_config->display_zoom)
                log_message("Display size has changed to %f\n", temp_config->display_zoom);

            if (vd_config->sbs_display_size != temp_config->sbs_display_size)
                log_message("SBS display size has changed to %f\n", temp_config->sbs_display_size);

            if (vd_config->sbs_display_distance != temp_config->sbs_display_distance)
                log_message("SBS display distance has changed to %f\n", temp_config->sbs_display_distance);

            if (vd_config->sbs_content != temp_config->sbs_content)
                log_message("SBS content has been changed to %s\n", temp_config->sbs_content ? "enabled" : "disabled");

            if (vd_config->sbs_mode_stretched != temp_config->sbs_mode_stretched)
                log_message("SBS mode has been changed to %s\n", temp_config->sbs_mode_stretched ? "stretched" : "centered");
        }

        free(vd_config);
    }
    vd_config = temp_config;

    set_virtual_display_ipc_values();
};

int virtual_display_register_features_func(char*** features) {
    *features = calloc(virtual_display_feature_count, sizeof(char*));
    (*features)[0] = strdup(sbs_feature_name);
    (*features)[1] = strdup(smooth_follow_feature_name);

    return virtual_display_feature_count;
}

const char *virtual_display_enabled_ipc_name = "virtual_display_enabled";
const char *virtual_display_look_ahead_cfg_ipc_name = "look_ahead_cfg";
const char *virtual_display_display_zoom_ipc_name = "display_zoom";
const char *virtual_display_display_north_offset_ipc_name = "display_north_offset";
const char *virtual_display_sbs_enabled_name = "sbs_enabled";
const char *virtual_display_sbs_content_name = "sbs_content";
const char *virtual_display_sbs_mode_stretched_name = "sbs_mode_stretched";

bool virtual_display_setup_ipc_func() {
    bool debug = config()->debug_ipc;
    if (!virtual_display_ipc_values) virtual_display_ipc_values = calloc(1, sizeof(virtual_display_ipc_values_type));
    setup_ipc_value(virtual_display_enabled_ipc_name, (void**) &virtual_display_ipc_values->enabled, sizeof(bool), debug);
    setup_ipc_value(virtual_display_look_ahead_cfg_ipc_name, (void**) &virtual_display_ipc_values->look_ahead_cfg, sizeof(float) * 4, debug);
    setup_ipc_value(virtual_display_display_zoom_ipc_name, (void**) &virtual_display_ipc_values->display_zoom, sizeof(float), debug);
    setup_ipc_value(virtual_display_display_north_offset_ipc_name, (void**) &virtual_display_ipc_values->display_north_offset, sizeof(float), debug);
    setup_ipc_value(virtual_display_sbs_enabled_name, (void**) &virtual_display_ipc_values->sbs_enabled, sizeof(bool), debug);
    setup_ipc_value(virtual_display_sbs_content_name, (void**) &virtual_display_ipc_values->sbs_content, sizeof(bool), debug);
    setup_ipc_value(virtual_display_sbs_mode_stretched_name, (void**) &virtual_display_ipc_values->sbs_mode_stretched, sizeof(bool), debug);

    set_virtual_display_ipc_values();

    return true;
}

void virtual_display_handle_state_func() {
    bool sbs_enabled = state()->sbs_mode_enabled && is_sbs_granted();
    if (virtual_display_ipc_values) *virtual_display_ipc_values->sbs_enabled = sbs_enabled;
    if (using_gamescope_wayland) set_gamescope_reshade_effect_uniform_variable("sbs_enabled", &sbs_enabled, 1, sizeof(bool), true);

    set_virtual_display_ipc_values();
}

const plugin_type virtual_display_plugin = {
    .id = "virtual_display",
    .default_config = virtual_display_default_config_func,
    .handle_config_line = virtual_display_handle_config_line_func,
    .set_config = virtual_display_set_config_func,
    .register_features = virtual_display_register_features_func,
    .handle_imu_data = virtual_display_handle_imu_data_func,
    .reset_imu_data = virtual_display_reset_imu_data_func,
    .setup_ipc = virtual_display_setup_ipc_func,
    .handle_state = virtual_display_handle_state_func,
    .handle_device_connect = virtual_display_handle_device_connect_func,
    .handle_device_disconnect = virtual_display_handle_device_disconnect_func
};