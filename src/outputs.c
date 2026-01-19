#include "buffer.h"
#include "config.h"
#include "devices.h"
#include "imu.h"
#include "files.h"
#include "ipc.h"
#include "logging.h"
#include "memory.h"
#include "outputs.h"
#include "plugins.h"
#include "plugins/gamescope_reshade_wayland.h"
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
#define IMU_DEAD_ZONE_STABILITY_MS 500


imu_buffer_type *imu_buffer;

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

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static imu_quat_type quat_slerp(imu_quat_type a, imu_quat_type b, float t) {
    t = clampf(t, 0.0f, 1.0f);

    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        dot = -dot;
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }

    // When the quaternions are very close, fall back to lerp to avoid numeric issues.
    if (dot > 0.9995f) {
        imu_quat_type out = {
            .x = a.x + (b.x - a.x) * t,
            .y = a.y + (b.y - a.y) * t,
            .z = a.z + (b.z - a.z) * t,
            .w = a.w + (b.w - a.w) * t,
        };
        float len = sqrtf(out.x * out.x + out.y * out.y + out.z * out.z + out.w * out.w);
        if (len > 0.0f) {
            out.x /= len;
            out.y /= len;
            out.z /= len;
            out.w /= len;
        }
        return out;
    }

    dot = clampf(dot, -1.0f, 1.0f);
    float theta_0 = acosf(dot);
    float sin_theta_0 = sinf(theta_0);
    if (sin_theta_0 <= 0.0f) {
        return b;
    }

    float theta = theta_0 * t;
    float sin_theta = sinf(theta);

    float s0 = cosf(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;

    imu_quat_type out = {
        .x = (s0 * a.x) + (s1 * b.x),
        .y = (s0 * a.y) + (s1 * b.y),
        .z = (s0 * a.z) + (s1 * b.z),
        .w = (s0 * a.w) + (s1 * b.w),
    };

    return out;
}

static float dead_zone_exponential_curve(float ratio01) {
    // Exponential curve with very low values near 0 and a smooth rise towards 1.
    // ratio01 is expected in [0, 1].
    const float k = 8.0f;
    ratio01 = clampf(ratio01, 0.0f, 1.0f);

    float e0 = expf(-k);
    float e = expf(k * (ratio01 - 1.0f));
    return (e - e0) / (1.0f - e0);
}

static float dead_zone_slerp_alpha(float angle_rad, float threshold_rad, int imu_cycles_per_s) {
    if (threshold_rad <= 0.0f) return 1.0f;
    if (imu_cycles_per_s <= 0) return 1.0f;

    float ratio = clampf(angle_rad / threshold_rad, 0.0f, 1.0f);
    float curve = dead_zone_exponential_curve(ratio);

    // Floor the alpha so extremely small deltas still converge over a few seconds.
    // Using a time constant keeps behavior stable across different IMU rates.
    const float tau_slow_s = 5.0f;
    float dt_s = 1.0f / (float)imu_cycles_per_s;
    float alpha_min = 1.0f - expf(-dt_s / tau_slow_s);
    float alpha = alpha_min + curve * (1.0f - alpha_min);
    return clampf(alpha, 0.0f, 1.0f);
}

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

imu_euler_type get_euler_velocities(imu_euler_type* previous, imu_euler_type current, int imu_cycles_per_sec) {
    imu_euler_type velocities = {
        .roll=degree_delta(previous->roll, current.roll) * imu_cycles_per_sec,
        .pitch=degree_delta(previous->pitch, current.pitch) * imu_cycles_per_sec,
        .yaw=degree_delta(previous->yaw, current.yaw) * imu_cycles_per_sec
    };

    *previous = current;

    return velocities;
}

