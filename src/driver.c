#include "buffer.h"
#include "driver.h"
#include "config.h"
#include "devices.h"
#include "devices/viture.h"
#include "devices/xreal.h"
#include "connection_pool.h"
#include "files.h"
#include "imu.h"
#include "ipc.h"
#include "logging.h"
#include "memory.h"
#include "multitap.h"
#include "ipc.h"
#include "outputs.h"
#include "plugins.h"
#include "plugins/gamescope_reshade_wayland.h"
#include "runtime_context.h"
#include "state.h"
#include "strings.h"
#include "system.h"

#include <dirent.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <netdb.h>
#include <limits.h>
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

#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define INOTIFY_EVENT_BUFFER_SIZE (1024 * INOTIFY_EVENT_SIZE)
#define MT_RECENTER_SCREEN 2
#define MT_RESET_CALIBRATION 3


ipc_values_type *ipc_values;

bool glasses_calibrated=false;
long int glasses_calibration_started_sec=0;
bool force_quit=false;
control_flags_type *control_flags;

bool captured_reference_pose=false;
imu_pose_type reference_pose;
imu_quat_type reference_orientation_conj;

static bool is_driver_connected() {
    return connection_pool_is_connected();
}

void reset_calibration(bool reset_device) {
    glasses_calibration_started_sec=0;
    glasses_calibrated=false;
    captured_reference_pose=false;
    control_flags->recalibrate=false;
    state()->calibration_state = CALIBRATING;

    if (reset_device && is_driver_connected()) {
        if (config()->debug_device) log_debug("reset_calibration, connection_pool_disconnect_all(true)\n");
        connection_pool_disconnect_all(true);
    } else log_message("Waiting on device calibration\n");
}

