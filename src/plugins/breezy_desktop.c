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

#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>

#define BREEZY_DESKTOP_FD_RESET -2

const char* shared_mem_directory = "/dev/shm";
const char* shared_mem_filename = "breezy_desktop_imu";
const int breezy_desktop_feature_count = 1;
static bool has_started = false;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

static int fd = BREEZY_DESKTOP_FD_RESET;
static pthread_once_t shared_mem_path_once = PTHREAD_ONCE_INIT;

#define NUM_ORIENTATION_VALUES 16
float ORIENTATION_RESET[NUM_ORIENTATION_VALUES] = {
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 0.0
};

#define NUM_POSITION_VALUES 3
float POSITION_RESET[NUM_POSITION_VALUES] = {
    0.0, 0.0, 0.0
};

breezy_desktop_config *bd_config;

void breezy_desktop_reset_config(breezy_desktop_config *config) {
    config->enabled = false;
    config->look_ahead_override = 0.0;
    config->display_distance = 1.0;
    config->display_size = 1.0;
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
        temp_config->enabled = list_string_contains("breezy_desktop", value) && is_productivity_granted();
    } else if (equal(key, "display_distance")) {
        float_config(key, value, &temp_config->display_distance);
    } else if (equal(key, "display_size")) {
        float_config(key, value, &temp_config->display_size);
    } else if (equal(key, "sbs_content")) {
        boolean_config(key, value, &temp_config->sbs_content);
    } else if (equal(key, "sbs_mode_stretched")) {
        boolean_config(key, value, &temp_config->sbs_mode_stretched);
    }
};

const uint8_t DATA_LAYOUT_VERSION = 5;
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

const int IMU_RECORD_SIZE =
    sizeof(uint8_t) + // smooth_follow_enabled
    sizeof(float) * NUM_ORIENTATION_VALUES + // smooth_follow_origin (4 quaternion rows, 4 values each)
    sizeof(float) * NUM_POSITION_VALUES + // pose_position
    sizeof(uint64_t) + // imu_date_ms
    sizeof(float) * NUM_ORIENTATION_VALUES + // pose_orientation (4 quaternion rows, 4 values each)
    sizeof(uint8_t); // imu_parity_byte

uint64_t last_config_write_ts = 0;

// https://stackoverflow.com/a/12340725
static int fd_is_valid(int fd) {
    return fd >= 0 && (fcntl(fd, F_GETFD) != -1 || errno != EBADF);
}

static char* shared_mem_file_path = NULL;
static void init_shared_mem_file_path() {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/%s", shared_mem_directory, shared_mem_filename);
    shared_mem_file_path = strdup(buf);
}
static char* get_shared_mem_file_path_once() {
    pthread_once(&shared_mem_path_once, init_shared_mem_file_path);
    return shared_mem_file_path;
}
static int full_write(int wfd, const void* buf, size_t len) {
    const uint8_t *p = (const uint8_t*)buf;
    while (len) {
        ssize_t n = write(wfd, p, len);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) { errno = EIO; return -1; }
        p += (size_t)n; len -= (size_t)n;
    }
    return 0;
}

char* get_shared_mem_file_path() { return get_shared_mem_file_path_once(); }

static off_t expected_file_size() { return (off_t)(CONFIG_DATA_END_OFFSET + IMU_RECORD_SIZE); }
static int create_or_open_shared_mem_file() {
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
    char* path = get_shared_mem_file_path();
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", shared_mem_filename);
    int new_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (new_fd == -1) {
        new_fd = open(path, O_RDWR | O_CLOEXEC | O_CREAT | O_NOFOLLOW, 0644);
        if (new_fd == -1) { log_error("breezy_desktop: open failed %s: %s\n", path, strerror(errno)); return -1; }
    }
    struct stat st; if (fstat(new_fd, &st) == -1) { log_error("breezy_desktop: fstat failed: %s\n", strerror(errno)); close(new_fd); return -1; }
    off_t want = expected_file_size();
    if (st.st_size != want) {
        if (ftruncate(new_fd, want) == -1) { log_error("breezy_desktop: ftruncate(%lld) failed: %s\n", (long long)want, strerror(errno)); close(new_fd); return -1; }
        if (lseek(new_fd, 0, SEEK_SET) != -1) {
            size_t sz = (size_t)want; uint8_t *zero = calloc(1, sz);
            if (zero) { if (full_write(new_fd, zero, sz) == -1) log_error("breezy_desktop: zero init write failed: %s\n", strerror(errno)); free(zero);} else log_error("breezy_desktop: calloc zero init failed\n");
        }
    }
    return new_fd;
}
static int get_shared_mem_fd() {
    if (!fd_is_valid(fd)) fd = BREEZY_DESKTOP_FD_RESET;
    if (fd == BREEZY_DESKTOP_FD_RESET) {
        int new_fd = create_or_open_shared_mem_file();
        if (new_fd != -1) fd = new_fd;
    }
    return fd;
}