static void _init_outputs() {
    device_properties_type* device = device_checkout();
    joystick_debug_imu_cycles = device == NULL ? 6 : ceil(100.0 * device->imu_cycles_per_s / 1000.0); // update joystick debug file roughly every 100 ms
    joystick_max_degrees_per_s = 360.0 / 4;
    float joystick_max_radians_per_s = joystick_max_degrees_per_s * M_PI / 180.0;
    device_checkin(device);

    evdev = libevdev_new();
    if (config()->joystick_mode) {
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
    } else if (config()->mouse_mode) {
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
    if (config()->mouse_mode || config()->joystick_mode)
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

void handle_imu_update(imu_pose_type pose, imu_euler_type velocities, bool imu_calibrated, ipc_values_type *ipc_values) {
    // counter that resets every second, for triggering things that we don't want to do every cycle
    static int imu_counter = 0;

    // periodically run checks to keep an eye on the health of the IMU
    if (pose.timestamp_ms - last_imu_checkpoint_ms > IMU_CHECKPOINT_MS) {
        last_imu_checkpoint_ms = pose.timestamp_ms;

        // in practice, no two quats will be exactly equal even if the glasses are stationary
        if (!quat_equal(pose.orientation, last_imu_checkpoint_quat)) {
            last_healthy_imu_timestamp_ms = get_epoch_time_ms();
            last_imu_checkpoint_quat = pose.orientation;
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
                set_skippable_gamescope_reshade_effect_uniform_variable("keepalive_date", ipc_values->date, 4, sizeof(float), true);
            }

            if (imu_calibrated) {
                if (imu_buffer != NULL && imu_buffer_size(imu_buffer) != device->imu_buffer_size) {
                    free_imu_buffer(imu_buffer);
                    imu_buffer = NULL;
                }

                if (imu_buffer == NULL) {
                    imu_buffer = create_imu_buffer(device->imu_buffer_size);
                    if (imu_buffer == NULL) {
                        log_error("Error allocating memory\n");
                        exit(1);
                    }
                }

                imu_buffer_response_type *response = push_to_imu_buffer(imu_buffer, pose.orientation, (float)pose.timestamp_ms);

                if (response && response->ready) {
                    // Deadzone smoothing (WIP): below the configured threshold, slerp towards the new quat.
                    // The closer the angle is to the threshold, the more aggressively we slerp (exponential curve).
                    // Past the threshold, we effectively "snap" (copy) to preserve responsiveness.
                    static bool dead_zone_initialized = false;
                    static imu_quat_type dead_zone_quat = {0};

                    static float dead_zone_threshold_deg = 0.0f;
                    static float dead_zone_threshold_rad = 0.0f;
                    if (config()->dead_zone_threshold_deg != dead_zone_threshold_deg) {
                        dead_zone_threshold_deg = config()->dead_zone_threshold_deg;
                        dead_zone_threshold_rad = degree_to_radian(dead_zone_threshold_deg);
                        dead_zone_initialized = false;

                        log_message("dead zone updated: threshold %.2f deg (%.4f rad), curve k=%.1f, tau_slow=%.1fs\n",
                                    dead_zone_threshold_deg, dead_zone_threshold_rad, 8.0f, 5.0f);
                    }

                    if (dead_zone_threshold_deg > 0.0f) {
                        imu_quat_type current_quat = {
                            .x = response->data[0],
                            .y = response->data[1],
                            .z = response->data[2],
                            .w = response->data[3],
                        };

                        if (!dead_zone_initialized) {
                            dead_zone_initialized = true;
                            dead_zone_quat = current_quat;
                        } else {
                            float angle_rad = quat_small_angle_rad(dead_zone_quat, current_quat);
                            if (angle_rad >= dead_zone_threshold_rad) {
                                dead_zone_quat = current_quat;
                            } else {
                                float alpha = dead_zone_slerp_alpha(angle_rad, dead_zone_threshold_rad, device->imu_cycles_per_s);
                                dead_zone_quat = quat_slerp(dead_zone_quat, current_quat, alpha);
                            }
                        }

                        // Overwrite quaternions with smoothed orientation (timestamps left unchanged).
                        response->data[0] = dead_zone_quat.x;
                        response->data[1] = dead_zone_quat.y;
                        response->data[2] = dead_zone_quat.z;
                        response->data[3] = dead_zone_quat.w;
                        response->data[4] = dead_zone_quat.x;
                        response->data[5] = dead_zone_quat.y;
                        response->data[6] = dead_zone_quat.z;
                        response->data[7] = dead_zone_quat.w;
                        response->data[8] = dead_zone_quat.x;
                        response->data[9] = dead_zone_quat.y;
                        response->data[10] = dead_zone_quat.z;
                        response->data[11] = dead_zone_quat.w;
                    }

                    pthread_mutex_lock(ipc_values->pose_orientation_mutex);

                    memcpy(ipc_values->pose_orientation, response->data, sizeof(float) * 16);
                    memcpy(ipc_values->pose_position, &pose.position, sizeof(float) * 3);
                    // trigger flush on just the last write
                    set_skippable_gamescope_reshade_effect_uniform_variable("pose_orientation", ipc_values->pose_orientation, 16, sizeof(float), false);
                    set_skippable_gamescope_reshade_effect_uniform_variable("pose_position", ipc_values->pose_position, 3, sizeof(float), true);

                    pthread_mutex_unlock(ipc_values->pose_orientation_mutex);
                }
                free(response);
            }
        }

        int x_velocity;
        int y_velocity;
        int next_joystick_x;
        int next_joystick_y;
        bool do_joystick_debug = config()->debug_joystick && (imu_counter % joystick_debug_imu_cycles) == 0;
        if (uinput || do_joystick_debug) {
            // tracking head movements in euler (roll, pitch, yaw) against 2d joystick/mouse (x,y) coordinates means that yaw
            // maps to horizontal movements (x) and pitch maps to vertical (y) movements. Because the euler values use a NWU
            // coordinate system, positive yaw/pitch values move left/down, respectively, and the mouse/joystick coordinate
            // systems are right-down, so a positive yaw should result in a negative x, and a positive pitch should result in a
            // positive y.
            x_velocity = config()->vr_lite_invert_x ? velocities.yaw : -velocities.yaw;
            y_velocity = config()->vr_lite_invert_y ? -velocities.pitch : velocities.pitch;
            next_joystick_x = joystick_value(x_velocity, joystick_max_degrees_per_s);
            next_joystick_y = joystick_value(y_velocity, joystick_max_degrees_per_s);
        }

        if (uinput) {
            if (config()->joystick_mode) {
                int next_joystick_z = joystick_value(-velocities.roll, joystick_max_degrees_per_s);
                libevdev_uinput_write_event(uinput, EV_ABS, ABS_RX, next_joystick_x);
                libevdev_uinput_write_event(uinput, EV_ABS, ABS_RY, next_joystick_y);
                if (config()->use_roll_axis)
                    libevdev_uinput_write_event(uinput, EV_ABS, ABS_RZ, next_joystick_z);
            } else if (config()->mouse_mode) {
                // keep track of the remainder (the amount that was lost with round()) for smoothing out mouse movements
                static float mouse_x_remainder = 0.0;
                static float mouse_y_remainder = 0.0;
                static float mouse_z_remainder = 0.0;

                // smooth out the mouse values using the remainders left over from previous writes
                float mouse_sensitivity_seconds = (float) config()->mouse_sensitivity / device->imu_cycles_per_s;
                float next_x = x_velocity * mouse_sensitivity_seconds + mouse_x_remainder;
                int next_x_int = round(next_x);
                mouse_x_remainder = next_x - next_x_int;

                float next_y = y_velocity * mouse_sensitivity_seconds + mouse_y_remainder;
                int next_y_int = round(next_y);
                mouse_y_remainder = next_y - next_y_int;

                float next_z = -velocities.roll * mouse_sensitivity_seconds + mouse_z_remainder;
                int next_z_int = round(next_z);
                mouse_z_remainder = next_z - next_z_int;

                libevdev_uinput_write_event(uinput, EV_REL, REL_X, next_x_int);
                libevdev_uinput_write_event(uinput, EV_REL, REL_Y, next_y_int);
                if (config()->use_roll_axis)
                    libevdev_uinput_write_event(uinput, EV_REL, REL_Z, next_z_int);
            } else if (!config()->external_mode) {
                log_error("Unsupported output mode: %s\n", config()->output_mode);
            }

            if (config()->mouse_mode || config()->joystick_mode)
                libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
        }

        // always use joystick debugging as it adds a helpful visual
        if (do_joystick_debug)
            joystick_debug(prev_joystick_x, prev_joystick_y, next_joystick_x, next_joystick_y);

        prev_joystick_x = next_joystick_x;
        prev_joystick_y = next_joystick_y;

        plugins.handle_pose_data(pose, velocities, imu_calibrated, ipc_values);

        // reset the counter every second
        if ((++imu_counter % device->imu_cycles_per_s) == 0) {
            imu_counter = 0;
        }
        pthread_mutex_unlock(&outputs_mutex);
    }
    device_checkin(device);
}

void reset_pose_data(ipc_values_type *ipc_values) {
    if (ipc_values) {    
        pthread_mutex_lock(ipc_values->pose_orientation_mutex);
        memcpy(ipc_values->pose_orientation, pose_orientation_reset_data, sizeof(float) * 16);
        memcpy(ipc_values->pose_position, pose_position_reset_data, sizeof(float) * 3);
        pthread_mutex_unlock(ipc_values->pose_orientation_mutex);
    }

    plugins.reset_pose_data();
}

bool is_imu_alive() {
    return get_epoch_time_ms() - last_healthy_imu_timestamp_ms < MS_PER_SEC;
}