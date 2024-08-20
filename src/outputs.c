#include "buffer.h"
#include "config.h"
#include "devices.h"
#include "imu.h"
#include "files.h"
#include "ipc.h"
#include "logging.h"
#include "outputs.h"
#include "plugins.h"
#include "runtime_context.h"
#include "strings.h"
#include "epoch.h"

#include <errno.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MS_PER_SEC 1000
#define IMU_CHECKPOINT_MS MS_PER_SEC / 4
#define GYRO_BUFFERS_COUNT 5 // quat values: x, y, z, w, timestamp

buffer_type **quat_stage_1_buffer;
buffer_type **quat_stage_2_buffer;

static int last_imu_checkpoint_ms = 0;
static imu_quat_type last_imu_checkpoint_quat = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};
static uint64_t last_healthy_imu_timestamp_ms = 0;

static pthread_mutex_t outputs_mutex = PTHREAD_MUTEX_INITIALIZER;
struct libevdev* evdev;
struct libevdev_uinput* uinput;

int joystick_debug_imu_cycles;
int prev_joystick_x = 0;
int prev_joystick_y = 0;

const int max_input = 1 << 16;
const int mid_input = 0;
const int min_input = -max_input;

float joystick_max_degrees_per_s;

static int evdev_check(char * function, int i) {
    if (i < 0) {
        log_message("libevdev.%s: %s\n", function, strerror(-i));
        exit(1);
    }

    return i;
}

// returns an integer between -max_input and max_input, the magnitude of which is just the ratio of
// input_velocity to max_input_velocity (where velocity is degrees/sec)
int joystick_value(float input_velocity, float max_input_velocity) {
  int value = round(input_velocity * max_input / max_input_velocity);
  if (value < min_input) {
    return min_input;
  } else if (value > max_input) {
    return max_input;
  }

  return value;
}

#define JOYSTICK_DEBUG_LINES 17
#define JOYSTICK_DEBUG_LINES_MIDDLE 8 // zero-indexed from 17 total lines

// converts a value in the joystick min/max range to a value in the file row/col range
int joystick_debug_val_to_line(int value) {
    int joystick_middle = (max_input + min_input) / 2;
    int value_from_middle = value - joystick_middle;
    float value_percent_of_total = (float)value_from_middle / (max_input - min_input);
    int line_value_from_middle = value_percent_of_total < 0 ? ceil(value_percent_of_total * JOYSTICK_DEBUG_LINES) : floor(value_percent_of_total * JOYSTICK_DEBUG_LINES);

    return JOYSTICK_DEBUG_LINES_MIDDLE + line_value_from_middle;
}

// write a character to a coordinate -- as a grid of characters -- in a file
void write_character_to_joystick_debug_file(FILE *fp, int col, int row, char new_char) {
    if (row < 0 || row >= JOYSTICK_DEBUG_LINES || col < 0 || col >= JOYSTICK_DEBUG_LINES) {
        log_error("joystick_debug: invalid row or column index: %d %d\n", row, col);
    } else {
        for (int i = 0; i < row; i++) {
            for (int j = 0; j < JOYSTICK_DEBUG_LINES; j++) {
                fgetc(fp);
            }
            char c = fgetc(fp);
            if (c != '\n') {
                return;
            }
        }

        for (int j = 0; j <= col; j++) {
            fgetc(fp);
        }

        fseek(fp, -1, SEEK_CUR);
        fputc(new_char, fp);
    }
}

