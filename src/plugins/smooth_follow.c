#include "config.h"
#include "imu.h"
#include "ipc.h"
#include "plugins/smooth_follow.h"
#include "runtime_context.h"
#include "strings.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

const int smooth_follow_feature_count = 1;
const char* smooth_follow_feature = "smooth_follow";

typedef enum {
    FOLLOW_STATE_NONE,
    FOLLOW_STATE_WAITING,
    FOLLOW_STATE_SLERPING,
    FOLLOW_STATE_DONE
} follow_state_type;

const smooth_follow_params virtual_display_smooth_follow_params = {
    .lower_angle_threshold = 20.0,
    .upper_angle_threshold = 40.0,
    .delay_ms = 2000,

    // moves 99% of the way to the center in 1.5 seconds
    .interpolation_ratio_ms = 1-pow(1 - 0.99, 1.0/1500.0)
};

const smooth_follow_params sideview_smooth_follow_params = {
    .lower_angle_threshold = 0.5,
    .upper_angle_threshold = 0.5,
    .delay_ms = 0,

    // moves 99% of the way to the center in 1 second
    .interpolation_ratio_ms = 1-pow(1 - 0.99, 1.0/1000.0)
};

uint32_t get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

smooth_follow_config* sf_config = NULL;
smooth_follow_params* sf_params = NULL;

void *smooth_follow_default_config_func() {
    smooth_follow_config *config = malloc(sizeof(smooth_follow_config));
    config->virtual_display_enabled = false;
    config->virtual_display_follow_enabled = false;
    config->sideview_enabled = false;
    config->sideview_follow_enabled = false;
    config->virtual_display_size = 1.0;

    return config;
};

void smooth_follow_handle_config_line_func(void* config, char* key, char* value) {
    smooth_follow_config* temp_config = (smooth_follow_config*) config;
    if (equal(key, "virtual_display_smooth_follow_enabled")) {
        boolean_config(key, value, &temp_config->virtual_display_follow_enabled);
    } else if (equal(key, "sideview_smooth_follow_enabled")) {
        boolean_config(key, value, &temp_config->sideview_follow_enabled);
    } else if (equal(key, "external_mode")) {
        if (equal(value, "virtual_display")) {
            temp_config->virtual_display_enabled = true;
        } else if (equal(value, "sideview")) {
            temp_config->sideview_enabled = true;
        }
    } else if (equal(key, "display_zoom")) {
        float_config(key, value, &temp_config->virtual_display_size);
    }
}

bool smooth_follow_enabled=false;
void smooth_follow_set_config_func(void* config) {
    if (!config) return;
    smooth_follow_config* temp_config = (smooth_follow_config*) config;

    if (sf_config) {
        if (temp_config->virtual_display_enabled && (
                sf_config->virtual_display_follow_enabled != temp_config->virtual_display_follow_enabled ||
                sf_config->virtual_display_enabled != temp_config->virtual_display_enabled))
            printf("Virtual display follow has been %s\n", temp_config->virtual_display_follow_enabled ? "enabled" : "disabled");

        if (temp_config->sideview_enabled && (
                sf_config->sideview_follow_enabled != temp_config->sideview_follow_enabled ||
                sf_config->sideview_enabled != temp_config->sideview_enabled))
            printf("Sideview follow has been %s\n", temp_config->sideview_follow_enabled ? "enabled" : "disabled");

        free(sf_config);
    }

    if (!sf_params) sf_params = malloc(sizeof(smooth_follow_params));
    if (temp_config->virtual_display_enabled && temp_config->virtual_display_follow_enabled) {
        *sf_params = virtual_display_smooth_follow_params;
        if (context.device) {
            float device_fov_threshold = context.device->fov * 0.9;
            sf_params->lower_angle_threshold = device_fov_threshold / 2.0 * temp_config->virtual_display_size;
            sf_params->upper_angle_threshold = device_fov_threshold * temp_config->virtual_display_size;
        }
    }
    if (temp_config->sideview_enabled && temp_config->sideview_follow_enabled) *sf_params = sideview_smooth_follow_params;
    smooth_follow_enabled = temp_config->virtual_display_enabled && temp_config->virtual_display_follow_enabled ||
                            temp_config->sideview_enabled && temp_config->sideview_follow_enabled;

    sf_config = temp_config;
}