void do_write_config_data(int target_fd) {
    if (!bd_config) bd_config = breezy_desktop_default_config_func();
    if (lseek(target_fd, 0, SEEK_SET) == -1) {
        int first_err = errno;
        if (first_err == EBADF) {
            // Attempt single reopen and retry
            fd = BREEZY_DESKTOP_FD_RESET;
            int new_fd = get_shared_mem_fd();
            if (new_fd != -1) {
                target_fd = new_fd;
                if (lseek(target_fd, 0, SEEK_SET) == -1) {
                    log_error("lseek config retry failed: %s\n", strerror(errno));
                    goto error;
                }
            } else {
                log_error("reopen after EBADF failed: %s\n", strerror(first_err));
                goto error;
            }
        } else {
            log_error("lseek config: %s\n", strerror(first_err));
            goto error;
        }
    }
    uint8_t enabled = BOOL_FALSE;
    if (full_write(target_fd, &DATA_LAYOUT_VERSION, sizeof(uint8_t)) == -1) goto error;
    device_properties_type* device = device_checkout();
    if (device) {
        enabled = (!config()->disabled && bd_config->enabled) ? BOOL_TRUE : BOOL_FALSE;
        float look_ahead_cfg[4] = { device->look_ahead_constant, device->look_ahead_frametime_multiplier, device->look_ahead_scanline_adjust, device->look_ahead_ms_cap };
        int display_res[2] = { device->resolution_w, device->resolution_h };
        uint8_t sbs_enabled = state()->sbs_mode_enabled ? BOOL_TRUE : BOOL_FALSE;
        uint8_t custom_banner_enabled = (custom_banner_ipc_values && custom_banner_ipc_values->enabled && *custom_banner_ipc_values->enabled) ? BOOL_TRUE : BOOL_FALSE;
        if (full_write(target_fd, &enabled, sizeof(uint8_t)) == -1) goto error;
        if (full_write(target_fd, look_ahead_cfg, sizeof(float) * 4) == -1) goto error;
        if (full_write(target_fd, display_res, sizeof(uint32_t) * 2) == -1) goto error;
        if (full_write(target_fd, &device->fov, sizeof(float)) == -1) goto error;
        if (full_write(target_fd, &device->lens_distance_ratio, sizeof(float)) == -1) goto error;
        if (full_write(target_fd, &sbs_enabled, sizeof(uint8_t)) == -1) goto error;
        if (full_write(target_fd, &custom_banner_enabled, sizeof(uint8_t)) == -1) goto error;
    } else {
        if (full_write(target_fd, &enabled, sizeof(uint8_t)) == -1) goto error;
        const int remaining = CONFIG_DATA_END_OFFSET - (int)(sizeof(uint8_t) * 2);
        uint8_t *zero_data = calloc(1, remaining);
        if (!zero_data) { log_error("breezy_desktop: calloc zero config failed\n"); goto error; }
        if (full_write(target_fd, zero_data, remaining) == -1) { free(zero_data); goto error; }
        free(zero_data);
    }
    device_checkin(device);
    last_config_write_ts = get_epoch_time_ms();
    return;
error:
    log_error("breezy_desktop: config write failed: %s\n", strerror(errno));
    fd = BREEZY_DESKTOP_FD_RESET;
}

void write_config_data() {
    if (fd_is_valid(fd) || (is_productivity_granted() && bd_config && bd_config->enabled)) {
        pthread_mutex_lock(&file_mutex);
        if (!fd_is_valid(fd)) (void)get_shared_mem_fd();
        if (fd_is_valid(fd)) do_write_config_data(fd);
        pthread_mutex_unlock(&file_mutex);
    }
}

