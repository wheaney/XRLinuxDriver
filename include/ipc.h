#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

// TODO - this is specific to the sombrero integration, either provide no default or move to a plug-in system where
//        the plug-in library would be expected to provide this default, if this functionality is used
extern const char *sombrero_ipc_file_prefix;

extern const char *imu_data_ipc_name;
extern const char *imu_data_mutex_ipc_name;
extern const char *imu_data_period_name;
extern const char *look_ahead_cfg_ipc_name;
extern const char *display_res_ipc_name;
extern const char *display_fov_ipc_name;
extern const char *lens_distance_ratio_ipc_name;
extern const char *zoom_ipc_name;
extern const char *disabled_ipc_name;
extern const char *date_ipc_name;

struct ipc_values_t {
    float *imu_data;
    pthread_mutex_t *imu_data_mutex;
    float *imu_data_period;
    float *look_ahead_cfg;
    unsigned int *display_res;
    float *display_fov;
    float *lens_distance_ratio;
    float *zoom;
    bool *disabled;
    float *date;
};

typedef struct ipc_values_t ipc_values_type;

bool setup_ipc_values(ipc_values_type *ipc_values, bool debug);

void setup_ipc_value(const char *name, void **shmemValue, size_t size, bool debug);

void cleanup_ipc(char* file_prefix, bool debug);