int smooth_follow_register_features_func(char*** features) {
    *features = malloc(sizeof(char*) * smooth_follow_feature_count);
    (*features)[0] = strdup(smooth_follow_feature);

    return smooth_follow_feature_count;
}

follow_state_type follow_state = FOLLOW_STATE_NONE;
uint32_t follow_wait_time_start_ms = -1;
follow_state_type next_state_for_angle(float angle_degrees) {
    if (isnan(angle_degrees) || angle_degrees < fmin(1.0, sf_params->lower_angle_threshold) ||
        follow_state != FOLLOW_STATE_SLERPING && angle_degrees < sf_params->lower_angle_threshold) {
        follow_state = FOLLOW_STATE_NONE;
    } else if (angle_degrees > sf_params->lower_angle_threshold &&
               angle_degrees < sf_params->upper_angle_threshold &&
               sf_params->delay_ms > 0) {
        if (follow_state == FOLLOW_STATE_NONE) {
            follow_state = FOLLOW_STATE_WAITING;
            follow_wait_time_start_ms = get_time_ms();
        } else if (follow_state == FOLLOW_STATE_WAITING) {
            if (get_time_ms() - follow_wait_time_start_ms > sf_params->delay_ms) {
                follow_state = FOLLOW_STATE_SLERPING;
            }
        }
    } else {
        follow_state = FOLLOW_STATE_SLERPING;
    }

    return follow_state;
}

// ported and modified from https://github.com/g-truc/glm/blob/master/glm/ext/quaternion_common.inl
imu_quat_type slerp(imu_quat_type x, imu_quat_type y, float a) {
    imu_quat_type z = y;
    float cosTheta = x.w * z.w + x.x * z.x + x.y * z.y + x.z * z.z;
    if (cosTheta < 0) {
        imu_quat_type tmp = {
            .w = -z.w,
            .x = -z.x,
            .y = -z.y,
            .z = -z.z
        };
        z = tmp;
        cosTheta = -cosTheta;
    }
    float a_compliment = 1 - a;
    imu_quat_type result;
    float half_angle = acos(cosTheta);
    if (next_state_for_angle(radian_to_degree(2 * half_angle)) == FOLLOW_STATE_SLERPING) {
        float sin_of_angle = sin(half_angle);
        float x_multiplier = sin(a_compliment * half_angle) / sin_of_angle;
        float z_multiplier = sin(a * half_angle) / sin_of_angle;
        imu_quat_type tmp = {
            .w = x_multiplier * x.w + z_multiplier * z.w,
            .x = x_multiplier * x.x + z_multiplier * z.x,
            .y = x_multiplier * x.y + z_multiplier * z.y,
            .z = x_multiplier * x.z + z_multiplier * z.z
        };
        result = tmp;
        return result;
    }

    return x;
}

uint32_t last_timestamp_ms = -1;
imu_quat_type smooth_follow_modify_screen_center_func(uint32_t timestamp_ms, imu_quat_type quat, imu_quat_type screen_center) {
    if (!in_array(smooth_follow_feature, context.state->enabled_features, context.state->enabled_features_count) ||
        !smooth_follow_enabled || !sf_params) {
        return screen_center;
    }

    if (last_timestamp_ms == -1) {
        last_timestamp_ms = timestamp_ms;
        return screen_center;
    }

    uint32_t elapsed_ms = timestamp_ms - last_timestamp_ms;
    last_timestamp_ms = timestamp_ms;

    return slerp(screen_center, quat, 1 - pow(1 - sf_params->interpolation_ratio_ms, elapsed_ms));
}

const plugin_type smooth_follow_plugin = {
    .id = "smooth_follow",
    .default_config = smooth_follow_default_config_func,
    .handle_config_line = smooth_follow_handle_config_line_func,
    .set_config = smooth_follow_set_config_func,
    .register_features = smooth_follow_register_features_func,
    .modify_screen_center = smooth_follow_modify_screen_center_func,
};