#include "buffer.h"
#include "device.h"
#include "features/sbs.h"
#include "plugins.h"
#include "plugins/breezy_desktop.h"
#include "plugins/custom_banner.h"
#include "runtime_context.h"
#include "state.h"
#include "system.h"

#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>


const char* shared_mem_directory = "/dev/shm";
const char* shared_mem_filename = "imu_data";

#define NUM_IMU_VALUES 16
float IMU_RESET[NUM_IMU_VALUES] = {
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 0.0
};

#define GYRO_BUFFERS_COUNT 5 // quat values: x, y, z, w, timestamp

buffer_type **bd_quat_stage_1_buffer;
buffer_type **bd_quat_stage_2_buffer;
breezy_desktop_config *bd_config;

void breezy_desktop_reset_config(breezy_desktop_config *config) {
    config->enabled = false;
    config->look_ahead_override = 0.0;
    config->display_zoom = 1.0;
    config->sbs_display_distance = 1.0;
    config->sbs_display_size = 1.0;
    config->sbs_content = false;
    config->sbs_mode_stretched = false;
};

void *breezy_desktop_default_config_func() {
    breezy_desktop_config *config = calloc(1, sizeof(breezy_desktop_config));
    breezy_desktop_reset_config(config);

    return config;
};

void breezy_desktop_handle_config_line_func(void* config, char* key, char* value) {
    breezy_desktop_config* temp_config = (breezy_desktop_config*) config;

    if (equal(key, "external_mode")) {
        temp_config->enabled = equal(value, "breezy_desktop");
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
    }
};

uint32_t getEpochSec() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec;
}

const uint8_t DATA_LAYOUT_VERSION = 1;
#define BOOL_TRUE 1
#define BOOL_FALSE 0

// IMU data is written more frequently, so we need to know the offset in the file
const int IMU_DATA_OFFSET = 
    sizeof(uint8_t) + // version
    sizeof(uint8_t) + // enabled
    sizeof(uint32_t) + // epoch_sec
    sizeof(float) * 4 + // look_ahead_cfg
    sizeof(uint32_t) * 2 + // display_res
    sizeof(float) + // fov
    sizeof(float) + // display_zoom
    sizeof(float) + // sbs_display_distance
    sizeof(float) + // lens_distance_ratio
    sizeof(uint8_t) + // sbs_enabled
    sizeof(uint8_t) + // sbs_content
    sizeof(uint8_t) + // sbs_mode_stretched
    sizeof(uint8_t); // custom_banner_enabled