void driver_handle_pose_event(const char* driver_id, imu_pose_type pose) {
    // counter that resets every second, for triggering things that we don't want to do every cycle
    static int imu_counter = 0;
    static int multi_tap = 0;

    device_properties_type* device = device_checkout();
    if (is_driver_connected() && device != NULL) {
        if (config()->debug_device && imu_counter == 0 && pose.has_orientation)
            log_debug("driver_handle_imu_event - quat: %f %f %f %f; pos: %f %f %f\n", pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w, pose.position.x, pose.position.y, pose.position.z);
            
        if (glasses_calibrated) {
            if (!captured_reference_pose || multi_tap == MT_RECENTER_SCREEN || control_flags->recenter_screen) {
                if (multi_tap == MT_RECENTER_SCREEN) log_message("Double-tap detected.\n");
                log_message("Centering screen\n");

                if (pose.has_orientation) {
                    reference_pose.orientation = pose.orientation;
                    reference_pose.has_orientation = true;
                    reference_orientation_conj = conjugate(reference_pose.orientation);
                } else {
                    imu_quat_type tmp_screen_center = { .w = 1.0, .x = 0.0, .y = 0.0, .z = 0.0 };
                    reference_pose.orientation = tmp_screen_center;
                    reference_orientation_conj = tmp_screen_center;
                    reference_pose.has_orientation = false;
                }

                if (pose.has_position) {
                    reference_pose.position = pose.position;
                    reference_pose.has_position = true;
                } else {
                    reference_pose.position = (imu_vec3_type){0.0f, 0.0f, 0.0f};
                    reference_pose.has_position = false;
                }
                
                captured_reference_pose = true;
                control_flags->recenter_screen = false;
            } else {
                imu_pose_type current_pose = pose;
                if (current_pose.has_orientation) current_pose.euler = quaternion_to_euler_zyx(current_pose.orientation);
                bool pose_updated = plugins.modify_reference_pose(current_pose, &reference_pose);
                if (pose_updated) reference_orientation_conj = conjugate(reference_pose.orientation);
            }
        } else {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            if (glasses_calibration_started_sec == 0) {
                // defaults used for mouse/joystick while waiting on calibration
                imu_quat_type tmp_screen_center = { .w = 1.0, .x = 0.0, .y = 0.0, .z = 0.0 };
                reference_pose.orientation = tmp_screen_center;
                reference_orientation_conj = tmp_screen_center;
                reference_pose.position = (imu_vec3_type){0.0f, 0.0f, 0.0f};
                reference_pose.has_orientation = pose.has_orientation;
                reference_pose.has_position = pose.has_position;

                glasses_calibration_started_sec=tv.tv_sec;
                if (ipc_values) reset_pose_data(ipc_values);
            } else {
                glasses_calibrated = (tv.tv_sec - glasses_calibration_started_sec) > device->calibration_wait_s;
                if (glasses_calibrated) {
                    state()->calibration_state = CALIBRATED;
                    log_message("Device calibration complete\n");
                }
            }
        }

        // be resilient to bad values that may come from device drivers
        if (!isnan(pose.orientation.w)) {
            static imu_euler_type prev_unmodified_euler = {0.0f, 0.0f, 0.0f};

            if (pose.has_orientation) {
                pose.orientation = multiply_quaternions(reference_orientation_conj, pose.orientation);
                pose.euler = quaternion_to_euler_zyx(pose.orientation);
            }
            // interpret all positions relative to the reference orientation
            if (pose.has_position) {
                imu_vec3_type rel = {
                    .x = pose.position.x - reference_pose.position.x,
                    .y = pose.position.y - reference_pose.position.y,
                    .z = pose.position.z - reference_pose.position.z
                };
                pose.position = vector_rotate(rel, reference_orientation_conj);
            }

            imu_euler_type euler_velocities;
            bool velocities_set = false;
            
            if (config()->multi_tap_enabled) {
                euler_velocities = get_euler_velocities(&prev_unmodified_euler, pose.euler, device->imu_cycles_per_s);
                multi_tap = detect_multi_tap(euler_velocities, pose.timestamp_ms, config()->debug_multi_tap);
                velocities_set = true;
            }

            if (multi_tap == MT_RESET_CALIBRATION || control_flags->recalibrate) {
                if (multi_tap == MT_RESET_CALIBRATION) log_message("Triple-tap detected. ");
                log_message("Kicking off calibration\n");
                reset_calibration(true);
            }

            if (glasses_calibrated) {
                static imu_euler_type prev_modified_euler = {0.0f, 0.0f, 0.0f};
                plugins.modify_pose(&pose);

                // recompute velocities after pose modification, since outputs that use them
                // will want to be relative to the modified pose
                euler_velocities = get_euler_velocities(&prev_modified_euler, pose.euler, device->imu_cycles_per_s);
                velocities_set = true;
            }

            if (!velocities_set) {
                euler_velocities = get_euler_velocities(&prev_unmodified_euler, pose.euler, device->imu_cycles_per_s);
            }
            handle_imu_update_ext(pose, euler_velocities, glasses_calibrated, ipc_values);
        } else if (config()->debug_device) log_debug("driver_handle_imu_event, received invalid quat\n");

        // reset the counter every second
        if ((++imu_counter % device->imu_cycles_per_s) == 0) {
            imu_counter = 0;
        }
    }
    device_checkin(device);
}

bool driver_disabled() {
    return config()->disabled;
}

void setup_ipc() {
    if (!ipc_values) {
        if (config()->debug_ipc) log_debug("setup_ipc, enabling IPC\n");
        ipc_values = calloc(1, sizeof(*ipc_values));
        if (!setup_ipc_values(ipc_values, config()->debug_ipc) || !plugins.setup_ipc()) {
            log_error("Error setting up IPC values\n");
            exit(1);
        }
    } else if (config()->debug_ipc) log_debug("setup_ipc, already enabled, doing nothing\n");

    device_properties_type* device = device_checkout();
    if (device != NULL) {
        plugins.reset_pose_data();

        // set IPC values that won't change after a device is set
        ipc_values->display_res[0]        = (float) device->resolution_w;
        ipc_values->display_res[1]        = (float) device->resolution_h;

        // deprecated - can be removed once this version is widely distributed
        *ipc_values->display_fov          = device->fov;
        *ipc_values->lens_distance_ratio  = device->lens_distance_ratio;

        // always start out disabled, let it be explicitly enabled later
        *ipc_values->disabled             = true;

        // set defaults for everything else
        ipc_values->date[0]               = 0.0;
        ipc_values->date[1]               = 0.0;
        ipc_values->date[2]               = 0.0;
        ipc_values->date[3]               = 0.0;

        set_gamescope_reshade_effect_uniform_variable("display_resolution", ipc_values->display_res, 2, sizeof(float), false);
        set_gamescope_reshade_effect_uniform_variable("keepalive_date", ipc_values->date, 4, sizeof(float), false);
    }
    device_checkin(device);
}

