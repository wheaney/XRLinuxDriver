#include "config.h"
#include "devices.h"
#include "features/smooth_follow.h"
#include "features/sbs.h"
#include "imu.h"
#include "ipc.h"
#include "logging.h"
#include "plugins.h"
#include "plugins/gamescope_reshade_wayland.h"
#include "plugins/smooth_follow.h"
#include "plugins/virtual_display.h"
#include "runtime_context.h"
#include "state.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

virtual_display_config *vd_config;
virtual_display_ipc_values_type *virtual_display_ipc_values;

const int virtual_display_feature_count = 2;

void virtual_display_reset_config(virtual_display_config *config) {
    config->enabled = false;
    config->look_ahead_override = 0.0;
    config->display_distance = 1.0;
    config->display_size = 1.0;
    config->sbs_content = false;
    config->sbs_mode_stretched = true;
    config->passthrough_smooth_follow_enabled = false;
    config->follow_mode_enabled = false;
    config->curved_display = false;
};

void *virtual_display_default_config_func() {
    virtual_display_config *config = calloc(1, sizeof(virtual_display_config));
    virtual_display_reset_config(config);

    return config;
};

void virtual_display_handle_config_line_func(void* config, char* key, char* value) {
    virtual_display_config* temp_config = (virtual_display_config*) config;

    if (equal(key, "external_mode")) {
        temp_config->enabled = list_string_contains("virtual_display", value);
        temp_config->follow_mode_enabled = list_string_contains("sideview", value);
    } else if (equal(key, "look_ahead")) {
        float_config(key, value, &temp_config->look_ahead_override);
    } else if (equal(key, "display_distance")) {
        float_config(key, value, &temp_config->display_distance);
    } else if (equal(key, "display_size")) {
        float_config(key, value, &temp_config->display_size);
    } else if (equal(key, "sbs_content")) {
        boolean_config(key, value, &temp_config->sbs_content);
    } else if (equal(key, "sbs_mode_stretched")) {
        boolean_config(key, value, &temp_config->sbs_mode_stretched);
    } else if (equal(key, "sideview_smooth_follow_enabled") && is_smooth_follow_granted()) {
        boolean_config(key, value, &temp_config->passthrough_smooth_follow_enabled);
    } else if (equal(key, "curved_display")) {
        boolean_config(key, value, &temp_config->curved_display);
    }
};

void virtual_display_handle_device_disconnect_func() {
    bool enabled = false;
    if (virtual_display_ipc_values) *virtual_display_ipc_values->enabled = enabled;
    set_gamescope_reshade_effect_uniform_variable("virtual_display_enabled", &enabled, 1, sizeof(bool), true);
};

