#include "buffer.h"
#include "config.h"
#include "device.h"
#include "imu.h"
#include "files.h"
#include "ipc.h"
#include "multitap.h"
#include "outputs.h"
#include "strings.h"
#include "xreal.h"

#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <sys/inotify.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/time.h>

#define EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define MT_RECENTER_SCREEN 2
#define MT_RESET_CALIBRATION 3

driver_config_type *config;
device_properties_type *device;
ipc_values_type *ipc_values;

bool ipc_enabled = false;
bool glasses_ready=false;
bool glasses_calibrated=false;
long int glasses_calibration_started_sec=0;
bool force_reset_threads=false;

bool captured_screen_center=false;
imu_quat_type screen_center;

void reset_calibration(bool reset_device) {
    glasses_calibration_started_sec=0;
    glasses_calibrated=false;
    captured_screen_center=false;
    if (reset_device) {
        device->device_cleanup_func();
        glasses_ready=false;
    }

    if (glasses_ready && ipc_enabled) printf("Waiting on device calibration\n");
}

void handle_imu_event(uint32_t timestamp_ms, imu_quat_type quat, imu_vector_type euler) {
    imu_vector_type euler_deltas = get_euler_deltas(euler);
    imu_vector_type euler_velocities = get_euler_velocities(device, euler_deltas);

    int multi_tap = detect_multi_tap(euler_velocities,
                                     timestamp_ms,
                                     config->debug_multi_tap);
    if (multi_tap == MT_RESET_CALIBRATION) reset_calibration(true);
    if (ipc_enabled) {
        if (glasses_calibrated) {
            if (!captured_screen_center || multi_tap == MT_RECENTER_SCREEN) {
                if (captured_screen_center) printf("Double-tap detected, centering screen\n");

                screen_center = quat;
                captured_screen_center=true;
            }
        } else {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            if (glasses_calibration_started_sec == 0) {
                glasses_calibration_started_sec=tv.tv_sec;
                reset_imu_data(ipc_values);
            } else {
                glasses_calibrated = (tv.tv_sec - glasses_calibration_started_sec) > device->calibration_wait_s;
                if (glasses_calibrated) printf("Device calibration complete\n");
            }
        }
    }

    handle_imu_update(quat, euler_deltas, screen_center, ipc_enabled, glasses_calibrated, ipc_values, device, config);
}

bool should_disconnect_device() {
    return config->disabled || force_reset_threads;
}

// pthread function to create outputs and block on the device
void *block_on_device_thread_func(void *arg) {
    fprintf(stdout, "Device connected, redirecting input to %s...\n", config->output_mode);

    init_outputs(device, config);

    device->block_on_device_func(should_disconnect_device);

    glasses_ready=false;
    if (ipc_enabled) *ipc_values->disabled = true;
    deinit_outputs(config);

    if (config->debug_threads)
        printf("\tdebug: Exiting block_on_device thread\n");
}

// TODO - do this setup once from main() using the default ipc file prefix
void setup_ipc() {
    bool not_external_only = config->output_mode && !is_external_mode(config);
    if (!ipc_enabled) {
        if (config->debug_ipc) printf("\tdebug: setup_ipc, prefix set, enabling IPC\n");
        if (!ipc_values) ipc_values = malloc(sizeof(*ipc_values));
        if (setup_ipc_values(ipc_values, config->debug_ipc)) {
            ipc_enabled = true;

            reset_imu_data(ipc_values);

            // set IPC values that won't change
            // TODO - move this to a plug-in system, allow for adding different devices
            ipc_values->display_res[0]        = device->resolution_w;
            ipc_values->display_res[1]        = device->resolution_h;
            *ipc_values->display_fov          = device->fov;
            *ipc_values->lens_distance_ratio  = device->lens_distance_ratio;
            *ipc_values->imu_data_period      = 1000.0 * (float)device->imu_buffer_size / device->imu_cycles_per_s;

            // always start out disabled, let it be explicitly enabled later
            *ipc_values->disabled             = true;

            // set defaults for everything else
            *ipc_values->zoom                 = config->external_zoom;
            ipc_values->look_ahead_cfg[0]     = config->look_ahead_override == 0 ?
                                                    device->look_ahead_constant : config->look_ahead_override;
            ipc_values->look_ahead_cfg[1]     = config->look_ahead_override == 0 ?
                                                    device->look_ahead_frametime_multiplier : 0.0;
            ipc_values->date[0]               = 0.0;
            ipc_values->date[1]               = 0.0;
            ipc_values->date[2]               = 0.0;
            ipc_values->date[3]               = 0.0;

            if (not_external_only) {
                if (config->debug_ipc) printf("\tdebug: setup_ipc, mode is %s, disabling IPC\n", config->output_mode);
                ipc_enabled = false;
            } else {
                printf("IPC enabled\n");
            }
        } else {
            fprintf(stderr, "Error setting up IPC values\n");
            exit(1);
        }
    } else {
        if (not_external_only) {
            if (config->debug_ipc) printf("\tdebug: setup_ipc, mode is %s, disabling IPC\n", config->output_mode);
            *ipc_values->disabled = true;
            ipc_enabled = false;
        } else {
            if (config->debug_ipc) printf("\tdebug: setup_ipc, already enabled, doing nothing\n");
        }
    }
}