pthread_mutex_t block_on_device_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t block_on_device_cond = PTHREAD_COND_INITIALIZER;
static bool block_on_device_ready = false;

// reevaluates the conditions that determine whether the block_on_device_thread function can be unblocked,
// we should call this whenever a condition changes that may effect the evaluation of block_on_device_ready
void evaluate_block_on_device_ready() {
    if (config()->debug_device) 
        log_debug("evaluate_block_on_device_ready, %s, %s, %s\n", 
            force_quit ? "force_quit" : "no force_quit", 
            driver_disabled() ? "driver_disabled" : "driver_enabled", 
            device_present() ? "device_present" : "no device_present");

    pthread_mutex_lock(&block_on_device_mutex);
    block_on_device_ready = force_quit || !driver_disabled() && device_present();
    if (block_on_device_ready) pthread_cond_signal(&block_on_device_cond);
    pthread_mutex_unlock(&block_on_device_mutex);
}

// pthread function to wait for a supported device, create outputs, and block on the device while it's connected
void *block_on_device_thread_func(void *arg) {
    while (!force_quit) {
        if (config()->debug_device) log_debug("block_on_device_thread, loop start\n");

        bool first_device_wait = true;
        pthread_mutex_lock(&block_on_device_mutex);
        while (!block_on_device_ready) {
            // if the only thing blocking this thread is the device not being ready, print a message
            if (!force_quit && !driver_disabled() && first_device_wait) {
                log_message("Waiting for glasses\n");
                first_device_wait = false;
            } else if (config()->debug_device) log_debug("block_on_device_thread, waiting on ready\n");
            pthread_cond_wait(&block_on_device_cond, &block_on_device_mutex);
        }
        pthread_mutex_unlock(&block_on_device_mutex);

        if (!force_quit) {
            if (config()->debug_device) log_debug("block_on_device_thread, connection_pool_connect_active()\n");

            if (connection_pool_connect_active()) {
                log_message("Device connected, redirecting input to %s...\n", config()->output_mode);

                setup_ipc();
                reset_calibration(false);
                *ipc_values->disabled = false;
                plugins.handle_device_connect();
                init_outputs();

                if (config()->debug_device) log_debug("block_on_device_thread, connection_pool_block_on_active()\n");
                connection_pool_block_on_active();

                plugins.handle_device_disconnect();
                deinit_outputs();
            } else if (block_on_device_ready) {
                log_message("Device driver connection attempt failed\n");
            }
            
            if (block_on_device_ready) {
                // device is still physically connected and will retry, pause for a moment
                log_message("Retrying driver connection in 1 second\n");
                sleep(1);
            }
        }

        if (ipc_values) *ipc_values->disabled = true;
    }

    if (config()->debug_threads)
        log_debug("Exiting block_on_device thread; force_quit %d\n", force_quit);
}