void set_virtual_display_ipc_values() {
    if (!vd_config) vd_config = virtual_display_default_config_func();

    device_properties_type* device = device_checkout();
    if (device != NULL) {
        bool enabled = !config()->disabled && 
                            (vd_config->enabled ||
                            vd_config->follow_mode_enabled &&
                            vd_config->passthrough_smooth_follow_enabled);
        bool show_banner = enabled && state()->calibration_state == CALIBRATING;

        float look_ahead_constant = vd_config->look_ahead_override == 0 ?
                                        device->look_ahead_constant :
                                        vd_config->look_ahead_override;
        float look_ahead_ftm =  vd_config->look_ahead_override == 0 ? 
                                    device->look_ahead_frametime_multiplier : 
                                    0.0;
        float look_ahead_cfg[4] = {look_ahead_constant, look_ahead_ftm, device->look_ahead_scanline_adjust, device->look_ahead_ms_cap};

        // computed values based on display config/state
        float display_north_offset = (device->provides_position || state()->sbs_mode_enabled)
                                         ? vd_config->display_distance
                                         : 1.0;
        float display_aspect_ratio = (float)device->resolution_w / (float)device->resolution_h;
        float diag_to_vert_ratio = sqrt(pow(display_aspect_ratio, 2) + 1);
        float half_fov_z_rads = degree_to_radian(device->fov / diag_to_vert_ratio) / 2;
        float half_fov_y_rads = half_fov_z_rads * display_aspect_ratio;
        float fov_half_widths[2] = {tan(half_fov_y_rads), tan(half_fov_z_rads)};
        float fov_widths[2] = {fov_half_widths[0] * 2, fov_half_widths[1] * 2};
        float texcoord_x_limits[2] = {0.0, 1.0};
        float texcoord_x_limits_r[2] = {0.0, 1.0};

        // for 3DoF-only devices, the north offset creates a realistic shift in viewport position based 
        // on orientation, but 6DoF devices give us the actual position, so we don't use the lens north
        // offset to avoid double shifting
        float lens_north_offset = device->provides_position ? 0.0 : device->lens_distance_ratio;
        float lens_vector[3] = {lens_north_offset, 0.0, 0.0};
        float lens_vector_r[3] = {lens_north_offset, 0.0, 0.0};

        // gamescope's texture will always be full width (no black bars)
        bool sbs_mode_full_width = is_gamescope_reshade_ipc_connected() || vd_config->sbs_mode_stretched;

        // if using vkBasalt as an implicit layer and the content is full screen, it must have been stretched,
        // if this is true it tells the shader to consider the real width of the content to be half of the texture width
        bool sbs_mode_stretched = !is_gamescope_reshade_ipc_connected() && vd_config->sbs_mode_stretched;

        if (state()->sbs_mode_enabled) {
            lens_vector[1] = device->lens_distance_ratio / 3.0;
            lens_vector_r[1] = -lens_vector[1];
            if (vd_config->sbs_content) {
                texcoord_x_limits[1] = 0.5;
                texcoord_x_limits_r[0] = 0.5;
                if (!sbs_mode_full_width) {
                    texcoord_x_limits[0] = 0.25;
                    texcoord_x_limits_r[1] = 0.75;
                }
            } else if (!sbs_mode_full_width) {
                texcoord_x_limits[0] = 0.25;
                texcoord_x_limits[1] = 0.75;
                texcoord_x_limits_r[0] = 0.25;
                texcoord_x_limits_r[1] = 0.75;
            }
        }
        if (virtual_display_ipc_values) {
            *virtual_display_ipc_values->enabled                = enabled && !is_gamescope_reshade_ipc_connected();
            *virtual_display_ipc_values->show_banner            = show_banner;
            *virtual_display_ipc_values->display_size           = vd_config->display_size;
            *virtual_display_ipc_values->sbs_mode_stretched     = sbs_mode_stretched;
            *virtual_display_ipc_values->display_north_offset   = display_north_offset;
            *virtual_display_ipc_values->curved_display         = vd_config->curved_display;
            *virtual_display_ipc_values->half_fov_z_rads        = half_fov_z_rads;
            *virtual_display_ipc_values->half_fov_y_rads        = half_fov_y_rads;
            memcpy(virtual_display_ipc_values->look_ahead_cfg, look_ahead_cfg, sizeof(look_ahead_cfg));
            memcpy(virtual_display_ipc_values->fov_half_widths, fov_half_widths, sizeof(fov_half_widths));
            memcpy(virtual_display_ipc_values->fov_widths, fov_widths, sizeof(fov_widths));
            memcpy(virtual_display_ipc_values->texcoord_x_limits, texcoord_x_limits, sizeof(texcoord_x_limits));
            memcpy(virtual_display_ipc_values->texcoord_x_limits_r, texcoord_x_limits_r, sizeof(texcoord_x_limits_r));
            memcpy(virtual_display_ipc_values->lens_vector, lens_vector, sizeof(lens_vector));
            memcpy(virtual_display_ipc_values->lens_vector_r, lens_vector_r, sizeof(lens_vector_r));
        }

        // don't set the "flush" flag here if gamescope is enabled, we'll let the frequent IMU data writes trigger the flush
        bool gamescope_enabled = enabled && is_gamescope_reshade_ipc_connected();
        set_gamescope_reshade_effect_uniform_variable("virtual_display_enabled", &gamescope_enabled, 1, sizeof(bool), !gamescope_enabled);
        if (gamescope_enabled) {
            set_gamescope_reshade_effect_uniform_variable("show_banner", &show_banner, 1, sizeof(bool), false);
            set_gamescope_reshade_effect_uniform_variable("display_size", &vd_config->display_size, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("sbs_mode_stretched", &sbs_mode_stretched, 1, sizeof(bool), false);
            set_gamescope_reshade_effect_uniform_variable("display_north_offset", &display_north_offset, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("look_ahead_cfg", (void*) look_ahead_cfg, 4, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("curved_display", &vd_config->curved_display, 1, sizeof(bool), false);
            set_gamescope_reshade_effect_uniform_variable("half_fov_z_rads", &half_fov_z_rads, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("half_fov_y_rads", &half_fov_y_rads, 1, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("fov_half_widths", (void*) fov_half_widths, 2, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("fov_widths", (void*) fov_widths, 2, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("texcoord_x_limits", (void*) texcoord_x_limits, 2, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("texcoord_x_limits_r", (void*) texcoord_x_limits_r, 2, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("lens_vector", (void*) lens_vector, 3, sizeof(float), false);
            set_gamescope_reshade_effect_uniform_variable("lens_vector_r", (void*) lens_vector_r, 3, sizeof(float), false);
        }
    } else {
        virtual_display_handle_device_disconnect_func();
    }
    device_checkin(device);
}

void virtual_display_set_config_func(void* config) {
    if (!config) return;
    virtual_display_config* temp_config = (virtual_display_config*) config;

    if (vd_config) {
        if (vd_config->enabled != temp_config->enabled)
            log_message("Virtual display has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        if (temp_config->enabled || temp_config->follow_mode_enabled) {
            if (vd_config->look_ahead_override != temp_config->look_ahead_override)
                log_message("Look ahead override has changed to %f\n", temp_config->look_ahead_override);

            if (vd_config->display_size != temp_config->display_size)
                log_message("Display size has changed to %f\n", temp_config->display_size);

            if (vd_config->display_distance != temp_config->display_distance)
                log_message("Display distance has changed to %f\n", temp_config->display_distance);

            if (vd_config->sbs_content != temp_config->sbs_content)
                log_message("SBS content has been changed to %s\n", temp_config->sbs_content ? "enabled" : "disabled");

            if (vd_config->sbs_mode_stretched != temp_config->sbs_mode_stretched)
                log_message("SBS mode has been changed to %s\n", temp_config->sbs_mode_stretched ? "stretched" : "centered");

            if (vd_config->curved_display != temp_config->curved_display)
                log_message("Curved display has been %s\n", temp_config->curved_display ? "enabled" : "disabled");
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
const char *virtual_display_show_banner_ipc_name = "show_banner";
const char *virtual_display_look_ahead_cfg_ipc_name = "look_ahead_cfg";
const char *virtual_display_display_size_ipc_name = "display_size";
const char *virtual_display_display_north_offset_ipc_name = "display_north_offset";
const char *virtual_display_sbs_enabled_ipc_name = "sbs_enabled";
const char *virtual_display_sbs_content_ipc_name = "sbs_content";
const char *virtual_display_sbs_mode_stretched_ipc_name = "sbs_mode_stretched";
const char *virtual_display_half_fov_z_rads_ipc_name = "half_fov_z_rads";
const char *virtual_display_half_fov_y_rads_ipc_name = "half_fov_y_rads";
const char *virtual_display_fov_half_widths_ipc_name = "fov_half_widths";
const char *virtual_display_fov_widths_ipc_name = "fov_widths";
const char *virtual_display_texcoord_x_limits_ipc_name = "texcoord_x_limits";
const char *virtual_display_texcoord_x_limits_r_ipc_name = "texcoord_x_limits_r";
const char *virtual_display_lens_vector_ipc_name = "lens_vector";
const char *virtual_display_lens_vector_r_ipc_name = "lens_vector_r";
const char *virtual_display_curved_display_ipc_name = "curved_display";

bool virtual_display_setup_ipc_func() {
    bool debug = config()->debug_ipc;
    if (!virtual_display_ipc_values) virtual_display_ipc_values = calloc(1, sizeof(virtual_display_ipc_values_type));
    setup_ipc_value(virtual_display_enabled_ipc_name, (void**) &virtual_display_ipc_values->enabled, sizeof(bool), debug);
    setup_ipc_value(virtual_display_show_banner_ipc_name, (void**) &virtual_display_ipc_values->show_banner, sizeof(bool), debug);
    setup_ipc_value(virtual_display_look_ahead_cfg_ipc_name, (void**) &virtual_display_ipc_values->look_ahead_cfg, sizeof(float) * 4, debug);
    setup_ipc_value(virtual_display_display_size_ipc_name, (void**) &virtual_display_ipc_values->display_size, sizeof(float), debug);
    setup_ipc_value(virtual_display_display_north_offset_ipc_name, (void**) &virtual_display_ipc_values->display_north_offset, sizeof(float), debug);
    setup_ipc_value(virtual_display_sbs_enabled_ipc_name, (void**) &virtual_display_ipc_values->sbs_enabled, sizeof(bool), debug);
    setup_ipc_value(virtual_display_sbs_content_ipc_name, (void**) &virtual_display_ipc_values->sbs_content, sizeof(bool), debug);
    setup_ipc_value(virtual_display_sbs_mode_stretched_ipc_name, (void**) &virtual_display_ipc_values->sbs_mode_stretched, sizeof(bool), debug);
    setup_ipc_value(virtual_display_half_fov_z_rads_ipc_name, (void**) &virtual_display_ipc_values->half_fov_z_rads, sizeof(float), debug);
    setup_ipc_value(virtual_display_half_fov_y_rads_ipc_name, (void**) &virtual_display_ipc_values->half_fov_y_rads, sizeof(float), debug);
    setup_ipc_value(virtual_display_fov_half_widths_ipc_name, (void**) &virtual_display_ipc_values->fov_half_widths, sizeof(float) * 2, debug);
    setup_ipc_value(virtual_display_fov_widths_ipc_name, (void**) &virtual_display_ipc_values->fov_widths, sizeof(float) * 2, debug);
    setup_ipc_value(virtual_display_texcoord_x_limits_ipc_name, (void**) &virtual_display_ipc_values->texcoord_x_limits, sizeof(float) * 2, debug);
    setup_ipc_value(virtual_display_texcoord_x_limits_r_ipc_name, (void**) &virtual_display_ipc_values->texcoord_x_limits_r, sizeof(float) * 2, debug);
    setup_ipc_value(virtual_display_lens_vector_ipc_name, (void**) &virtual_display_ipc_values->lens_vector, sizeof(float) * 3, debug);
    setup_ipc_value(virtual_display_lens_vector_r_ipc_name, (void**) &virtual_display_ipc_values->lens_vector_r, sizeof(float) * 3, debug);
    setup_ipc_value(virtual_display_curved_display_ipc_name, (void**) &virtual_display_ipc_values->curved_display, sizeof(bool), debug);

    set_virtual_display_ipc_values();

    return true;
}

void virtual_display_handle_state_func() {
    bool sbs_enabled = state()->sbs_mode_enabled && is_sbs_granted();
    if (virtual_display_ipc_values) *virtual_display_ipc_values->sbs_enabled = sbs_enabled;
    set_gamescope_reshade_effect_uniform_variable("sbs_enabled", &sbs_enabled, 1, sizeof(bool), true);

    set_virtual_display_ipc_values();
}

const plugin_type virtual_display_plugin = {
    .id = "virtual_display",
    .default_config = virtual_display_default_config_func,
    .handle_config_line = virtual_display_handle_config_line_func,
    .set_config = virtual_display_set_config_func,
    .register_features = virtual_display_register_features_func,
    .setup_ipc = virtual_display_setup_ipc_func,
    .handle_ipc_change = set_virtual_display_ipc_values,
    .handle_state = virtual_display_handle_state_func,
    .handle_device_connect = set_virtual_display_ipc_values,
    .handle_device_disconnect = virtual_display_handle_device_disconnect_func
};