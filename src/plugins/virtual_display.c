#include "buffer.h"
#include "config.h"
#include "device.h"
#include "ipc.h"
#include "plugins.h"
#include "plugins/virtual_display.h"
#include "runtime_context.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define GYRO_BUFFERS_COUNT 5 // quat values: x, y, z, w, timestamp

buffer_type **quat_stage_1_buffer;
buffer_type **quat_stage_2_buffer;
virtual_display_config *vd_config;
virtual_display_ipc_values_type *virtual_display_ipc_values;

const int virtual_display_feature_count = 1;
const char* virtual_display_feature_sbs = "sbs";

void *virtual_display_default_config_func() {
    virtual_display_config *config = malloc(sizeof(virtual_display_config));
    config->enabled = false;
    config->look_ahead_override = 0.0;
    config->display_zoom = 1.0;
    config->sbs_display_distance = 1.0;
    config->sbs_content = false;
    config->sbs_mode_stretched = false;

    return config;
};

void virtual_display_handle_config_line_func(void* config, char* key, char* value) {
    virtual_display_config* temp_config = (virtual_display_config*) config;

    if (equal(key, "external_mode")) {
        temp_config->enabled = equal(value, "virtual_display");
    } else if (equal(key, "look_ahead")) {
        float_config(key, value, &temp_config->look_ahead_override);
    } else if (equal(key, "external_zoom") || equal(key, "display_zoom")) {
        float_config(key, value, &temp_config->display_zoom);
    } else if (equal(key, "sbs_display_distance")) {
        float_config(key, value, &temp_config->sbs_display_distance);
    } else if (equal(key, "sbs_display_size")) {
        float_config(key, value, &temp_config->sbs_display_size);
    } else if (equal(key, "sbs_content")) {
        boolean_config(key, value, &temp_config->sbs_mode_stretched);
    } else if (equal(key, "sbs_mode_stretched")) {
        boolean_config(key, value, &temp_config->sbs_mode_stretched);
    }
};

void virtual_display_handle_device_disconnect_func() {
    if (!virtual_display_ipc_values) return;
    *virtual_display_ipc_values->enabled = false;
};

void set_virtual_display_ipc_values() {
    if (!virtual_display_ipc_values) return;
    if (!vd_config) vd_config = virtual_display_default_config_func();

    if (context.device) {
        *virtual_display_ipc_values->enabled               = vd_config->enabled && !context.config->disabled;
        *virtual_display_ipc_values->display_zoom          = context.state->sbs_mode_enabled ? vd_config->sbs_display_size :
                                                                vd_config->display_zoom;
        *virtual_display_ipc_values->display_north_offset  = vd_config->sbs_display_distance;
        virtual_display_ipc_values->look_ahead_cfg[0]      = vd_config->look_ahead_override == 0 ?
                                                                context.device->look_ahead_constant :
                                                                vd_config->look_ahead_override;
        virtual_display_ipc_values->look_ahead_cfg[1]      = vd_config->look_ahead_override == 0 ?
                                                                context.device->look_ahead_frametime_multiplier : 0.0;
        virtual_display_ipc_values->look_ahead_cfg[2]      = context.device->look_ahead_scanline_adjust;
        virtual_display_ipc_values->look_ahead_cfg[3]      = context.device->look_ahead_ms_cap;
        *virtual_display_ipc_values->sbs_content           = vd_config->sbs_content;
        *virtual_display_ipc_values->sbs_mode_stretched    = vd_config->sbs_mode_stretched;
    } else {
        virtual_display_handle_device_disconnect_func();
    }
}

