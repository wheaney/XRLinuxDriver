#include "device3.h"
#include "device4.h"
#include "buffer.h"
#include "multitap.h"
#include "ipc.h"

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include <stdio.h>
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

#define EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)

const int max_input = 1 << 16;
const int mid_input = 0;
const int min_input = -max_input;

const int cycles_per_second = 1000;

const float joystick_max_degrees = 360.0 / cycles_per_second / 4;
const float joystick_max_radians = joystick_max_degrees * M_PI / 180.0;
const int joystick_resolution = max_input / joystick_max_radians;
const int default_mouse_sensitivity = 30;
const float default_look_ahead = -10.0;
const float default_look_ahead_ftm = 2.6;
const float default_external_zoom = 1.0;

const char *joystick_output_mode = "joystick";
const char *mouse_output_mode = "mouse";
const char *external_only_output_mode = "external_only";

// TODO - this is specific to the sombrero integration, either provide no default or move to a plug-in system where
//        the plug-in library would be expected to provide this default, if this functionality is used
char *sombrero_ipc_file_prefix = "/tmp/shader_runtime_";

const char *imu_data_ipc_name = "imu_quat_data";
const char *look_ahead_ms_ipc_name = "look_ahead_cfg";
const char *zoom_ipc_name = "zoom";
const char *disabled_ipc_name = "disabled";
float *imu_data_ipc_value;
float *look_ahead_ipc_value;
float *zoom_ipc_value;
bool *disabled_ipc_value;
bool ipc_enabled = false;

device3_type* glasses_imu;
bool glasses_ready=false;

bool driver_disabled=false;
bool use_roll_axis=false;
int mouse_sensitivity=default_mouse_sensitivity;
float look_ahead=default_look_ahead;
float look_ahead_ftm=default_look_ahead_ftm;
float external_zoom=0.0;
char *output_mode = NULL;
bool debug_threads=false;
bool debug_joystick=false;
bool debug_multi_tap=false;
bool debug_ipc=false;
bool force_reset_threads=false;

bool captured_screen_center=false;
device3_quat_type screen_center;

static int check(char * function, int i) {
    if (i < 0) {
        printf("libevdev.%s: %s\n", function, strerror(-i));
        exit(1);
    }

    return i;
}