void update_config_from_file(FILE *fp) {
    driver_config_type* new_config = parse_config_file(fp);

    bool driver_newly_disabled = !driver_disabled() && new_config->disabled;
    if (driver_newly_disabled)
        log_message("Driver has been disabled\n");
    bool driver_reenabled = driver_disabled() && !new_config->disabled;
    if (driver_reenabled)
        log_message("Driver has been re-enabled\n");

    if (config()->vr_lite_invert_x != new_config->vr_lite_invert_x)
        log_message("VR-Lite invert X-axis has been %s\n", new_config->vr_lite_invert_x ? "enabled" : "disabled");
    if (config()->vr_lite_invert_y != new_config->vr_lite_invert_y)
        log_message("VR-Lite invert Y-axis has been %s\n", new_config->vr_lite_invert_y ? "enabled" : "disabled");

    if (!config()->use_roll_axis && new_config->use_roll_axis)
        log_message("VR-Lite roll axis has been enabled\n");
    if (config()->use_roll_axis && !new_config->use_roll_axis)
        log_message("VR-Lite roll axis has been disabled\n");

    if (config()->mouse_sensitivity != new_config->mouse_sensitivity)
        log_message("Mouse sensitivity has changed to %d\n", new_config->mouse_sensitivity);

    bool output_mode_changed = strcmp(config()->output_mode, new_config->output_mode) != 0;
    if (output_mode_changed)
        log_message("Output mode has been changed to '%s'\n", new_config->output_mode);

    if (config()->metrics_disabled != new_config->metrics_disabled)
        log_message("Metrics have been %s\n", new_config->metrics_disabled ? "disabled" : "enabled");

    if (!config()->debug_joystick && new_config->debug_joystick)
        log_message("Joystick debugging has been enabled, to see it, use 'watch -n 0.1 cat $XDG_RUNTIME_DIR/xr_driver/joystick_debug' in bash\n");
    if (config()->debug_joystick && !new_config->debug_joystick)
        log_message("Joystick debugging has been disabled\n");

    if (config()->debug_threads != new_config->debug_threads)
        log_message("Threads debugging has been %s\n", new_config->debug_threads ? "enabled" : "disabled");

    if (config()->debug_multi_tap != new_config->debug_multi_tap)
        log_message("Multi-tap debugging has been %s\n", new_config->debug_multi_tap ? "enabled" : "disabled");

    if (config()->debug_ipc != new_config->debug_ipc)
        log_message("IPC debugging has been %s\n", new_config->debug_ipc ? "enabled" : "disabled");

    if (config()->debug_license != new_config->debug_license)
        log_message("License debugging has been %s\n", new_config->debug_license ? "enabled" : "disabled");

    if (config()->debug_device != new_config->debug_device)
        log_message("Device debugging has been %s\n", new_config->debug_device ? "enabled" : "disabled");

    update_config(config(), new_config);

    if (config()->disabled && is_driver_connected()) {
        if (config()->debug_device) log_debug("update_config_from_file, connection_pool_disconnect_all(true)\n");
        connection_pool_disconnect_all(true);
    }
    if (driver_reenabled) plugins.start();

    if (output_mode_changed && is_driver_connected()) reinit_outputs();

    if (ipc_values) *ipc_values->disabled = driver_disabled();
    
    evaluate_block_on_device_ready();
}

// pthread function to monitor the config file for changes
char *config_filename = NULL;
FILE *config_fp;
void *monitor_config_file_thread_func(void *arg) {
    config_fp = freopen(config_filename, "r", config_fp);
    update_config_from_file(config_fp);

    int fd = inotify_init();
    if (fd < 0) {
        perror("Error initializing inotify");
        return NULL;
    }

    int wd = inotify_add_watch(fd, config_filename, IN_CLOSE_WRITE | IN_DELETE_SELF | IN_ATTRIB);
    if (wd < 0) {
        perror("Error adding watch");
        return NULL;
    }

    char inotify_event_buffer[INOTIFY_EVENT_BUFFER_SIZE];

    while (!force_quit) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        // wait for data to be available for reading, do this with select() so we can specify a timeout and make
        // sure we shouldn't exit due to force_quit
        int retval = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select()");
            exit(EXIT_FAILURE);
        } else if (retval) {
            int length = read(fd, inotify_event_buffer, INOTIFY_EVENT_BUFFER_SIZE);
            if (length < 0) {
                perror("Error reading inotify events");
                return NULL;
            }

            bool updated = false;
            int i = 0;
            while (i < length) {
                struct inotify_event *event = (struct inotify_event *) &inotify_event_buffer[i];
                if (event->mask & IN_DELETE_SELF) {
                    // The file has been deleted, so we need to re-add the watch
                    wd = inotify_add_watch(fd, config_filename, IN_CLOSE_WRITE | IN_DELETE_SELF | IN_ATTRIB);
                    if (wd < 0) {
                        perror("Error re-adding watch");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    updated = true;
                }
                i += INOTIFY_EVENT_SIZE + event->len;
            }
            if (ferror(config_fp) != 0 || feof(config_fp) != 0) {
                config_fp = freopen(config_filename, "r", config_fp);
                if (config_fp == NULL) {
                    perror("Error reopening config file");
                    exit(EXIT_FAILURE);
                }
                if (!updated) update_config_from_file(config_fp);
            }
            if (updated) update_config_from_file(config_fp);
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);

    if (config()->debug_threads)
        log_debug("Exiting monitor_config_file thread; force_quit: %d\n", force_quit);
}

