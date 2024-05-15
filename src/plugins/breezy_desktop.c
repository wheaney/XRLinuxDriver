#include "buffer.h"
#include "device.h"
#include "features/breezy_desktop.h"
#include "features/sbs.h"
#include "plugins.h"
#include "plugins/breezy_desktop.h"
#include "plugins/custom_banner.h"
#include "runtime_context.h"
#include "state.h"
#include "system.h"

#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>


const char* shared_mem_directory = "/dev/shm";
const char* shared_mem_filename = "breezy_desktop_imu";
const int breezy_desktop_feature_count = 1;
static bool has_started = false;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        temp_config->enabled = equal(value, "breezy_desktop") && is_productivity_granted();
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

uint64_t getEpochTimestampMS() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);

    long int sec_ms = ts.tv_sec * 1000;
    long int nsec_ms = ts.tv_nsec / 1000000;

    return (uint64_t)(sec_ms + nsec_ms);
}

const uint8_t DATA_LAYOUT_VERSION = 2;
#define BOOL_TRUE 1
#define BOOL_FALSE 0

// IMU data is written more frequently, so we need to know the offset in the file
const int CONFIG_DATA_END_OFFSET = 
    sizeof(uint8_t) + // version
    sizeof(uint8_t) + // enabled
    sizeof(float) * 4 + // look_ahead_cfg
    sizeof(uint32_t) * 2 + // display_res
    sizeof(float) + // fov
    sizeof(float) + // lens_distance_ratio
    sizeof(uint8_t) + // sbs_enabled
    sizeof(uint8_t); // custom_banner_enabled

uint64_t last_config_write_ts = 0;
void do_write_config_data(int fd) {
    if (!bd_config) bd_config = breezy_desktop_default_config_func();

    lseek(fd, 0, SEEK_SET);
    uint8_t enabled = BOOL_FALSE;
    write(fd, &DATA_LAYOUT_VERSION, sizeof(uint8_t));
    if (context.device) {
        enabled = !context.config->disabled && bd_config->enabled ? BOOL_TRUE : BOOL_FALSE;
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
        uint8_t sbs_enabled = context.state->sbs_mode_enabled && is_sbs_granted() ? BOOL_TRUE : BOOL_FALSE;
        uint8_t custom_banner_enabled = custom_banner_ipc_values && custom_banner_ipc_values->enabled && *custom_banner_ipc_values->enabled ? BOOL_TRUE : BOOL_FALSE;

        write(fd, &enabled, sizeof(uint8_t));
        write(fd, look_ahead_cfg, sizeof(float) * 4);
        write(fd, display_res, sizeof(uint32_t) * 2);
        write(fd, &context.device->fov, sizeof(float));
        write(fd, &context.device->lens_distance_ratio, sizeof(float));
        write(fd, &sbs_enabled, sizeof(uint8_t));
        write(fd, &custom_banner_enabled, sizeof(uint8_t));
    } else {
        write(fd, &enabled, sizeof(uint8_t));

        // we already wrote version and enabled flags
        const int remainingBytes = CONFIG_DATA_END_OFFSET - sizeof(uint8_t) * 2;

        uint8_t *zero_data = calloc(remainingBytes, 1);
        write(fd, zero_data, remainingBytes);
        free(zero_data);
    }
    last_config_write_ts = getEpochTimestampMS();
}

char* get_shared_mem_file_path() {
    static char* shared_mem_file_path = NULL;
    if (!shared_mem_file_path) {
        char file_path[1024];
        snprintf(file_path, strlen(shared_mem_directory) + strlen(shared_mem_filename) + 2, "%s/%s", shared_mem_directory, shared_mem_filename);

        shared_mem_file_path = strdup(file_path);
    }

    return shared_mem_file_path;
}

int get_shared_mem_fd() {
    static int fd = -2;

    if (fd == -2) {
        char* file_path = get_shared_mem_file_path();

        fd = open(file_path, O_WRONLY);
        if (fd == -1) {
            if (errno == ENOENT) {
                // File doesn't exist; create it
                fd = open(file_path, O_WRONLY | O_CREAT, 0644);
                if (fd == -1) {
                    perror("Error creating file");
                }
            } else {
                perror("Error opening file");
            }
        }
    }

    return fd;
}

void write_config_data() {
    pthread_mutex_lock(&file_mutex);
    int fd = get_shared_mem_fd();
    if (fd != -1) do_write_config_data(fd);
    pthread_mutex_unlock(&file_mutex);
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

    if (has_started) write_config_data();
};

void breezy_desktop_write_imu_data(float values[NUM_IMU_VALUES]) {
    pthread_mutex_lock(&file_mutex);
    int fd = get_shared_mem_fd();
    if (fd != -1) {
        const uint64_t epoch_ms = getEpochTimestampMS();
        if (last_config_write_ts == 0 || epoch_ms - last_config_write_ts > 250) {
            // write this periodically, to ensure a recent heartbeat at the very least
            do_write_config_data(fd);
        }
        lseek(fd, CONFIG_DATA_END_OFFSET, SEEK_SET);
        write(fd, &epoch_ms, sizeof(uint64_t));
        write(fd, values, sizeof(float) * NUM_IMU_VALUES);
    }
    pthread_mutex_unlock(&file_mutex);
}

void breezy_desktop_reset_imu_data_func() {
    breezy_desktop_write_imu_data(IMU_RESET);
}

// TODO - share this with virtual_display plugin
void breezy_desktop_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                          bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values) {
    if (is_productivity_granted() && bd_config && bd_config->enabled) {
        if (imu_calibrated) {
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
        } else {
            breezy_desktop_reset_imu_data_func();
        }
    }
}

int breezy_desktop_register_features_func(char*** features) {
    *features = calloc(breezy_desktop_feature_count, sizeof(char*));
    (*features)[0] = strdup(productivity_basic_feature_name);
    // (*features)[1] = strdup(productivity_pro_feature_name);

    return breezy_desktop_feature_count;
}

void breezy_desktop_start_func() {
    // delete this first, in case it's left over from a previous run
    remove(get_shared_mem_file_path());
    pthread_mutex_init(&file_mutex, NULL);

    has_started = true;
    breezy_desktop_write_imu_data(IMU_RESET);
}

const plugin_type breezy_desktop_plugin = {
    .id = "breezy_desktop",
    .start = breezy_desktop_start_func,
    .default_config = breezy_desktop_default_config_func,
    .handle_config_line = breezy_desktop_handle_config_line_func,
    .set_config = breezy_desktop_set_config_func,
    .register_features = breezy_desktop_register_features_func,
    .handle_imu_data = breezy_desktop_handle_imu_data_func,
    .reset_imu_data = breezy_desktop_reset_imu_data_func,

    // just rewrite the config values whenever anything changes
    .handle_state = write_config_data,
    .handle_device_disconnect = write_config_data
};