void free_and_clear(char **str_ptr) {
    free(*str_ptr);
    *str_ptr = NULL;
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

// creates a file, if it doesn't already exist, in the user home directory with home directory permissions and ownership.
// this is helpful since the driver may be run with sudo, so we don't create files owned by root:root
static FILE* get_or_create_home_file(char *filename, char *mode, char *full_path, bool *created) {
    char *home_directory = getenv("HOME");
    snprintf(full_path, 1024, "%s/%s", home_directory, filename);
    FILE *fp = fopen(full_path, mode ? mode : "r");
    if (fp == NULL) {
        // Retrieve the permissions of the parent directory
        struct stat st;
        if (stat(home_directory, &st) == -1) {
            perror("stat");
            return NULL;
        }

        fp = fopen(full_path, "w");
        if (fp == NULL) {
            perror("Error creating config file");
            return NULL;
        }
        if (created != NULL)
            *created = true;

        // Set the permissions and ownership of the new file to be the same as the parent directory
        if (chmod(full_path, st.st_mode & 0777) == -1) {
            perror("Error setting file permissions");
            return NULL;
        }
        if (chown(full_path, st.st_uid, st.st_gid) == -1) {
            perror("Error setting file ownership");
            return NULL;
        }
    } else if (created != NULL) {
        *created = false;
    }

    return fp;
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

int joystick_debug_count = 0;
int prev_joystick_x = 0;
int prev_joystick_y = 0;

// keep track of the remainder (the amount that was lost with round()) for smoothing out mouse movements
float mouse_x_remainder = 0.0;
float mouse_y_remainder = 0.0;
float mouse_z_remainder = 0.0;

#define GYRO_BUFFERS_COUNT 4 // quat values: x, y, z, w
#define GYRO_BUFFER_SIZE 4 // how many events to use for smoothing out velocity
const float buffer_to_seconds = (float) cycles_per_second / GYRO_BUFFER_SIZE;

buffer_type **quat_stage_1_buffer = NULL;
buffer_type **quat_stage_2_buffer = NULL;
struct libevdev_uinput* uinput;
void handle_imu_event(uint64_t timestamp,
		   device3_event_type event,
		   const device3_ahrs_type* ahrs) {
    if (uinput && event == DEVICE3_EVENT_UPDATE) {
        static device3_vec3_type last_euler;
        device3_quat_type q = device3_get_orientation(ahrs);
        device3_vec3_type e = device3_get_euler(q);

        float delta_x = degree_delta(last_euler.z, e.z);
        float delta_y = degree_delta(last_euler.y, e.y);
        float delta_z = degree_delta(last_euler.x, e.x);

        if (ipc_enabled) {
            // TODO - wait for calibration before capturing a center, add splash screen or text indicating the wait
            device3_vec3_type velocities = {
                .x=delta_x * cycles_per_second,
                .y=delta_y * cycles_per_second,
                .z=delta_z * cycles_per_second
            };
            if (!captured_screen_center || detect_multi_tap(velocities, timestamp, cycles_per_second, debug_multi_tap) == 2) {
                if (captured_screen_center) printf("Double-tap detected, centering screen\n");

                screen_center = q;
                captured_screen_center=true;
            }

            if (quat_stage_1_buffer == NULL || quat_stage_2_buffer == NULL) {
                quat_stage_1_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
                quat_stage_2_buffer = malloc(sizeof(buffer_type*) * GYRO_BUFFERS_COUNT);
                for (int i = 0; i < GYRO_BUFFERS_COUNT; i++) {
                    quat_stage_1_buffer[i] = create_buffer(GYRO_BUFFER_SIZE);
                    quat_stage_2_buffer[i] = create_buffer(GYRO_BUFFER_SIZE);
                    if (quat_stage_1_buffer[i] == NULL || quat_stage_2_buffer[i] == NULL) {
                        fprintf(stderr, "Error allocating memory\n");
                        exit(1);
                    }
                }
            }

            // the oldest values are zero/unset if the buffer hasn't been filled yet, so we check prior to doing a
            // push/pop, to know if the values that are returned will be relevant to our calculations
            bool was_full = is_full(quat_stage_1_buffer[0]);
            float stage_1_quat_w = push(quat_stage_1_buffer[0], q.w);
            float stage_1_quat_x = push(quat_stage_1_buffer[1], q.x);
            float stage_1_quat_y = push(quat_stage_1_buffer[2], q.y);
            float stage_1_quat_z = push(quat_stage_1_buffer[3], q.z);

            if (was_full) {
                was_full = is_full(quat_stage_2_buffer[0]);
                float stage_2_quat_w = push(quat_stage_2_buffer[0], stage_1_quat_w);
                float stage_2_quat_x = push(quat_stage_2_buffer[1], stage_1_quat_x);
                float stage_2_quat_y = push(quat_stage_2_buffer[2], stage_1_quat_y);
                float stage_2_quat_z = push(quat_stage_2_buffer[3], stage_1_quat_z);

                if (was_full) {
                    // write to shared memory for anyone using the same ipc prefix to consume
                    imu_data_ipc_value[0] = q.x;
                    imu_data_ipc_value[1] = q.y;
                    imu_data_ipc_value[2] = q.z;
                    imu_data_ipc_value[3] = q.w;
                    imu_data_ipc_value[4] = stage_1_quat_x;
                    imu_data_ipc_value[5] = stage_1_quat_y;
                    imu_data_ipc_value[6] = stage_1_quat_z;
                    imu_data_ipc_value[7] = stage_1_quat_w;
                    imu_data_ipc_value[8] = stage_2_quat_x;
                    imu_data_ipc_value[9] = stage_2_quat_y;
                    imu_data_ipc_value[10] = stage_2_quat_z;
                    imu_data_ipc_value[11] = stage_2_quat_w;
                    imu_data_ipc_value[12] = screen_center.x;
                    imu_data_ipc_value[13] = screen_center.y;
                    imu_data_ipc_value[14] = screen_center.z;
                    imu_data_ipc_value[15] = screen_center.w;
                }
            }
        }

        int next_joystick_x = joystick_value(delta_x, joystick_max_degrees);
        int next_joystick_y = joystick_value(delta_y, joystick_max_degrees);

        bool using_evdev = false;
        if (strcmp(output_mode, joystick_output_mode) == 0) {
            using_evdev = true;
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RX, next_joystick_x);
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RY, next_joystick_y);
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RZ, joystick_value(delta_z, joystick_max_degrees));
        } else if (strcmp(output_mode, mouse_output_mode) == 0) {
            using_evdev = true;

            // smooth out the mouse values using the remainders left over from previous writes
            float next_x = delta_x * mouse_sensitivity + mouse_x_remainder;
            int next_x_int = round(next_x);
            mouse_x_remainder = next_x - next_x_int;

            float next_y = delta_y * mouse_sensitivity + mouse_y_remainder;
            int next_y_int = round(next_y);
            mouse_y_remainder = next_y - next_y_int;

            float next_z = delta_z * mouse_sensitivity + mouse_z_remainder;
            int next_z_int = round(next_z);
            mouse_z_remainder = next_z - next_z_int;

            libevdev_uinput_write_event(uinput, EV_REL, REL_X, next_x_int);
            libevdev_uinput_write_event(uinput, EV_REL, REL_Y, next_y_int);
            if (use_roll_axis)
                libevdev_uinput_write_event(uinput, EV_REL, REL_Z, next_z_int);
        } else if (strcmp(output_mode, external_only_output_mode) != 0) {
            fprintf(stderr, "Unsupported output mode: %s\n", output_mode);
        }

        if (using_evdev)
            libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);

        // always use joystick debugging as it adds a helpful visual
        if (debug_joystick && (joystick_debug_count++ % 100) == 0) {
            joystick_debug_count = 0;
            joystick_debug(prev_joystick_x, prev_joystick_y, next_joystick_x, next_joystick_y);
        }
        prev_joystick_x = next_joystick_x;
        prev_joystick_y = next_joystick_y;

        last_euler = e;
    }
}