void breezy_desktop_write_pose_data(float *orientation, float *position) {
    pthread_mutex_lock(&file_mutex);
    int wfd = get_shared_mem_fd();
    if (wfd != -1) {
        uint64_t epoch_ms = get_epoch_time_ms();
        if (last_config_write_ts == 0 || epoch_ms - last_config_write_ts > 250) {
            do_write_config_data(wfd);
            if (fd == BREEZY_DESKTOP_FD_RESET) { pthread_mutex_unlock(&file_mutex); return; }
        }
        if (lseek(wfd, CONFIG_DATA_END_OFFSET, SEEK_SET) == -1) {
            log_error("breezy_desktop: lseek imu: %s\n", strerror(errno)); goto imu_error; }
        uint8_t smooth_follow_enabled = state()->breezy_desktop_smooth_follow_enabled ? BOOL_TRUE : BOOL_FALSE;
        if (full_write(wfd, &smooth_follow_enabled, sizeof(uint8_t)) == -1) goto imu_error;
        if (state()->smooth_follow_origin_ready && state()->smooth_follow_origin) {
            if (full_write(wfd, state()->smooth_follow_origin, sizeof(float) * NUM_ORIENTATION_VALUES) == -1) goto imu_error;
        } else if (full_write(wfd, orientation, sizeof(float) * NUM_ORIENTATION_VALUES) == -1) goto imu_error;
        if (full_write(wfd, position, sizeof(float) * NUM_POSITION_VALUES) == -1) goto imu_error;
        if (full_write(wfd, &epoch_ms, sizeof(uint64_t)) == -1) goto imu_error;
        if (full_write(wfd, orientation, sizeof(float) * NUM_ORIENTATION_VALUES) == -1) goto imu_error;
        uint8_t parity = 0; uint8_t* d = (uint8_t*)&epoch_ms; for (size_t i=0;i<sizeof(uint64_t);++i) parity ^= d[i];
        d = (uint8_t*)orientation; for (size_t i=0;i<sizeof(float)*NUM_ORIENTATION_VALUES;++i) parity ^= d[i];
        if (full_write(wfd, &parity, sizeof(uint8_t)) == -1) goto imu_error;
        pthread_mutex_unlock(&file_mutex); return;
    }
imu_error:
    if (errno) log_error("breezy_desktop: imu write failed: %s\n", strerror(errno));
    if (fd_is_valid(wfd)) { close(wfd); fd = BREEZY_DESKTOP_FD_RESET; }
    pthread_mutex_unlock(&file_mutex);
}

void breezy_desktop_reset_pose_data_func() {
    if (fd_is_valid(fd) || is_productivity_granted() && bd_config && bd_config->enabled) {
        breezy_desktop_write_pose_data(&ORIENTATION_RESET[0], &POSITION_RESET[0]);
    }
}

void breezy_desktop_set_config_func(void* new_config) {
    if (!new_config) return;
    
    breezy_desktop_config* temp_config = (breezy_desktop_config*) new_config;
    if (bd_config) {
        if (bd_config->enabled != temp_config->enabled)
            log_message("Breezy desktop has been %s\n", temp_config->enabled ? "enabled" : "disabled");
        free(bd_config);
    }
    bd_config = temp_config;
    if (has_started) {
        write_config_data();
        if (state()->calibration_state == CALIBRATING) breezy_desktop_reset_pose_data_func();
    }
};

void breezy_desktop_handle_pose_data_func(imu_pose_type pose, imu_euler_type velocities, bool imu_calibrated, ipc_values_type *ipc_values) {
    if (is_productivity_granted() && bd_config && bd_config->enabled) {
        if (imu_calibrated && ipc_values) {
            breezy_desktop_write_pose_data(ipc_values->pose_orientation, ipc_values->pose_position);
        } else {
            breezy_desktop_reset_pose_data_func();
        }
    }
}

void breezy_desktop_start_func() {
    pthread_mutex_init(&file_mutex, NULL);
}

void breezy_desktop_device_connect_func() {
    pthread_mutex_lock(&file_mutex);
    if (fd_is_valid(fd)) close(fd);
    fd = BREEZY_DESKTOP_FD_RESET;
    pthread_mutex_unlock(&file_mutex);
    has_started = true;
    write_config_data();
    breezy_desktop_reset_pose_data_func();
}

const plugin_type breezy_desktop_plugin = {
    .id = "breezy_desktop",
    .start = breezy_desktop_start_func,
    .default_config = breezy_desktop_default_config_func,
    .handle_config_line = breezy_desktop_handle_config_line_func,
    .set_config = breezy_desktop_set_config_func,
    .handle_pose_data = breezy_desktop_handle_pose_data_func,
    .reset_pose_data = breezy_desktop_reset_pose_data_func,
    .handle_device_disconnect = write_config_data,
    .handle_device_connect = breezy_desktop_device_connect_func
};