// debug visual joystick from bash: watch -n 0.1 cat $XDG_RUNTIME_DIR/xr_driver/joystick_debug
void joystick_debug(int old_joystick_x, int old_joystick_y, int new_joystick_x, int new_joystick_y) {
    int old_x = joystick_debug_val_to_line(old_joystick_x);
    int old_y = joystick_debug_val_to_line(old_joystick_y);
    int new_x = joystick_debug_val_to_line(new_joystick_x);
    int new_y = joystick_debug_val_to_line(new_joystick_y);

    if (old_x != new_x || old_y != new_y) {
        char *full_path = NULL;
        bool file_created = false;
        FILE *fp = get_or_create_runtime_file("joystick_debug", "r+", &full_path, &file_created);
        if (file_created) {
            for (int i = 0; i < JOYSTICK_DEBUG_LINES; i++) {
                for (int j = 0; j < JOYSTICK_DEBUG_LINES; j++) {
                    char grid_char = ' ';
                    if (i == JOYSTICK_DEBUG_LINES_MIDDLE && j == JOYSTICK_DEBUG_LINES_MIDDLE)
                        grid_char = 'X';
                    fputc(grid_char, fp);
                }
                fputc('\n', fp);
            }
            fclose(fp);

            fp = fopen(full_path, "r+");
        }
        free_and_clear(&full_path);
        if (fp == NULL) {
            return;
        }

        char reset_char = ' ';
        if (old_x == JOYSTICK_DEBUG_LINES_MIDDLE && old_y == JOYSTICK_DEBUG_LINES_MIDDLE)
            reset_char = 'X';

        write_character_to_joystick_debug_file(fp, old_x, old_y, reset_char);
        rewind(fp);
        write_character_to_joystick_debug_file(fp, new_x, new_y, 'O');
        fclose(fp);
    }
}

// Starting from degree 0, 180 and -180 are the same. If the previous value was 179 and the new value is -179,
// the diff is 2 (-179 is equivalent to 181). This function takes the diff and then adjusts it if it detects
// that we've crossed the +/-180 threshold.
float degree_delta(float prev, float next) {
    float delta = fmod(next - prev, 360);
    if (delta > 180) {
        return delta - 360;
    } else if (delta < -180) {
        return delta + 360;
    }

    return delta;
}

imu_euler_type get_euler_velocities(imu_euler_type euler, int imu_cycles_per_sec) {
    static imu_euler_type prev_euler = {0.0f, 0.0f, 0.0f};
    imu_euler_type velocities = {
        .roll=degree_delta(prev_euler.roll, euler.roll) * imu_cycles_per_sec,
        .pitch=degree_delta(prev_euler.pitch, euler.pitch) * imu_cycles_per_sec,
        .yaw=degree_delta(prev_euler.yaw, euler.yaw) * imu_cycles_per_sec
    };

    prev_euler = euler;

    return velocities;
}

static void _init_outputs() {
    device_properties_type* device = device_checkout();
    joystick_debug_imu_cycles = device == NULL ? 6 : ceil(100.0 * device->imu_cycles_per_s / 1000.0); // update joystick debug file roughly every 100 ms
    joystick_max_degrees_per_s = 360.0 / 4;
    float joystick_max_radians_per_s = joystick_max_degrees_per_s * M_PI / 180.0;
    device_checkin(device);

    evdev = libevdev_new();
    if (is_joystick_mode(config())) {
        struct input_absinfo absinfo;
        absinfo.minimum = min_input;
        absinfo.maximum = max_input;
        absinfo.resolution = max_input / joystick_max_radians_per_s;
        absinfo.value = mid_input;
        absinfo.flat = 2;
        absinfo.fuzz = 0;

        evdev_check("libevdev_enable_property", libevdev_enable_property(evdev, INPUT_PROP_BUTTONPAD));
        libevdev_set_name(evdev, "XR virtual joystick");
        evdev_check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_ABS));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_X, &absinfo));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_Y, &absinfo));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_Z, &absinfo));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_RX, &absinfo));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_RY, &absinfo));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_RZ, &absinfo));

        /* do not remove next 3 lines or udev scripts won't assign 0664 permissions -sh */
        evdev_check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_KEY));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_JOYSTICK, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_TRIGGER, NULL));

        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_A, NULL));
    } else if (is_mouse_mode(config())) {
        libevdev_set_name(evdev, "XR virtual mouse");

        evdev_check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_REL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_X, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_Y, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_WHEEL, NULL));

        evdev_check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_KEY));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_LEFT, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_MIDDLE, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_RIGHT, NULL));
    }
    if (is_evdev_output_mode(config()))
        evdev_check("libevdev_uinput_create_from_device", libevdev_uinput_create_from_device(evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput));
}

static void _deinit_outputs() {
    last_imu_checkpoint_ms = 0;
    if (uinput) {
        libevdev_uinput_destroy(uinput);
        uinput = NULL;
    }
    if (evdev) {
        libevdev_free(evdev);
        evdev = NULL;
    }
}