void virtual_display_set_config_func(void* config) {
    if (!config) return;
    virtual_display_config* temp_config = (virtual_display_config*) config;

    if (vd_config) {
        if (vd_config->enabled != temp_config->enabled)
            printf("Virtual display has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        if (vd_config->look_ahead_override != temp_config->look_ahead_override)
            fprintf(stdout, "Look ahead override has changed to %f\n", temp_config->look_ahead_override);

        if (vd_config->display_zoom != temp_config->display_zoom)
            fprintf(stdout, "Display size has changed to %f\n", temp_config->display_zoom);

        if (vd_config->sbs_display_size != temp_config->sbs_display_size)
            fprintf(stdout, "SBS display size has changed to %f\n", temp_config->sbs_display_size);

        if (vd_config->sbs_display_distance != temp_config->sbs_display_distance)
            fprintf(stdout, "SBS display distance has changed to %f\n", temp_config->sbs_display_distance);

        if (vd_config->sbs_content != temp_config->sbs_content)
            fprintf(stdout, "SBS content has been changed to %s\n", temp_config->sbs_content ? "enabled" : "disabled");

        if (vd_config->sbs_mode_stretched != temp_config->sbs_mode_stretched)
            fprintf(stdout, "SBS mode has been changed to %s\n", temp_config->sbs_mode_stretched ? "stretched" : "centered");

        free(vd_config);
    }
    vd_config = temp_config;

    set_virtual_display_ipc_values();
};

int virtual_display_register_features_func(char*** features) {
    *features = malloc(sizeof(char*) * virtual_display_feature_count);
    *features[0] = strdup(virtual_display_feature_sbs);

    return virtual_display_feature_count;
}

const char *virtual_display_enabled_ipc_name = "virtual_display_enabled";
const char *virtual_display_imu_data_ipc_name = "imu_quat_data";
const char *virtual_display_imu_data_mutex_ipc_name = "imu_quat_data_mutex";
const char *virtual_display_look_ahead_cfg_ipc_name = "look_ahead_cfg";
const char *virtual_display_display_zoom_ipc_name = "display_zoom";
const char *virtual_display_display_north_offset_ipc_name = "display_north_offset";
const char *virtual_display_sbs_enabled_name = "sbs_enabled";
const char *virtual_display_sbs_content_name = "sbs_content";
const char *virtual_display_sbs_mode_stretched_name = "sbs_mode_stretched";

bool virtual_display_setup_ipc_func() {
    bool debug = context.config->debug_ipc;
    if (!virtual_display_ipc_values) virtual_display_ipc_values = malloc(sizeof(virtual_display_ipc_values_type));
    setup_ipc_value(virtual_display_enabled_ipc_name, (void**) &virtual_display_ipc_values->enabled, sizeof(bool), debug);
    setup_ipc_value(virtual_display_imu_data_ipc_name, (void**) &virtual_display_ipc_values->imu_data, sizeof(float) * 16, debug);
    setup_ipc_value(virtual_display_look_ahead_cfg_ipc_name, (void**) &virtual_display_ipc_values->look_ahead_cfg, sizeof(float) * 4, debug);
    setup_ipc_value(virtual_display_display_zoom_ipc_name, (void**) &virtual_display_ipc_values->display_zoom, sizeof(float), debug);
    setup_ipc_value(virtual_display_display_north_offset_ipc_name, (void**) &virtual_display_ipc_values->display_north_offset, sizeof(float), debug);
    setup_ipc_value(virtual_display_sbs_enabled_name, (void**) &virtual_display_ipc_values->sbs_enabled, sizeof(bool), debug);
    setup_ipc_value(virtual_display_sbs_content_name, (void**) &virtual_display_ipc_values->sbs_content, sizeof(bool), debug);
    setup_ipc_value(virtual_display_sbs_mode_stretched_name, (void**) &virtual_display_ipc_values->sbs_mode_stretched, sizeof(bool), debug);

    // attempt to destroy the mutex if it already existed from a previous run
    setup_ipc_value(virtual_display_imu_data_mutex_ipc_name, (void**) &virtual_display_ipc_values->imu_data_mutex, sizeof(pthread_mutex_t), debug);
    int ret = pthread_mutex_destroy(virtual_display_ipc_values->imu_data_mutex);
    if (ret != 0) {
        perror("pthread_mutex_destroy");
        if (ret != EINVAL) return false;
    }

    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        perror("pthread_mutexattr_init");
        return false;
    }
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        perror("pthread_mutexattr_setpshared");
        return false;
    }
    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0) {
        perror("pthread_mutexattr_setrobust");
        return false;
    }
    if (pthread_mutex_init(virtual_display_ipc_values->imu_data_mutex, &attr) != 0) {
        perror("pthread_mutex_init");
        return false;
    }

    set_virtual_display_ipc_values();

    return true;
}

