#include "buffer.h"
#include "config.h"
#include "device.h"
#include "device3.h"
#include "files.h"
#include "ipc.h"
#include "outputs.h"
#include "strings.h"

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

buffer_type **quat_stage_1_buffer = NULL;
buffer_type **quat_stage_2_buffer = NULL;
struct libevdev* evdev;
struct libevdev_uinput* uinput;

// counter that resets every second, for triggering things that we don't want to do every cycle
int imu_counter = 0;

int joystick_debug_imu_cycles;
int prev_joystick_x = 0;
int prev_joystick_y = 0;

const int max_input = 1 << 16;
const int mid_input = 0;
const int min_input = -max_input;

float joystick_max_degrees;

#define GYRO_BUFFERS_COUNT 4 // quat values: x, y, z, w

static int evdev_check(char * function, int i) {
    if (i < 0) {
        printf("libevdev.%s: %s\n", function, strerror(-i));
        exit(1);
    }

    return i;
}

// returns an integer between -max_input and max_input, the magnitude of which is just the ratio of
// input_deg to max_input_deg
int joystick_value(float input_degrees, float max_input_degrees) {
  int value = round(input_degrees * max_input / max_input_degrees);
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
        fprintf(stderr, "joystick_debug: invalid row or column index: %d %d\n", row, col);
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

// debug visual joystick from bash: watch -n 0.1 cat ~/.xreal_joystick_debug
void joystick_debug(int old_joystick_x, int old_joystick_y, int new_joystick_x, int new_joystick_y) {
    int old_x = joystick_debug_val_to_line(old_joystick_x);
    int old_y = joystick_debug_val_to_line(old_joystick_y);
    int new_x = joystick_debug_val_to_line(new_joystick_x);
    int new_y = joystick_debug_val_to_line(new_joystick_y);

    if (old_x != new_x || old_y != new_y) {
        char full_path[1024];
        bool file_created = false;
        FILE *fp = get_or_create_home_file(".xreal_joystick_debug", "r+", &full_path[0], &file_created);
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
            if (fp == NULL) {
                return;
            }
        }

        if (fp != NULL) {
            char reset_char = ' ';
            if (old_x == JOYSTICK_DEBUG_LINES_MIDDLE && old_y == JOYSTICK_DEBUG_LINES_MIDDLE)
                reset_char = 'X';

            write_character_to_joystick_debug_file(fp, old_x, old_y, reset_char);
            rewind(fp);
            write_character_to_joystick_debug_file(fp, new_x, new_y, 'O');
            fclose(fp);
        }
    }
}

// Starting from degree 0, 180 and -180 are the same. If the previous value was 179 and the new value is -179,
// the diff is 2 (-179 is equivalent to 181). This function takes the diff and then adjusts it if it detects
// that we've crossed the +/-180 threshold.
float degree_delta(float prev, float next) {
    float delta = fmod(prev - next, 360);
    if (isnan(delta)) {
        printf("nan value");
        exit(1);
    }

    if (delta > 180) {
        return delta - 360;
    } else if (delta < -180) {
        return delta + 360;
    }

    return delta;
}

device3_vec3_type get_euler_deltas(device3_vec3_type euler) {
    static device3_vec3_type prev_euler;
    device3_vec3_type deltas = {
        .x=degree_delta(prev_euler.z, euler.z),
        .y=degree_delta(prev_euler.y, euler.y),
        .z=degree_delta(prev_euler.x, euler.x)
    };
    prev_euler = euler;

    return deltas;
}

device3_vec3_type get_euler_velocities(device_properties_type *device, device3_vec3_type euler_deltas) {
    device3_vec3_type velocities = {
        .x=euler_deltas.x * device->imu_cycles_per_s,
        .y=euler_deltas.y * device->imu_cycles_per_s,
        .z=euler_deltas.z * device->imu_cycles_per_s
    };

    return velocities;
}

void init_outputs(device_properties_type *device, driver_config_type *config) {
    joystick_debug_imu_cycles = ceil(100.0 * device->imu_cycles_per_s / 1000.0); // update joystick debug file roughly every 100 ms
    joystick_max_degrees = 360.0 / device->imu_cycles_per_s / 4;
    float joystick_max_radians = joystick_max_degrees * M_PI / 180.0;

    evdev = libevdev_new();
    if (is_joystick_mode(config)) {
        struct input_absinfo absinfo;
        absinfo.minimum = min_input;
        absinfo.maximum = max_input;
        absinfo.resolution = max_input / joystick_max_radians;
        absinfo.value = mid_input;
        absinfo.flat = 2;
        absinfo.fuzz = 0;

        evdev_check("libevdev_enable_property", libevdev_enable_property(evdev, INPUT_PROP_BUTTONPAD));
        libevdev_set_name(evdev, "XREAL Air virtual joystick");
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
    } else if (is_mouse_mode(config)) {
        libevdev_set_name(evdev, "XREAL Air virtual mouse");

        evdev_check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_REL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_X, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_Y, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_WHEEL, NULL));

        evdev_check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_KEY));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_LEFT, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_MIDDLE, NULL));
        evdev_check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_RIGHT, NULL));
    }
    if (is_evdev_output_mode(config))
        evdev_check("libevdev_uinput_create_from_device", libevdev_uinput_create_from_device(evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput));
}

