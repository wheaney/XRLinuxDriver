#include "buffer.h"
#include "device.h"
#include "plugins.h"
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


#define GYRO_BUFFERS_COUNT 5 // quat values: x, y, z, w, timestamp

buffer_type **bd_quat_stage_1_buffer;
buffer_type **bd_quat_stage_2_buffer;

FILE* get_shared_mem_file(char *mode, char *full_path) {
    snprintf(full_path, strlen(shared_mem_directory) + strlen(shared_mem_filename) + 2, "%s/%s", shared_mem_directory, shared_mem_filename);
    return fopen(full_path, mode ? mode : "r");
}

uint32_t getEpochSec() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec;
}

const uint8_t BOOL_TRUE = 1;
const uint8_t BOOL_FALSE = 0;
const float FLOAT_ZERO = 0.0;
const float DISPLAY_FOV = 46.0;
const float DISPLAY_ZOOM = 1.0;
const float DISPLAY_NORTH_OFFSET = 1.0;
const float LENS_DISTANCE_RATIO = 0.04;
const uint32_t DISPLAY_RES[2] = {1920, 1080};
const float LOOK_AHEAD_CFG[4] = {10.0, 1.25, 1.0, 30.0};

#define NUM_IMU_VALUES 16
void write_imu_data(float values[NUM_IMU_VALUES]) {
    char file_path[1024];
    FILE* fp = get_shared_mem_file("wb", &file_path[0]);
    if (fp == NULL) {
        printf("Error opening file %s: %s\n", file_path, strerror(errno));
        return;
    }
    uint32_t epoch = getEpochSec();

    // Move the file pointer to the desired offset (e.g., offset 100)
    fseek(fp, 0, SEEK_SET);
    fwrite(&BOOL_TRUE, sizeof(uint8_t), 1, fp); // version
    fwrite(&BOOL_TRUE, sizeof(uint8_t), 1, fp); // enabled
    fwrite(&epoch, sizeof(uint32_t), 1, fp); // epoch_sec
    fwrite(LOOK_AHEAD_CFG, sizeof(float), 4, fp); // look_ahead_cfg
    fwrite(DISPLAY_RES, sizeof(uint32_t), 2, fp); // display_res
    fwrite(&DISPLAY_FOV, sizeof(float), 1, fp); // display_fov
    fwrite(&DISPLAY_ZOOM, sizeof(float), 1, fp); // display_zoom
    fwrite(&FLOAT_ZERO, sizeof(float), 1, fp); // display_north_offset
    fwrite(&FLOAT_ZERO, sizeof(float), 1, fp); // lens_distance_ratio
    fwrite(&BOOL_FALSE, sizeof(uint8_t), 1, fp); // sbs_enabled
    fwrite(&BOOL_FALSE, sizeof(uint8_t), 1, fp); // sbs_content
    fwrite(&BOOL_FALSE, sizeof(uint8_t), 1, fp); // sbs_mode_stretched
    fwrite(&BOOL_FALSE, sizeof(uint8_t), 1, fp); // custom_banner_enabled
    fwrite(values, sizeof(float), NUM_IMU_VALUES, fp);

    fclose(fp);
}

void breezy_desktop_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                          bool ipc_enabled, bool imu_calibrated, ipc_values_type *ipc_values) {
    if (imu_calibrated) {
        if (bd_quat_stage_1_buffer == NULL || bd_quat_stage_2_buffer == NULL) {
            bd_quat_stage_1_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
            bd_quat_stage_2_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
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

                write_imu_data(imu_data);

                // pthread_mutex_unlock(imu_data_mutex);
            }
        }
    }
}

const plugin_type breezy_desktop_plugin = {
    .id = "breezy_desktop",
    .handle_imu_data = breezy_desktop_handle_imu_data_func
};