void virtual_display_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                          bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values) {
    if (vd_config && vd_config->enabled && ipc_enabled && virtual_display_ipc_values) {
        if (imu_calibrated) {
            if (quat_stage_1_buffer == NULL || quat_stage_2_buffer == NULL) {
                quat_stage_1_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
                quat_stage_2_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
                for (int i = 0; i < GYRO_BUFFERS_COUNT; i++) {
                    quat_stage_1_buffer[i] = create_buffer(context.device->imu_buffer_size);
                    quat_stage_2_buffer[i] = create_buffer(context.device->imu_buffer_size);
                    if (quat_stage_1_buffer[i] == NULL || quat_stage_2_buffer[i] == NULL) {
                        fprintf(stderr, "Error allocating memory\n");
                        exit(1);
                    }
                }
            }

            // the oldest values are zero/unset if the buffer hasn't been filled yet, so we check prior to doing a
            // push/pop, to know if the values that are returned will be relevant to our calculations
            bool was_full = is_full(quat_stage_1_buffer[0]);
            float stage_1_quat_w = push(quat_stage_1_buffer[0], quat.w);
            float stage_1_quat_x = push(quat_stage_1_buffer[1], quat.x);
            float stage_1_quat_y = push(quat_stage_1_buffer[2], quat.y);
            float stage_1_quat_z = push(quat_stage_1_buffer[3], quat.z);

            // TODO - timestamp_ms can only get as large as 2^24 before it starts to lose precision as a float,
            //        which is less than 5 hours of usage. Update this to just send two delta times, t0-t1 and t1-t2.
            float stage_1_ts = push(quat_stage_1_buffer[4], (float)timestamp_ms);

            if (was_full) {
                was_full = is_full(quat_stage_2_buffer[0]);
                float stage_2_quat_w = push(quat_stage_2_buffer[0], stage_1_quat_w);
                float stage_2_quat_x = push(quat_stage_2_buffer[1], stage_1_quat_x);
                float stage_2_quat_y = push(quat_stage_2_buffer[2], stage_1_quat_y);
                float stage_2_quat_z = push(quat_stage_2_buffer[3], stage_1_quat_z);
                float stage_2_ts = push(quat_stage_2_buffer[4], stage_1_ts);

                if (was_full) {
                    pthread_mutex_lock(virtual_display_ipc_values->imu_data_mutex);

                    // write to shared memory for anyone using the same ipc prefix to consume
                    virtual_display_ipc_values->imu_data[0] = quat.x;
                    virtual_display_ipc_values->imu_data[1] = quat.y;
                    virtual_display_ipc_values->imu_data[2] = quat.z;
                    virtual_display_ipc_values->imu_data[3] = quat.w;
                    virtual_display_ipc_values->imu_data[4] = stage_1_quat_x;
                    virtual_display_ipc_values->imu_data[5] = stage_1_quat_y;
                    virtual_display_ipc_values->imu_data[6] = stage_1_quat_z;
                    virtual_display_ipc_values->imu_data[7] = stage_1_quat_w;
                    virtual_display_ipc_values->imu_data[8] = stage_2_quat_x;
                    virtual_display_ipc_values->imu_data[9] = stage_2_quat_y;
                    virtual_display_ipc_values->imu_data[10] = stage_2_quat_z;
                    virtual_display_ipc_values->imu_data[11] = stage_2_quat_w;
                    virtual_display_ipc_values->imu_data[12] = (float)timestamp_ms;
                    virtual_display_ipc_values->imu_data[13] = stage_1_ts;
                    virtual_display_ipc_values->imu_data[14] = stage_2_ts;

                    pthread_mutex_unlock(virtual_display_ipc_values->imu_data_mutex);
                }
            }
        }
    }
}

void virtual_display_handle_state_func() {
    if (!virtual_display_ipc_values) return;
    *virtual_display_ipc_values->sbs_enabled = context.state->sbs_mode_enabled &&
        in_array(virtual_display_feature_sbs, context.state->enabled_features, context.state->enabled_features_count);

    set_virtual_display_ipc_values();
}

void virtual_display_reset_imu_data_func() {
    if (!virtual_display_ipc_values) return;

    // reset the 4 quaternion values to (0, 0, 0, 1)
    for (int i = 0; i < 16; i += 4) {
        virtual_display_ipc_values->imu_data[i] = 0;
        virtual_display_ipc_values->imu_data[i + 1] = 0;
        virtual_display_ipc_values->imu_data[i + 2] = 0;
        virtual_display_ipc_values->imu_data[i + 3] = 1;
    }
}

const plugin_type virtual_display_plugin = {
    .id = "virtual_display",
    .default_config = virtual_display_default_config_func,
    .handle_config_line = virtual_display_handle_config_line_func,
    .set_config = virtual_display_set_config_func,
    .register_features = virtual_display_register_features_func,
    .setup_ipc = virtual_display_setup_ipc_func,
    .handle_imu_data = virtual_display_handle_imu_data_func,
    .reset_imu_data = virtual_display_reset_imu_data_func,
    .handle_state = virtual_display_handle_state_func,
    .handle_device_disconnect = virtual_display_handle_device_disconnect_func
};