void parse_config_file(FILE *fp) {
    driver_config_type *new_config = default_config();

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (strcmp(key, "disabled") == 0) {
            new_config->disabled = strcmp(value, "true") == 0;
        } else if (strcmp(key, "debug") == 0) {
            char *token = strtok(value, ",");
            while (token != NULL) {
                if (strcmp(token, "joystick") == 0) {
                    new_config->debug_joystick = true;
                }
                if (strcmp(token, "taps") == 0) {
                    new_config->debug_multi_tap = true;
                }
                if (strcmp(token, "threads") == 0) {
                    new_config->debug_threads = true;
                }
                if (strcmp(token, "ipc") == 0) {
                    new_config->debug_ipc = true;
                }
                token = strtok(NULL, ",");
            }
        } else if (strcmp(key, "use_roll_axis") == 0) {
            new_config->use_roll_axis = true;
        } else if (strcmp(key, "mouse_sensitivity") == 0) {
            char *endptr;
            errno = 0;
            long num = strtol(value, &endptr, 10);
            if (errno != ERANGE && endptr != value) {
                new_config->mouse_sensitivity = (int) num;
            } else {
                fprintf(stderr, "Error parsing mouse_sensitivity value: %s\n", value);
            }
        } else if (strcmp(key, "look_ahead") == 0) {
            char *endptr;
            errno = 0;
            float num = strtof(value, &endptr);
            if (errno != ERANGE && endptr != value) {
                new_config->look_ahead_override = num;
            } else {
                fprintf(stderr, "Error parsing look_ahead value: %s\n", value);
            }
        } else if (strcmp(key, "external_zoom") == 0) {
            char *endptr;
            errno = 0;
            float num = strtof(value, &endptr);
            if (errno != ERANGE && endptr != value) {
                new_config->external_zoom = num;
            } else {
                fprintf(stderr, "Error parsing external_zoom value: %s\n", value);
            }
        } else if (strcmp(key, "output_mode") == 0) {
            copy_string(value, &new_config->output_mode, strlen(value) + 1);
        }
    }

    if (!config->disabled && new_config->disabled)
        printf("Driver has been disabled\n");
    if (config->disabled && !new_config->disabled)
        printf("Driver has been re-enabled\n");

    if (!config->use_roll_axis && new_config->use_roll_axis)
        printf("Roll axis has been enabled\n");
    if (config->use_roll_axis && !new_config->use_roll_axis)
        printf("Roll axis has been disabled\n");

    if (config->mouse_sensitivity != new_config->mouse_sensitivity)
        fprintf(stdout, "Mouse sensitivity has changed to %d\n", new_config->mouse_sensitivity);

    bool look_ahead_changed = config->look_ahead_override != new_config->look_ahead_override;
    if (look_ahead_changed)
        fprintf(stdout, "Look ahead override has changed to %f\n", new_config->look_ahead_override);

    bool external_zoom_changed = config->external_zoom != new_config->external_zoom;
    if (external_zoom_changed)
        fprintf(stdout, "External zoom has changed to %f\n", new_config->external_zoom);

    bool output_mode_changed = strcmp(config->output_mode, new_config->output_mode) != 0;
    if (output_mode_changed)
        printf("Output mode has been changed to '%s'\n", new_config->output_mode);

    if (!config->debug_joystick && new_config->debug_joystick)
        printf("Joystick debugging has been enabled, to see it, use 'watch -n 0.1 cat ~/.xreal_joystick_debug' in bash\n");
    if (config->debug_joystick && !new_config->debug_joystick)
        printf("Joystick debugging has been disabled\n");

    if (config->debug_threads != new_config->debug_threads)
        fprintf(stdout, "Threads debugging has been %s\n", new_config->debug_threads ? "enabled" : "disabled");

    if (config->debug_multi_tap != new_config->debug_multi_tap)
        fprintf(stdout, "Multi-tap debugging has been %s\n", new_config->debug_multi_tap ? "enabled" : "disabled");

    if (config->debug_ipc != new_config->debug_ipc)
        fprintf(stdout, "IPC debugging has been %s\n", new_config->debug_ipc ? "enabled" : "disabled");

    update_config(&config, new_config);

    if (output_mode_changed)
        // this will trigger another call to setup_ipc() before the next time the threads restart
        force_reset_threads = true;

    if (ipc_enabled) {
        *ipc_values->disabled = config->disabled;
        if (external_zoom_changed) *ipc_values->zoom = config->external_zoom;
        if (look_ahead_changed) {
            ipc_values->look_ahead_cfg[0] = config->look_ahead_override == 0 ?
                                                device->look_ahead_constant : config->look_ahead_override;
            ipc_values->look_ahead_cfg[1] = config->look_ahead_override == 0 ?
                                                device->look_ahead_frametime_multiplier : 0.0;
        }
    } else if (!force_reset_threads && is_external_mode(config)) {
        fprintf(stderr, "error: no IPC path set, IMU data will not be available for external usage\n");
    }
}

