#include "buffer.h"
#include "config.h"
#include "device.h"
#include "imu.h"
#include "files.h"
#include "ipc.h"
#include "multitap.h"
#include "outputs.h"
#include "state.h"
#include "strings.h"
#include "viture.h"
#include "xreal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUM_SUPPORTED_DEVICE_DRIVERS 2
const device_driver_type* device_drivers[NUM_SUPPORTED_DEVICE_DRIVERS] = {
    &xreal_driver,
    &viture_driver
};

#define EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define MT_RECENTER_SCREEN 2
#define MT_RESET_CALIBRATION 3

driver_config_type *config;
device_driver_type *device_driver;
device_properties_type *device;
ipc_values_type *ipc_values;

bool ipc_enabled = false;
bool glasses_ready=false;
bool glasses_calibrated=false;
long int glasses_calibration_started_sec=0;
bool force_reset_threads=false;
driver_state_type *state;
control_flags_type *control_flags;

bool captured_screen_center=false;
imu_quat_type screen_center;

void reset_calibration(bool reset_device) {
    glasses_calibration_started_sec=0;
    glasses_calibrated=false;
    captured_screen_center=false;
    control_flags->recalibrate=false;
    state->calibration_state = CALIBRATING;

    if (reset_device) {
        // trigger all threads to exit, this will cause the device thread to close the device and eventually re-open it
        force_reset_threads=true;
    } else if (ipc_enabled) printf("Waiting on device calibration\n");
}

void driver_handle_imu_event(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type euler) {
    if (device) {
        imu_euler_type euler_velocities = get_euler_velocities(device, euler);

        int multi_tap = detect_multi_tap(euler_velocities,
                                         timestamp_ms,
                                         config->debug_multi_tap);
        if (multi_tap == MT_RESET_CALIBRATION || control_flags->recalibrate) {
            if (multi_tap == MT_RESET_CALIBRATION) printf("Triple-tap detected. ");
            printf("Kicking off calibration\n");
            reset_calibration(true);
        }
        if (glasses_calibrated) {
            if (!captured_screen_center || multi_tap == MT_RECENTER_SCREEN || control_flags->recenter_screen) {
                if (multi_tap == MT_RECENTER_SCREEN) printf("Double-tap detected. ");
                printf("Centering screen\n");

                screen_center = quat;
                captured_screen_center=true;
                control_flags->recenter_screen=false;
            }
        } else {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            if (glasses_calibration_started_sec == 0) {
                glasses_calibration_started_sec=tv.tv_sec;
                if (ipc_enabled) reset_imu_data(ipc_values);
            } else {
                glasses_calibrated = (tv.tv_sec - glasses_calibration_started_sec) > device->calibration_wait_s;
                if (glasses_calibrated) {
                    state->calibration_state = CALIBRATED;
                    printf("Device calibration complete\n");
                }
            }
        }

        handle_imu_update(quat, euler_velocities, screen_center, ipc_enabled, glasses_calibrated, ipc_values, device, config);
    }
}

bool driver_device_should_disconnect() {
    return force_reset_threads;
}

bool driver_disabled() {
    return config->disabled;
}

// pthread function to create outputs and block on the device
void *block_on_device_thread_func(void *arg) {
    if (!config->disabled) {
        fprintf(stdout, "Device connected, redirecting input to %s...\n", config->output_mode);
        init_outputs(device, config);
    }

    device_driver->block_on_device_func();

    // glasses are probably still connected if the driver initiated the disconnect
    glasses_ready=driver_device_should_disconnect();
    if (!glasses_ready) free_and_clear(&state->connected_device_name);

    if (ipc_enabled) *ipc_values->disabled = true;
    deinit_outputs(config);

    if (config->debug_threads)
        printf("\tdebug: Exiting block_on_device thread; glasses_ready %d, driver_disabled %d, force_reset_threads: %d\n",
            glasses_ready, config->disabled, force_reset_threads);
}