// pthread function to create the virtual controller and poll the glasses
void *poll_glasses_imu(void *arg) {
    fprintf(stdout, "Device connected, redirecting input to %s...\n", output_mode);

    // create our virtual device
    struct input_absinfo absinfo;
    absinfo.minimum = min_input;
    absinfo.maximum = max_input;
    absinfo.resolution = joystick_resolution;
    absinfo.value = mid_input;
    absinfo.flat = 2;
    absinfo.fuzz = 0;

    struct libevdev* evdev = libevdev_new();
    bool using_evdev = false;
    if (strcmp(output_mode, joystick_output_mode) == 0) {
        using_evdev = true;
        check("libevdev_enable_property", libevdev_enable_property(evdev, INPUT_PROP_BUTTONPAD));
        libevdev_set_name(evdev, "XREAL Air virtual joystick");
        check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_ABS));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_X, &absinfo));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_Y, &absinfo));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_Z, &absinfo));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_RX, &absinfo));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_RY, &absinfo));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_ABS, ABS_RZ, &absinfo));

        /* do not remove next 3 lines or udev scripts won't assign 0664 permissions -sh */
        check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_KEY));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_JOYSTICK, NULL));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_TRIGGER, NULL));

        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_A, NULL));
    } else if (strcmp(output_mode, mouse_output_mode) == 0) {
        using_evdev = true;
        libevdev_set_name(evdev, "XREAL Air virtual mouse");

        check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_REL));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_X, NULL));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_Y, NULL));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_REL, REL_WHEEL, NULL));

        check("libevdev_enable_event_type", libevdev_enable_event_type(evdev, EV_KEY));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_LEFT, NULL));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_MIDDLE, NULL));
        check("libevdev_enable_event_code", libevdev_enable_event_code(evdev, EV_KEY, BTN_RIGHT, NULL));
    }
    if (using_evdev)
        check("libevdev_uinput_create_from_device", libevdev_uinput_create_from_device(evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput));

    device3_clear(glasses_imu);
    while (!driver_disabled && !force_reset_threads) {
        if (device3_read(glasses_imu, 1) != DEVICE3_ERROR_NO_ERROR) {
            break;
        }
    }

    device3_close(glasses_imu);
    glasses_ready=false;
    if (ipc_enabled) *disabled_ipc_value = true;
    if (using_evdev) {
        libevdev_uinput_destroy(uinput);
        libevdev_free(evdev);
    }

    if (debug_threads)
        printf("\tdebug: Exiting glasses_imu thread\n");
}

// TODO - do this setup once from main() using the default ipc file prefix
void setup_ipc() {
    if (!ipc_enabled) {
        if (get_ipc_file_prefix() != NULL) {
            if (debug_ipc) printf("\tdebug: setup_ipc, prefix set, enabling IPC\n");
            setup_ipc_value(imu_data_ipc_name, (void**) &imu_data_ipc_value, sizeof(float) * 16, debug_ipc);
            setup_ipc_value(look_ahead_ms_ipc_name, (void**) &look_ahead_ipc_value, sizeof(float) * 2, debug_ipc);
            setup_ipc_value(zoom_ipc_name, (void**) &zoom_ipc_value, sizeof(float), debug_ipc);
            setup_ipc_value(disabled_ipc_name, (void**) &disabled_ipc_value, sizeof(bool), debug_ipc);
            ipc_enabled = true;

            printf("IPC enabled, file prefix set to '%s'\n", get_ipc_file_prefix());
        } else {
            if (debug_ipc) printf("\tdebug: setup_ipc, prefix not set, disabling IPC\n");
            ipc_enabled = false;
        }
    } else {
        if (debug_ipc) printf("\tdebug: setup_ipc, already enabled, doing nothing\n");
    }
}