// pthread function to monitor the config file for changes
void *monitor_config_file_thread_func(void *arg) {
    char filename[1024];
    FILE *fp = get_or_create_home_file(".xreal_driver_config", "r", &filename[0], NULL);
    if (!fp)
        return NULL;

    parse_config_file(fp);

    int fd = inotify_init();
    if (fd < 0) {
        perror("Error initializing inotify");
        return NULL;
    }

    int wd = inotify_add_watch(fd, filename, IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB);
    if (wd < 0) {
        perror("Error adding watch");
        return NULL;
    }

    char buffer[EVENT_SIZE];

    // hold this pthread open while the glasses are plugged in, but if they become unplugged:
    // 1. hold this thread open as long as driver is disabled, this will block from re-initializing the glasses-polling thread until the driver becomes re-enabled
    // 2. exit this thread if the driver is enabled, then we'll wait for the glasses to get plugged back in to re-initialize these threads
    while ((glasses_ready || config->disabled) && !force_reset_threads) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        // wait for data to be available for reading, do this with select() so we can specify a timeout and make
        // sure we shouldn't exit due to config->disabled
        int retval = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select()");
            return NULL;
        } else if (retval) {
            int length = read(fd, buffer, EVENT_SIZE);
            if (length < 0) {
                perror("Error reading inotify events");
                return NULL;
            }

            int i = 0;
            while (i < length) {
                struct inotify_event *event = (struct inotify_event *) &buffer[i];
                if (event->mask & IN_DELETE_SELF) {
                    // The file has been deleted, so we need to re-add the watch
                    wd = inotify_add_watch(fd, filename, IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB);
                    if (wd < 0) {
                        perror("Error re-adding watch");
                        return NULL;
                    }
                } else {
                    fp = freopen(filename, "r", fp);
                    parse_config_file(fp);
                }
                i += EVENT_SIZE + event->len;
            }
        }
    }

    if (config->debug_threads)
        printf("\tdebug: Exiting monitor_config_file thread\n");

    fclose(fp);
    inotify_rm_watch(fd, wd);
    close(fd);
}

void set_device(const device_properties_type dev) {
    *device = dev;
    init_multi_tap(device->imu_cycles_per_s);
}

int main(int argc, const char** argv) {
    config = default_config();
    device = malloc(sizeof(device_properties_type));
    set_device(xreal_air_properties);

    // ensure the log file exists, reroute stdout and stderr there
    char log_file_path[1024];
    FILE *log_file = get_or_create_home_file(".xreal_driver_log", NULL, &log_file_path[0], NULL);
    fclose(log_file);
    freopen(log_file_path, "a", stdout);
    freopen(log_file_path, "a", stderr);

    // when redirecting stdout/stderr to a file, it becomes fully buffered, requiring lots of manual flushing of the
    // stream, this makes them unbuffered, which is fine since we log so little
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // set a lock so only one instance of the driver can be running at a time
    char lock_file_path[1024];
    FILE *lock_file = get_or_create_home_file(".xreal_driver_lock", "r", &lock_file_path[0], NULL);
    int rc = flock(fileno(lock_file), LOCK_EX | LOCK_NB);
    if(rc) {
        if(EWOULDBLOCK == errno)
            fprintf(stderr, "Another instance of this program is already running.\n");
        exit(1);
    }

    setup_ipc();


    while (1) {
        bool device_connected = device->device_connect_func(handle_imu_event);
        if (!device_connected)
            printf("Waiting for glasses\n");

        while (!device_connected) {
            // TODO - move to a blocking check, rather than polling for device availability
            // retry every 5 seconds until the device becomes available
            device->device_cleanup_func();
            sleep(5);
            device_connected = device->device_connect_func(handle_imu_event);
        }

        glasses_ready=true;
        setup_ipc();
        reset_calibration(false);
        if (ipc_enabled) *ipc_values->disabled = false;

        // kick off threads to monitor glasses and config file, wait for both to finish (glasses disconnected)
        pthread_t device_thread;
        pthread_t monitor_config_file_thread;
        pthread_create(&device_thread, NULL, block_on_device_thread_func, NULL);
        pthread_create(&monitor_config_file_thread, NULL, monitor_config_file_thread_func, NULL);
        pthread_join(device_thread, NULL);
        pthread_join(monitor_config_file_thread, NULL);

        if (config->debug_threads)
            printf("\tdebug: All threads have exited, starting over\n");

        force_reset_threads = false;
    }

    return 0;
}