void do_write_config_data(FILE* fp) {
    if (!bd_config) bd_config = breezy_desktop_default_config_func();

    uint8_t enabled = BOOL_FALSE;
    fwrite(&DATA_LAYOUT_VERSION, sizeof(uint8_t), 1, fp);
    if (context.device) {
        enabled = !context.config->disabled && bd_config->enabled ? BOOL_TRUE : BOOL_FALSE;
        const uint32_t epoch_sec = getEpochSec();
        const float look_ahead_constant =   bd_config->look_ahead_override == 0 ?
                                                context.device->look_ahead_constant :
                                                bd_config->look_ahead_override;
        const float look_ahead_frametime_multiplier =   bd_config->look_ahead_override == 0 ?
                                                            context.device->look_ahead_frametime_multiplier : 0.0;
        float look_ahead_cfg[4] = {
            look_ahead_constant,
            look_ahead_frametime_multiplier,
            context.device->look_ahead_scanline_adjust, 
            context.device->look_ahead_ms_cap
        };
        float display_res[2] = {
            context.device->resolution_w,
            context.device->resolution_h
        };
        uint8_t display_zoom = context.state->sbs_mode_enabled ? bd_config->sbs_display_size : bd_config->display_zoom; 
        uint8_t sbs_enabled = context.state->sbs_mode_enabled && is_sbs_granted() ? BOOL_TRUE : BOOL_FALSE;
        uint8_t sbs_content = bd_config->sbs_content ? BOOL_TRUE : BOOL_FALSE;
        uint8_t sbs_mode_stretched = bd_config->sbs_mode_stretched ? BOOL_TRUE : BOOL_FALSE;
        uint8_t custom_banner_enabled = custom_banner_ipc_values && custom_banner_ipc_values->enabled ? BOOL_TRUE : BOOL_FALSE;

        fwrite(&enabled, sizeof(uint8_t), 1, fp);
        fwrite(&epoch_sec, sizeof(uint32_t), 1, fp);
        fwrite(look_ahead_cfg, sizeof(float), 4, fp);
        fwrite(display_res, sizeof(uint32_t), 2, fp);
        fwrite(&context.device->fov, sizeof(float), 1, fp);
        fwrite(&display_zoom, sizeof(float), 1, fp);
        fwrite(&bd_config->sbs_display_distance, sizeof(float), 1, fp);
        fwrite(&context.device->lens_distance_ratio, sizeof(float), 1, fp);
        fwrite(&sbs_enabled, sizeof(uint8_t), 1, fp);
        fwrite(&sbs_content, sizeof(uint8_t), 1, fp);
        fwrite(&sbs_mode_stretched, sizeof(uint8_t), 1, fp);
        fwrite(&custom_banner_enabled, sizeof(uint8_t), 1, fp);
    } else {
        fwrite(&enabled, sizeof(uint8_t), 1, fp);

        // we already wrote version and enabled flags
        const int remainingBytes = IMU_DATA_OFFSET - sizeof(uint8_t) * 2;

        uint8_t *zero_data = calloc(remainingBytes, 1);
        fwrite(zero_data, remainingBytes, 1, fp);
        free(zero_data);
    }
}

FILE* get_shared_mem_file() {
    char file_path[1024];
    snprintf(file_path, strlen(shared_mem_directory) + strlen(shared_mem_filename) + 2, "%s/%s", shared_mem_directory, shared_mem_filename);
    FILE* fp = fopen(file_path, "r+b");

    if (!fp) {
        fp = fopen(file_path, "wb");
        if (!fp) {
            fprintf(stderr, "Error opening file %s: %s\n", file_path, strerror(errno));
            return NULL;
        }
        do_write_config_data(fp);
        fwrite(IMU_RESET, sizeof(float), NUM_IMU_VALUES, fp);
    }

    fseek(fp, 0, SEEK_SET);

    return fp;
}

void write_config_data() {
    FILE* fp = get_shared_mem_file();
    if (fp == NULL) return;

    do_write_config_data(fp);

    fclose(fp);
}