void parse_config_file(FILE *fp) {
    bool new_driver_disabled = false;
    bool new_use_roll_axis = false;
    int new_mouse_sensitivity = default_mouse_sensitivity;
    float new_external_zoom = default_external_zoom;
    char *new_output_mode = malloc(strlen(mouse_output_mode) + 1);
    strcpy(new_output_mode, mouse_output_mode);
    bool new_debug_joystick = false;
    bool new_debug_threads = false;
    bool new_debug_multi_tap = false;
    bool new_debug_ipc = false;
    float new_look_ahead = default_look_ahead;
    float new_look_ahead_ftm = default_look_ahead_ftm;

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (strcmp(key, "disabled") == 0) {
            new_driver_disabled = strcmp(value, "true") == 0;
        } else if (strcmp(key, "debug") == 0) {
            char *token = strtok(value, ",");
            while (token != NULL) {
                if (strcmp(token, "joystick") == 0) {
                    new_debug_joystick = true;
                }
                if (strcmp(token, "taps") == 0) {
                    new_debug_multi_tap = true;
                }
                if (strcmp(token, "threads") == 0) {
                    new_debug_threads = true;
                }
                if (strcmp(token, "ipc") == 0) {
                    new_debug_ipc = true;
                }
                token = strtok(NULL, ",");
            }
        } else if (strcmp(key, "use_roll_axis") == 0) {
            new_use_roll_axis = true;
        } else if (strcmp(key, "mouse_sensitivity") == 0) {
            char *endptr;
            errno = 0;
            long num = strtol(value, &endptr, 10);
            if (errno != ERANGE && endptr != value) {
                new_mouse_sensitivity = (int) num;
            } else {
                fprintf(stderr, "Error parsing mouse_sensitivity value: %s\n", value);
            }
        } else if (strcmp(key, "look_ahead") == 0) {
            char *endptr;
            errno = 0;
            float num = strtof(value, &endptr);
            if (errno != ERANGE && endptr != value) {
                new_look_ahead = num;
            } else {
                fprintf(stderr, "Error parsing look_ahead value: %s\n", value);
            }
        } else if (strcmp(key, "look_ahead_ftm") == 0) {
            char *endptr;
            errno = 0;
            float num = strtof(value, &endptr);
            if (errno != ERANGE && endptr != value) {
                new_look_ahead_ftm = num;
            } else {
                fprintf(stderr, "Error parsing look_ahead_ftm value: %s\n", value);
            }
        } else if (strcmp(key, "external_zoom") == 0) {
            char *endptr;
            errno = 0;
            float num = strtof(value, &endptr);
            if (errno != ERANGE && endptr != value) {
                new_external_zoom = num;
            } else {
                fprintf(stderr, "Error parsing external_zoom value: %s\n", value);
            }
        } else if (strcmp(key, "output_mode") == 0) {
            free_and_clear(&new_output_mode);
            new_output_mode = malloc(strlen(value) + 1);
            strcpy(new_output_mode, value);
        }
    }

    if (!driver_disabled && new_driver_disabled)
        printf("Driver has been disabled, see ~/bin/xreal_driver_config\n");
    if (driver_disabled && !new_driver_disabled)
        printf("Driver has been re-enabled, see ~/bin/xreal_driver_config\n");

    if (!use_roll_axis && new_use_roll_axis)
        printf("Roll axis has been enabled, see ~/bin/xreal_driver_config\n");
    if (use_roll_axis && !new_use_roll_axis)
        printf("Roll axis has been disabled, see ~/bin/xreal_driver_config\n");

    if (mouse_sensitivity != new_mouse_sensitivity)
        fprintf(stdout, "Mouse sensitivity has changed to %d, see ~/bin/xreal_driver_config\n", new_mouse_sensitivity);

    bool look_ahead_changed = look_ahead != new_look_ahead || look_ahead_ftm != new_look_ahead_ftm;
    if (look_ahead_changed)
        fprintf(stdout, "Look ahead has changed to %f and %f, see ~/bin/xreal_driver_config\n", new_look_ahead, new_look_ahead_ftm);

    bool external_zoom_changed = external_zoom != new_external_zoom;
    if (external_zoom_changed)
        fprintf(stdout, "External zoom has changed to %f, see ~/bin/xreal_driver_config\n", new_external_zoom);

    bool output_mode_changed = strcmp(output_mode, new_output_mode) != 0;
    if (output_mode_changed)
        printf("Output mode has been changed to '%s', see ~/bin/xreal_driver_config\n", new_output_mode);

    if (!debug_joystick && new_debug_joystick)
        printf("Joystick debugging has been enabled, to see it, use 'watch -n 0.1 cat ~/.xreal_joystick_debug' in bash\n");
    if (debug_joystick && !new_debug_joystick)
        printf("Joystick debugging has been disabled\n");

    if (debug_threads != new_debug_threads)
        fprintf(stdout, "Threads debugging has been %s\n", new_debug_threads ? "enabled" : "disabled");

    if (debug_multi_tap != new_debug_multi_tap)
        fprintf(stdout, "Multi-tap debugging has been %s\n", new_debug_multi_tap ? "enabled" : "disabled");

    if (debug_ipc != new_debug_ipc)
        fprintf(stdout, "IPC debugging has been %s\n", new_debug_ipc ? "enabled" : "disabled");

    driver_disabled = new_driver_disabled;
    use_roll_axis = new_use_roll_axis;
    mouse_sensitivity = new_mouse_sensitivity;
    look_ahead = new_look_ahead;
    look_ahead_ftm = new_look_ahead_ftm;
    external_zoom = new_external_zoom;
    if (output_mode) free_and_clear(&output_mode);
    output_mode = new_output_mode;
    debug_joystick = new_debug_joystick;
    debug_threads = new_debug_threads;
    debug_multi_tap = new_debug_multi_tap;
    debug_ipc = new_debug_ipc;

    if (output_mode_changed)
        force_reset_threads = true;

    if (ipc_enabled) {
        *disabled_ipc_value = driver_disabled;
        if (external_zoom_changed) *zoom_ipc_value = external_zoom;
        if (look_ahead_changed) {
            look_ahead_ipc_value[0] = look_ahead;
            look_ahead_ipc_value[1] = look_ahead_ftm;
        }
    } else if (strcmp(output_mode, external_only_output_mode) == 0) {
        fprintf(stderr, "error: no IPC path set, IMU data will not be available for external usage, see ~/bin/xreal_driver_config\n");
    }
}

