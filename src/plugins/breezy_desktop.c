#include "devices.h"
#include "features/breezy_desktop.h"
#include "logging.h"
#include "plugins.h"
#include "plugins/breezy_desktop.h"
#include "plugins/custom_banner.h"
#include "runtime_context.h"
#include "state.h"
#include "system.h"
#include "epoch.h"

#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>


#define BREEZY_DESKTOP_FD_RESET -2

const char* shared_mem_directory = "/dev/shm";
const char* shared_mem_filename = "breezy_desktop_imu";
const int breezy_desktop_feature_count = 1;
static bool has_started = false;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

static int fd = BREEZY_DESKTOP_FD_RESET;

#define NUM_IMU_VALUES 16
float IMU_RESET[NUM_IMU_VALUES] = {
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 0.0
};

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

const uint8_t DATA_LAYOUT_VERSION = 3;
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
    device_properties_type* device = device_checkout();
    if (device != NULL) {
        enabled = !config()->disabled && bd_config->enabled ? BOOL_TRUE : BOOL_FALSE;
        const float look_ahead_constant =   bd_config->look_ahead_override == 0 ?
                                                device->look_ahead_constant :
                                                bd_config->look_ahead_override;
        const float look_ahead_frametime_multiplier =   bd_config->look_ahead_override == 0 ?
                                                            device->look_ahead_frametime_multiplier : 0.0;
        float look_ahead_cfg[4] = {
            look_ahead_constant,
            look_ahead_frametime_multiplier,
            device->look_ahead_scanline_adjust,
            device->look_ahead_ms_cap
        };
        int display_res[2] = {
            device->resolution_w,
            device->resolution_h
        };
        uint8_t sbs_enabled = state()->sbs_mode_enabled ? BOOL_TRUE : BOOL_FALSE;
        uint8_t custom_banner_enabled = custom_banner_ipc_values && custom_banner_ipc_values->enabled && *custom_banner_ipc_values->enabled ? BOOL_TRUE : BOOL_FALSE;

        write(fd, &enabled, sizeof(uint8_t));
        write(fd, look_ahead_cfg, sizeof(float) * 4);
        write(fd, display_res, sizeof(uint32_t) * 2);
        write(fd, &device->fov, sizeof(float));
        write(fd, &device->lens_distance_ratio, sizeof(float));
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
    device_checkin(device);
    last_config_write_ts = get_epoch_time_ms();
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

// https://stackoverflow.com/a/12340725
static int fd_is_valid(int fd) {
    return fd >= 0 && (fcntl(fd, F_GETFD) != -1 || errno != EBADF);
}

static int get_shared_mem_fd() {
    if (!fd_is_valid(fd)) {
        fd = BREEZY_DESKTOP_FD_RESET;
    }

    if (fd == BREEZY_DESKTOP_FD_RESET) {
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
    // write this if the file exists, even if it's not actually enabled, so we can ensure that 
    // the file at least reflects the current state
    if (fd_is_valid(fd) || is_productivity_granted() && bd_config && bd_config->enabled) {
        pthread_mutex_lock(&file_mutex);
        do_write_config_data(fd);
        pthread_mutex_unlock(&file_mutex);
    }
}

void breezy_desktop_write_imu_data(float *values) {
    pthread_mutex_lock(&file_mutex);
    int fd = get_shared_mem_fd();
    if (fd != -1) {
        const uint64_t epoch_ms = get_epoch_time_ms();
        if (last_config_write_ts == 0 || epoch_ms - last_config_write_ts > 250) {
            // write this periodically, to ensure a recent heartbeat at the very least
            do_write_config_data(fd);
        }
        lseek(fd, CONFIG_DATA_END_OFFSET, SEEK_SET);
        write(fd, &epoch_ms, sizeof(uint64_t));
        write(fd, values, sizeof(float) * NUM_IMU_VALUES);

        // Calculate and write parity byte
        uint8_t parity = 0;
        uint8_t* data = (uint8_t*)&epoch_ms;
        for (size_t i = 0; i < sizeof(uint64_t); i++) {
            parity ^= data[i];
        }
        data = (uint8_t*)values;
        for (size_t i = 0; i < sizeof(float) * NUM_IMU_VALUES; i++) {
            parity ^= data[i];
        }
        write(fd, &parity, sizeof(uint8_t));
    }
    pthread_mutex_unlock(&file_mutex);
}

void breezy_desktop_reset_imu_data_func() {
    if (fd_is_valid(fd) || is_productivity_granted() && bd_config && bd_config->enabled) {
        breezy_desktop_write_imu_data(&IMU_RESET[0]);
    }
}

void breezy_desktop_set_config_func(void* config) {
    if (!config) return;
    breezy_desktop_config* temp_config = (breezy_desktop_config*) config;

    if (bd_config) {
        if (bd_config->enabled != temp_config->enabled)
            log_message("Breezy desktop has been %s\n", temp_config->enabled ? "enabled" : "disabled");

        free(bd_config);
    }
    bd_config = temp_config;

    if (has_started) {
        write_config_data();
        breezy_desktop_reset_imu_data_func();
    }
};

void breezy_desktop_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                                          bool imu_calibrated, ipc_values_type *ipc_values) {
    if (is_productivity_granted() && bd_config && bd_config->enabled) {
        if (imu_calibrated && ipc_values) {
            breezy_desktop_write_imu_data(ipc_values->imu_data);
        } else {
            breezy_desktop_reset_imu_data_func();
        }
    }
}

int breezy_desktop_register_features_func(char*** features) {
    *features = calloc(breezy_desktop_feature_count, sizeof(char*));
    (*features)[0] = strdup(productivity_basic_feature_name);

    return breezy_desktop_feature_count;
}

void breezy_desktop_start_func() {
    pthread_mutex_init(&file_mutex, NULL);
}

void breezy_desktop_device_connect_func() {
    // delete this first, in case it's left over from a previous run
    remove(get_shared_mem_file_path());
    fd = BREEZY_DESKTOP_FD_RESET;

    has_started = true;
    write_config_data();
    breezy_desktop_reset_imu_data_func();
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
    .handle_device_disconnect = write_config_data,
    .handle_device_connect = breezy_desktop_device_connect_func
};