void breezy_desktop_set_config_func(void* config) {
    if (!config) return;
    breezy_desktop_config* temp_config = (breezy_desktop_config*) config;

    if (bd_config) {
        if (bd_config->enabled != temp_config->enabled)
            printf("Breezy desktop has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        free(bd_config);
    }
    bd_config = temp_config;

    write_config_data();
};

int breezy_imu_counter = 0;
void breezy_desktop_write_imu_data(float values[NUM_IMU_VALUES]) {
    FILE* fp = get_shared_mem_file();
    if (fp == NULL) return;

    if (breezy_imu_counter == 0) {
        // write this periodically, to ensure a recent heartbeat at the very least
        do_write_config_data(fp);
    } else {
        // if not writing config data, move the file pointer to the IMU offset
        fseek(fp, IMU_DATA_OFFSET, SEEK_SET);
    }
    fwrite(values, sizeof(float), NUM_IMU_VALUES, fp);

    fclose(fp);

    // reset the counter every second
    if ((++breezy_imu_counter % context.device->imu_cycles_per_s) == 0) {
        breezy_imu_counter = 0;
    }
}

void breezy_desktop_reset_imu_data_func() {
    breezy_desktop_write_imu_data(IMU_RESET);
}

// TODO - share this with virtual_display plugin
void breezy_desktop_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                          bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values) {
    if (imu_calibrated && bd_config && bd_config->enabled) {
        if (bd_quat_stage_1_buffer == NULL || bd_quat_stage_2_buffer == NULL) {
            bd_quat_stage_1_buffer = calloc(GYRO_BUFFERS_COUNT, sizeof(buffer_type*));
            bd_quat_stage_2_buffer = calloc(GYRO_BUFFERS_COUNT, sizeof(buffer_type*));
            for (int i = 0; i < GYRO_BUFFERS_COUNT; i++) {
                bd_quat_stage_1_buffer[i] = create_buffer(context.device->imu_buffer_size);
                bd_quat_stage_2_buffer[i] = create_buffer(context.device->imu_buffer_size);
                if (bd_quat_stage_1_buffer[i] == NULL || bd_quat_stage_2_buffer[i] == NULL) {
                    fprintf(stderr, "Error allocating memory\n");
                    exit(1);
                }
            }
        }

        // the oldest values are zero/unset if the buffer hasn't been filled yet, so we check prior to doing a
        // push/pop, to know if the values that are returned will be relevant to our calculations
        bool was_full = is_full(bd_quat_stage_1_buffer[0]);
        float stage_1_quat_w = push(bd_quat_stage_1_buffer[0], quat.w);
        float stage_1_quat_x = push(bd_quat_stage_1_buffer[1], quat.x);
        float stage_1_quat_y = push(bd_quat_stage_1_buffer[2], quat.y);
        float stage_1_quat_z = push(bd_quat_stage_1_buffer[3], quat.z);

        // TODO - timestamp_ms can only get as large as 2^24 before it starts to lose precision as a float,
        //        which is less than 5 hours of usage. Update this to just send two delta times, t0-t1 and t1-t2.
        float stage_1_ts = push(bd_quat_stage_1_buffer[4], (float)timestamp_ms);

        if (was_full) {
            was_full = is_full(bd_quat_stage_2_buffer[0]);
            float stage_2_quat_w = push(bd_quat_stage_2_buffer[0], stage_1_quat_w);
            float stage_2_quat_x = push(bd_quat_stage_2_buffer[1], stage_1_quat_x);
            float stage_2_quat_y = push(bd_quat_stage_2_buffer[2], stage_1_quat_y);
            float stage_2_quat_z = push(bd_quat_stage_2_buffer[3], stage_1_quat_z);
            float stage_2_ts = push(bd_quat_stage_2_buffer[4], stage_1_ts);

            if (was_full) {
                // TODO - locking
                // pthread_mutex_lock(imu_data_mutex);

                // write to shared memory for anyone using the same ipc prefix to consume
                float imu_data[NUM_IMU_VALUES];
                imu_data[0] = quat.x;
                imu_data[1] = quat.y;
                imu_data[2] = quat.z;
                imu_data[3] = quat.w;
                imu_data[4] = stage_1_quat_x;
                imu_data[5] = stage_1_quat_y;
                imu_data[6] = stage_1_quat_z;
                imu_data[7] = stage_1_quat_w;
                imu_data[8] = stage_2_quat_x;
                imu_data[9] = stage_2_quat_y;
                imu_data[10] = stage_2_quat_z;
                imu_data[11] = stage_2_quat_w;
                imu_data[12] = (float)timestamp_ms;
                imu_data[13] = stage_1_ts;
                imu_data[14] = stage_2_ts;

                breezy_desktop_write_imu_data(imu_data);

                // pthread_mutex_unlock(imu_data_mutex);
            }
        }
    }
}

const plugin_type breezy_desktop_plugin = {
    .id = "breezy_desktop",
    .default_config = breezy_desktop_default_config_func,
    .handle_config_line = breezy_desktop_handle_config_line_func,
    .set_config = breezy_desktop_set_config_func,
    .handle_imu_data = breezy_desktop_handle_imu_data_func,
    .reset_imu_data = breezy_desktop_reset_imu_data_func,

    // just rewrite the config values whenever anything changes
    .handle_state = write_config_data,
    .handle_device_disconnect = write_config_data
};