// pthread function to monitor the config file for changes
void *monitor_config_file(void *arg) {
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
    while ((glasses_ready || driver_disabled) && !force_reset_threads) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        // wait for data to be available for reading, do this with select() so we can specify a timeout and make
        // sure we shouldn't exit due to driver_disabled
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

    if (debug_threads)
        printf("\tdebug: Exiting monitor_config_file thread\n");

    fclose(fp);
    inotify_rm_watch(fd, wd);
    close(fd);
}

int main(int argc, const char** argv) {
    output_mode = malloc(strlen(mouse_output_mode) + 1);
    strcpy(output_mode, mouse_output_mode);

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

    set_ipc_file_prefix(sombrero_ipc_file_prefix);
    setup_ipc();

    glasses_imu = malloc(sizeof(device3_type));
    while (1) {
        int device_error = device3_open(glasses_imu, handle_imu_event);
        if (device_error != DEVICE3_ERROR_NO_ERROR)
            printf("Waiting for glasses\n");

        while (device_error != DEVICE3_ERROR_NO_ERROR) {
            // TODO - move to a blocking check, rather than polling for device availability
            // retry every 5 seconds until the device becomes available
            device3_close(glasses_imu);
            sleep(5);
            device_error = device3_open(glasses_imu, handle_imu_event);
        }

        glasses_ready=true;
        setup_ipc();
        if (ipc_enabled) *disabled_ipc_value = false;

        // kick off threads to monitor glasses and config file, wait for both to finish (glasses disconnected)
        pthread_t glasses_imu_thread;
        pthread_t monitor_config_file_thread;
        pthread_create(&glasses_imu_thread, NULL, poll_glasses_imu, NULL);
        pthread_create(&monitor_config_file_thread, NULL, monitor_config_file, NULL);
        pthread_join(glasses_imu_thread, NULL);
        pthread_join(monitor_config_file_thread, NULL);

        if (debug_threads)
            printf("\tdebug: All threads have exited, starting over\n");

        force_reset_threads = false;
    }

    return 0;
}