// pthread function to update the state and read control flags
void *manage_state_thread_func(void *arg) {
    while (!force_quit) {
        device_properties_type* device = device_checkout();
        const device_driver_type* primary_drv_in_loop = connection_pool_primary_driver();
        update_state_from_device(state(), device, (device_driver_type*)primary_drv_in_loop);
        device_checkin(device);
        write_state(state());
        plugins.handle_state();

        sleep(1);
    }

    // in case any state changed during the last sleep()
    write_state(state());

    if (config()->debug_threads)
        log_debug("Exiting write_state thread; force_quit: %d\n", force_quit);
}

void handle_control_flags_update() {
    device_properties_type* device = device_checkout();
    if (is_driver_connected()) {
        if (device != NULL && device->sbs_mode_supported && control_flags->sbs_mode != SBS_CONTROL_UNSET) {
            // glasses can be sensitive to rapid mode changes, so only request a change if necessary
            bool requesting_enabled = control_flags->sbs_mode == SBS_CONTROL_ENABLE;
            bool is_already_enabled = connection_pool_device_is_sbs_mode();
            bool change_requested = is_already_enabled != requesting_enabled;

            if (change_requested && config()->debug_device) 
                log_debug("handle_control_flags_update, connection_pool_device_set_sbs_mode(%s)\n", requesting_enabled ? "true" : "false");

            if (change_requested && !connection_pool_device_set_sbs_mode(requesting_enabled)) {
                log_error("Error setting requested SBS mode\n");
            }
            control_flags->sbs_mode = SBS_CONTROL_UNSET;
        }
        if (control_flags->force_quit) {
            log_message("Force quit requested, exiting\n");
            force_quit = true;

            if (config()->debug_device) log_debug("handle_control_flags_update, connection_pool_disconnect_all(true)\n");
            connection_pool_disconnect_all(true);
            evaluate_block_on_device_ready();

            control_flags->force_quit = false;
        }
    }
    device_checkin(device);
}

// pthread function for watching control flags file
void *monitor_control_flags_file_thread_func(void *arg) {
    char *control_file_path = NULL;
    FILE* fp = get_driver_state_file(control_flags_filename, "r", &control_file_path);
    if (fp) {
        read_control_flags(fp, control_flags);
        write_state(state());
        handle_control_flags_update();

        fclose(fp);
        remove(control_file_path);
    }

    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return NULL;
    }

    int wd = inotify_add_watch(fd, state_files_directory, IN_CLOSE_WRITE);
    if (wd < 0) {
        perror("inotify_add_watch");
        close(fd);
        return NULL;
    }

    char inotify_event_buffer[INOTIFY_EVENT_BUFFER_SIZE];
    while (!force_quit) {
        int length = read(fd, inotify_event_buffer, INOTIFY_EVENT_BUFFER_SIZE);
        if (length < 0) {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &inotify_event_buffer[i];
            if ((event->mask & IN_CLOSE_WRITE) && strcmp(event->name, control_flags_filename) == 0) {
                fp = fopen(control_file_path, "r");
                if (fp) {
                    read_control_flags(fp, control_flags);
                    write_state(state());
                    handle_control_flags_update();

                    fclose(fp);
                    remove(control_file_path);
                }
            }
            i += INOTIFY_EVENT_SIZE + event->len;
        }
    }

    close(fd);
    free_and_clear(&control_file_path);

    if (config()->debug_threads)
        log_debug("Exiting monitor_control_flags_file_thread_func thread; force_quit: %d\n", force_quit);
}