void setup_ipc() {
    bool not_external_only = config->output_mode && !is_external_mode(config);
    if (!ipc_enabled) {
        if (config->debug_ipc) printf("\tdebug: setup_ipc, prefix set, enabling IPC\n");
        if (!ipc_values) ipc_values = malloc(sizeof(*ipc_values));
        if (setup_ipc_values(ipc_values, config->debug_ipc)) {
            ipc_enabled = true;

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

    if (ipc_enabled) {
        reset_imu_data(ipc_values);

        // set IPC values that won't change after a device is set
        ipc_values->display_res[0]        = device->resolution_w;
        ipc_values->display_res[1]        = device->resolution_h;
        *ipc_values->display_fov          = device->fov;
        *ipc_values->lens_distance_ratio  = device->lens_distance_ratio;
        *ipc_values->imu_data_period      = 1000.0 * (float)device->imu_buffer_size / device->imu_cycles_per_s;

        // always start out disabled, let it be explicitly enabled later
        *ipc_values->disabled             = true;

        // set defaults for everything else
        *ipc_values->display_zoom         = config->display_zoom;
        *ipc_values->display_north_offset = config->display_distance;
        ipc_values->look_ahead_cfg[0]     = config->look_ahead_override == 0 ?
                                                device->look_ahead_constant : config->look_ahead_override;
        ipc_values->look_ahead_cfg[1]     = config->look_ahead_override == 0 ?
                                                device->look_ahead_frametime_multiplier : 0.0;
        ipc_values->date[0]               = 0.0;
        ipc_values->date[1]               = 0.0;
        ipc_values->date[2]               = 0.0;
        ipc_values->date[3]               = 0.0;
    }
}

void update_config_from_file(FILE *fp) {
    driver_config_type* new_config = parse_config_file(fp);

    bool driver_disabled = !config->disabled && new_config->disabled;
    if (driver_disabled)
        printf("Driver has been disabled\n");
    bool driver_reenabled = config->disabled && !new_config->disabled;
    if (driver_reenabled)
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

    bool display_zoom_changed = config->display_zoom != new_config->display_zoom;
    if (display_zoom_changed)
        fprintf(stdout, "Display size has changed to %f\n", new_config->display_zoom);

    bool display_distance_changed = config->display_distance != new_config->display_distance;
    if (display_distance_changed)
        fprintf(stdout, "Display distance has changed to %f\n", new_config->display_distance);

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

    bool sbs_content_changed = config->sbs_content != new_config->sbs_content;
    if (sbs_content_changed)
        fprintf(stdout, "SBS content has been changed to %s\n", new_config->sbs_content ? "enabled" : "disabled");

    bool sbs_mode_changed = config->sbs_mode_stretched != new_config->sbs_mode_stretched;
    if (sbs_mode_changed)
        fprintf(stdout, "SBS mode has been changed to %s\n", new_config->sbs_mode_stretched ? "stretched" : "centered");

    update_config(&config, new_config);

    if (output_mode_changed || driver_reenabled || driver_disabled)
        // do this to teardown and re-initialize all outputs
        force_reset_threads = true;

    if (ipc_enabled) {
        *ipc_values->disabled = config->disabled;
        if (display_zoom_changed) *ipc_values->display_zoom = config->display_zoom;
        if (display_distance_changed) *ipc_values->display_north_offset = config->display_distance;
        if (sbs_content_changed) *ipc_values->sbs_content = config->sbs_content;
        if (sbs_mode_changed) *ipc_values->sbs_mode_stretched = config->sbs_mode_stretched;
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
char config_filename[1024];
FILE *config_fp;
void *monitor_config_file_thread_func(void *arg) {
    config_fp = freopen(config_filename, "r", config_fp);
    update_config_from_file(config_fp);

    int fd = inotify_init();
    if (fd < 0) {
        perror("Error initializing inotify");
        return NULL;
    }

    int wd = inotify_add_watch(fd, config_filename, IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB);
    if (wd < 0) {
        perror("Error adding watch");
        return NULL;
    }

    char buffer[EVENT_SIZE];

    // hold this pthread open while the glasses are plugged in, but if they become unplugged:
    // 1. hold this thread open as long as driver is disabled, this will block from re-initializing the glasses-polling thread until the driver becomes re-enabled
    // 2. exit this thread if the driver is enabled, then we'll wait for the glasses to get plugged back in to re-initialize these threads
    while (glasses_ready && !force_reset_threads) {
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
                    wd = inotify_add_watch(fd, config_filename, IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB);
                    if (wd < 0) {
                        perror("Error re-adding watch");
                        return NULL;
                    }
                } else {
                    config_fp = freopen(config_filename, "r", config_fp);
                    update_config_from_file(config_fp);
                }
                i += EVENT_SIZE + event->len;
            }
        }
    }

    if (config->debug_threads)
        printf("\tdebug: Exiting monitor_config_file thread; glasses_ready %d, driver_disabled %d, force_reset_threads: %d\n",
            glasses_ready, config->disabled, force_reset_threads);

    inotify_rm_watch(fd, wd);
    close(fd);
}

// pthread function to update the state and read control flags
void *manage_state_thread_func(void *arg) {
    struct timeval tv;
    while (glasses_ready && !force_reset_threads) {
        gettimeofday(&tv, NULL);
        state->heartbeat = tv.tv_sec;
        state->sbs_mode_enabled = device->sbs_mode_supported ? device_driver->device_is_sbs_mode_func() : false;
        update_state(state);

        if (ipc_enabled) {
            // this should reflect the real-world state, not the state requested by the control flag
            *ipc_values->sbs_enabled = state->sbs_mode_enabled;
        }

        read_control_flags(control_flags);
        if (device->sbs_mode_supported && control_flags->sbs_mode != SBS_CONTROL_UNSET) {
            if (!device_driver->device_set_sbs_mode_func(control_flags->sbs_mode == SBS_CONTROL_ENABLE)) {
                fprintf(stderr, "Error setting requested SBS mode\n");
            }
        }

        sleep(1);
    }

    // in case any state changed during the last sleep()
    update_state(state);

    if (config->debug_threads)
        printf("\tdebug: Exiting write_state thread; glasses_ready %d, driver_disabled %d, force_reset_threads: %d\n",
                                                               glasses_ready, config->disabled, force_reset_threads);
}

bool search_for_device() {
    for (int i = 0; i < NUM_SUPPORTED_DEVICE_DRIVERS; i++) {
        device_driver = device_drivers[i];
        device = device_driver->device_connect_func();
        if (device) {
            init_multi_tap(device->imu_cycles_per_s);
            copy_string(device->name, &state->connected_device_name);
            state->calibration_setup = device->calibration_setup;
            state->calibration_state = NOT_CALIBRATED;
            state->sbs_mode_supported = device->sbs_mode_supported;
            state->sbs_mode_enabled = device->sbs_mode_supported ? device_driver->device_is_sbs_mode_func() : false;

            return true;
        } else {
            device_driver = NULL;
        }
    }

    return false;
}

int main(int argc, const char** argv) {
    config = default_config();
    config_fp = get_or_create_home_file(".xreal_driver_config", "r", &config_filename[0], NULL);
    update_config_from_file(config_fp);

    state = malloc(sizeof(state));
    control_flags = malloc(sizeof(control_flags_type));
    read_control_flags(control_flags);

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

    while (1) {
        bool first_device_search_attempt = true;
        while (!search_for_device()) {
            if (first_device_search_attempt) {
                printf("Waiting for glasses\n");
                first_device_search_attempt = false;
            }

            // retry every second until a device becomes available
            sleep(1);
        }

        glasses_ready=true;
        setup_ipc();
        reset_calibration(false);
        if (ipc_enabled) *ipc_values->disabled = false;

        if (config->debug_threads)
            printf("\tdebug: Kicking off all threads\n");

        // kick off threads to monitor glasses and config file, wait for both to finish (glasses disconnected)
        force_reset_threads = false;
        pthread_t device_thread;
        pthread_t monitor_config_file_thread;
        pthread_t manage_state_thread;
        pthread_create(&device_thread, NULL, block_on_device_thread_func, NULL);
        pthread_create(&monitor_config_file_thread, NULL, monitor_config_file_thread_func, NULL);
        pthread_create(&manage_state_thread, NULL, manage_state_thread_func, NULL);
        pthread_join(device_thread, NULL);
        pthread_join(manage_state_thread, NULL);
        pthread_join(monitor_config_file_thread, NULL);

        if (device) {
            free(device->name);
            free(device);
            device = NULL;
        }

        if (config->debug_threads)
            printf("\tdebug: All threads have exited, starting over\n");
    }

    return 0;
}