void init_outputs() {
    pthread_mutex_lock(&outputs_mutex);
    _init_outputs();
    pthread_mutex_unlock(&outputs_mutex);
}

void deinit_outputs() {
    pthread_mutex_lock(&outputs_mutex);
    _deinit_outputs();
    pthread_mutex_unlock(&outputs_mutex);
}

void reinit_outputs() {
    pthread_mutex_lock(&outputs_mutex);
    _deinit_outputs();
    _init_outputs();
    pthread_mutex_unlock(&outputs_mutex);
}

#define WAIT_FOR_IMU_ATTEMPTS 5
bool wait_for_imu_start() {
    int attempts = 0;
    while (!is_imu_alive()) {
        if (attempts++ == WAIT_FOR_IMU_ATTEMPTS) return false;
        sleep(1);
    }

    return true;
}

void handle_imu_update(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type velocities,
                       bool imu_calibrated, ipc_values_type *ipc_values) {
    // counter that resets every second, for triggering things that we don't want to do every cycle
    static int imu_counter = 0;

    // periodically run checks to keep an eye on the health of the IMU
    if (timestamp_ms - last_imu_checkpoint_ms > IMU_CHECKPOINT_MS) {
        last_imu_checkpoint_ms = timestamp_ms;

        // in practice, no two quats will be exactly equal even if the glasses are stationary
        if (!quat_equal(quat, last_imu_checkpoint_quat)) {
            last_healthy_imu_timestamp_ms = get_epoch_time_ms();
            last_imu_checkpoint_quat = quat;
        } else if (config()->debug_device) {
            log_debug("handle_imu_update, device failed health check\n");
        }
    }

    device_properties_type* device = device_checkout();
    if (device != NULL) {
        pthread_mutex_lock(&outputs_mutex);
        if (ipc_values) {
            // send keepalive every counter period
            if (imu_counter == 0) {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);

                // match the float4 date uniform type definition
                ipc_values->date[0] = (float)(t->tm_year + 1900);
                ipc_values->date[1] = (float)(t->tm_mon + 1);
                ipc_values->date[2] = (float)t->tm_mday;
                ipc_values->date[3] = (float)(t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
            }

            if (imu_calibrated) {
                if (quat_stage_1_buffer == NULL || quat_stage_2_buffer == NULL) {
                    quat_stage_1_buffer = calloc(GYRO_BUFFERS_COUNT, sizeof(buffer_type*));
                    quat_stage_2_buffer = calloc(GYRO_BUFFERS_COUNT, sizeof(buffer_type*));
                    for (int i = 0; i < GYRO_BUFFERS_COUNT; i++) {
                        quat_stage_1_buffer[i] = create_buffer(device->imu_buffer_size);
                        quat_stage_2_buffer[i] = create_buffer(device->imu_buffer_size);
                        if (quat_stage_1_buffer[i] == NULL || quat_stage_2_buffer[i] == NULL) {
                            log_error("Error allocating memory\n");
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
                        pthread_mutex_lock(ipc_values->imu_data_mutex);

                        // write to shared memory for anyone using the same ipc prefix to consume
                        ipc_values->imu_data[0] = quat.x;
                        ipc_values->imu_data[1] = quat.y;
                        ipc_values->imu_data[2] = quat.z;
                        ipc_values->imu_data[3] = quat.w;
                        ipc_values->imu_data[4] = stage_1_quat_x;
                        ipc_values->imu_data[5] = stage_1_quat_y;
                        ipc_values->imu_data[6] = stage_1_quat_z;
                        ipc_values->imu_data[7] = stage_1_quat_w;
                        ipc_values->imu_data[8] = stage_2_quat_x;
                        ipc_values->imu_data[9] = stage_2_quat_y;
                        ipc_values->imu_data[10] = stage_2_quat_z;
                        ipc_values->imu_data[11] = stage_2_quat_w;
                        ipc_values->imu_data[12] = (float)timestamp_ms;
                        ipc_values->imu_data[13] = stage_1_ts;
                        ipc_values->imu_data[14] = stage_2_ts;

                        pthread_mutex_unlock(ipc_values->imu_data_mutex);
                    }
                }
            }
        }

        // tracking head movements in euler (roll, pitch, yaw) against 2d joystick/mouse (x,y) coordinates means that yaw
        // maps to horizontal movements (x) and pitch maps to vertical (y) movements. Because the euler values use a NWU
        // coordinate system, positive yaw/pitch values move left/down, respectively, and the mouse/joystick coordinate
        // systems are right-down, so a positive yaw should result in a negative x, and a positive pitch should result in a
        // positive y.
        int next_joystick_x = joystick_value(-velocities.yaw, joystick_max_degrees_per_s);
        int next_joystick_y = joystick_value(velocities.pitch, joystick_max_degrees_per_s);

        if (uinput) {
            if (is_joystick_mode(config())) {
                int next_joystick_z = joystick_value(-velocities.roll, joystick_max_degrees_per_s);
                libevdev_uinput_write_event(uinput, EV_ABS, ABS_RX, next_joystick_x);
                libevdev_uinput_write_event(uinput, EV_ABS, ABS_RY, next_joystick_y);
                if (config()->use_roll_axis)
                    libevdev_uinput_write_event(uinput, EV_ABS, ABS_RZ, next_joystick_z);
            } else if (is_mouse_mode(config())) {
                // keep track of the remainder (the amount that was lost with round()) for smoothing out mouse movements
                static float mouse_x_remainder = 0.0;
                static float mouse_y_remainder = 0.0;
                static float mouse_z_remainder = 0.0;

                // smooth out the mouse values using the remainders left over from previous writes
                float mouse_sensitivity_seconds = (float) config()->mouse_sensitivity / device->imu_cycles_per_s;
                float next_x = -velocities.yaw * mouse_sensitivity_seconds + mouse_x_remainder;
                int next_x_int = round(next_x);
                mouse_x_remainder = next_x - next_x_int;

                float next_y = velocities.pitch * mouse_sensitivity_seconds + mouse_y_remainder;
                int next_y_int = round(next_y);
                mouse_y_remainder = next_y - next_y_int;

                float next_z = -velocities.roll * mouse_sensitivity_seconds + mouse_z_remainder;
                int next_z_int = round(next_z);
                mouse_z_remainder = next_z - next_z_int;

                libevdev_uinput_write_event(uinput, EV_REL, REL_X, next_x_int);
                libevdev_uinput_write_event(uinput, EV_REL, REL_Y, next_y_int);
                if (config()->use_roll_axis)
                    libevdev_uinput_write_event(uinput, EV_REL, REL_Z, next_z_int);
            } else if (!is_external_mode(config())) {
                log_error("Unsupported output mode: %s\n", config()->output_mode);
            }

            if (is_evdev_output_mode(config()))
                libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
        }

        // always use joystick debugging as it adds a helpful visual
        if (config()->debug_joystick && (imu_counter % joystick_debug_imu_cycles) == 0)
            joystick_debug(prev_joystick_x, prev_joystick_y, next_joystick_x, next_joystick_y);

        prev_joystick_x = next_joystick_x;
        prev_joystick_y = next_joystick_y;

        plugins.handle_imu_data(timestamp_ms, quat, velocities, imu_calibrated, ipc_values);

        // reset the counter every second
        if ((++imu_counter % device->imu_cycles_per_s) == 0) {
            imu_counter = 0;
        }
        pthread_mutex_unlock(&outputs_mutex);
    }
    device_checkin(device);
}

void reset_imu_data(ipc_values_type *ipc_values) {
    if (ipc_values) {    
        pthread_mutex_lock(ipc_values->imu_data_mutex);
        // reset the 4 quaternion values to (0, 0, 0, 1)
        for (int i = 0; i < 16; i += 4) {
            ipc_values->imu_data[i] = 0;
            ipc_values->imu_data[i + 1] = 0;
            ipc_values->imu_data[i + 2] = 0;
            ipc_values->imu_data[i + 3] = 1;
        }
        pthread_mutex_unlock(ipc_values->imu_data_mutex);
    }

    plugins.reset_imu_data();
}

bool is_imu_alive() {
    return get_epoch_time_ms() - last_healthy_imu_timestamp_ms < MS_PER_SEC;
}