void handle_device_connection_changed(bool is_added, connected_device_type* device_info) {
    // as long as we want a device to remain connected, we need to hold at least one checked out reference to it,
    // otherwise it will get freed prematurely. since this function manages device dis/connect events,
    // it has the responsibility of always holding open at least one reference as long as a device remains connected.
    static device_properties_type* primary_device_ref = NULL;

    if (is_added) {
        if (config()->debug_device) log_debug("device added for driver %s\n", device_info->driver->id);
        connection_pool_handle_device_added(device_info->driver, device_info->device);
        free(device_info);
    } else if (!is_added) {
        if (config()->debug_device) log_debug("device removed for driver %s\n", device_info->driver->id);
        connection_pool_handle_device_removed(device_info->driver->id);
        free(device_info);
    }

    // Reflect the pool's current primary in the runtime context
    device_properties_type* new_primary = connection_pool_primary_device();
    if (new_primary != primary_device_ref) {
        if (primary_device_ref) {
            // Release previous primary
            device_checkin(primary_device_ref);
            primary_device_ref = NULL;
            block_on_device_ready = false;
        }
        if (new_primary) {
            state()->calibration_state = NOT_CALIBRATED;
            set_device_and_checkout(new_primary);
            init_multi_tap(new_primary->imu_cycles_per_s);
            primary_device_ref = new_primary;
        }
    }

    const device_driver_type* primary_drv = connection_pool_primary_driver();
    update_state_from_device(state(), new_primary, (device_driver_type*)primary_drv);
}

void *monitor_usb_devices_thread_func(void *arg) {
    init_devices();
    while (!force_quit) {
        handle_device_connection_events();
        sleep(1);
    }

    if (config()->debug_threads)
        log_debug("Exiting monitor_usb_devices_thread_func thread; force_quit: %d\n", force_quit);
}

void segfault_handler(int sig) {
    (void)sig;
    log_error("Segmentation fault occurred\n");
    void *buffer[10];
    int nptrs = backtrace(buffer, 10);
    backtrace_symbols_fd(buffer, nptrs, 2);
    exit(EXIT_FAILURE);
}

int main(int argc, const char** argv) {
    signal(SIGSEGV, segfault_handler);

    log_init();

    // set a lock so only one instance of the driver can be running at a time
    char *lock_file_path = NULL;
    FILE *lock_file = get_or_create_runtime_file("lock.pid", "r", &lock_file_path, NULL);
    int rc = flock(fileno(lock_file), LOCK_EX | LOCK_NB);
    if(rc) {
        if(EWOULDBLOCK == errno)
            log_error("Another instance of this program is already running.\n");
        exit(1);
    }
    free_and_clear(&lock_file_path);

    connection_pool_init();
    set_config(default_config());
    set_state(calloc(1, sizeof(driver_state_type)));
    config_fp = get_or_create_config_file("config.ini", "r", &config_filename, NULL);
    update_config_from_file(config_fp);

    if (driver_disabled()) log_message("Driver is disabled\n");

    char** features = NULL;
    int feature_count = plugins.register_features(&features);
    state()->registered_features_count = feature_count;
    state()->registered_features = features;

    control_flags = calloc(1, sizeof(control_flags_type));
    control_flags->recenter_screen = false;
    control_flags->recalibrate = false;
    control_flags->force_quit = false;
    control_flags->sbs_mode = SBS_CONTROL_UNSET;

    plugins.start();
    write_state(state());
    set_on_device_change_callback(evaluate_block_on_device_ready);
    log_message("Starting up XR driver\n");

    pthread_t monitor_control_flags_file_thread;
    pthread_t monitor_config_file_thread;
    pthread_t manage_state_thread;
    pthread_t monitor_usb_devices_thread;
    pthread_t device_thread;
    pthread_create(&monitor_control_flags_file_thread, NULL, monitor_control_flags_file_thread_func, NULL);
    pthread_create(&monitor_config_file_thread, NULL, monitor_config_file_thread_func, NULL);
    pthread_create(&manage_state_thread, NULL, manage_state_thread_func, NULL);
    pthread_create(&monitor_usb_devices_thread, NULL, monitor_usb_devices_thread_func, NULL);
    pthread_create(&device_thread, NULL, block_on_device_thread_func, NULL);

    pthread_join(monitor_control_flags_file_thread, NULL);
    pthread_join(monitor_config_file_thread, NULL);
    pthread_join(manage_state_thread, NULL);
    pthread_join(monitor_usb_devices_thread, NULL);
    pthread_join(device_thread, NULL);

    return 0;
}