void deinit_outputs(driver_config_type *config) {
    if (uinput) libevdev_uinput_destroy(uinput);
    if (evdev) libevdev_free(evdev);
}

void handle_imu_update(device3_quat_type quat, device3_vec3_type euler_deltas, device3_quat_type screen_center,
                       bool ipc_enabled, bool send_imu_data, ipc_values_type *ipc_values, device_properties_type *device,
                       driver_config_type *config) {
    if (ipc_enabled) {
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

        if (send_imu_data) {
            if (quat_stage_1_buffer == NULL || quat_stage_2_buffer == NULL) {
                quat_stage_1_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
                quat_stage_2_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
                for (int i = 0; i < GYRO_BUFFERS_COUNT; i++) {
                    quat_stage_1_buffer[i] = create_buffer(device->imu_buffer_size);
                    quat_stage_2_buffer[i] = create_buffer(device->imu_buffer_size);
                    if (quat_stage_1_buffer[i] == NULL || quat_stage_2_buffer[i] == NULL) {
                        fprintf(stderr, "Error allocating memory\n");
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

            if (was_full) {
                was_full = is_full(quat_stage_2_buffer[0]);
                float stage_2_quat_w = push(quat_stage_2_buffer[0], stage_1_quat_w);
                float stage_2_quat_x = push(quat_stage_2_buffer[1], stage_1_quat_x);
                float stage_2_quat_y = push(quat_stage_2_buffer[2], stage_1_quat_y);
                float stage_2_quat_z = push(quat_stage_2_buffer[3], stage_1_quat_z);

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
                    ipc_values->imu_data[12] = screen_center.x;
                    ipc_values->imu_data[13] = screen_center.y;
                    ipc_values->imu_data[14] = screen_center.z;
                    ipc_values->imu_data[15] = screen_center.w;

                    pthread_mutex_unlock(ipc_values->imu_data_mutex);
                }
            }
        }
    }

    int next_joystick_x = joystick_value(euler_deltas.x, joystick_max_degrees);
    int next_joystick_y = joystick_value(euler_deltas.y, joystick_max_degrees);

    if (uinput) {
        if (is_joystick_mode(config)) {
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RX, next_joystick_x);
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RY, next_joystick_y);
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RZ, joystick_value(euler_deltas.z, joystick_max_degrees));
        } else if (is_mouse_mode(config)) {
            // keep track of the remainder (the amount that was lost with round()) for smoothing out mouse movements
            static float mouse_x_remainder = 0.0;
            static float mouse_y_remainder = 0.0;
            static float mouse_z_remainder = 0.0;

            // smooth out the mouse values using the remainders left over from previous writes
            float next_x = euler_deltas.x * config->mouse_sensitivity + mouse_x_remainder;
            int next_x_int = round(next_x);
            mouse_x_remainder = next_x - next_x_int;

            float next_y = euler_deltas.y * config->mouse_sensitivity + mouse_y_remainder;
            int next_y_int = round(next_y);
            mouse_y_remainder = next_y - next_y_int;

            float next_z = euler_deltas.z * config->mouse_sensitivity + mouse_z_remainder;
            int next_z_int = round(next_z);
            mouse_z_remainder = next_z - next_z_int;

            libevdev_uinput_write_event(uinput, EV_REL, REL_X, next_x_int);
            libevdev_uinput_write_event(uinput, EV_REL, REL_Y, next_y_int);
            if (config->use_roll_axis)
                libevdev_uinput_write_event(uinput, EV_REL, REL_Z, next_z_int);
        } else if (!is_external_mode(config)) {
            fprintf(stderr, "Unsupported output mode: %s\n", config->output_mode);
        }

        if (is_evdev_output_mode(config))
            libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
    }

    // always use joystick debugging as it adds a helpful visual
    if (config->debug_joystick && (imu_counter % joystick_debug_imu_cycles) == 0) {
        joystick_debug(prev_joystick_x, prev_joystick_y, next_joystick_x, next_joystick_y);
    }
    prev_joystick_x = next_joystick_x;
    prev_joystick_y = next_joystick_y;

    // reset the counter every second
    if ((++imu_counter % device->imu_cycles_per_s) == 0) {
        imu_counter = 0;
    }
}

void reset_imu_data(ipc_values_type *ipc_values) {
     // reset the 4 quaternion values to (0, 0, 0, 1)
     for (int i = 0; i < 16; i += 4) {
         ipc_values->imu_data[i] = 0;
         ipc_values->imu_data[i + 1] = 0;
         ipc_values->imu_data[i + 2] = 0;
         ipc_values->imu_data[i + 3] = 1;
     }
 }