//
// Created by thejackimonster on 29.03.23.
//
// Copyright (c) 2023 thejackimonster. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "device3.h"
#include "device4.h"

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
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

#include <math.h>
#include <pthread.h>

#define EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)

const int max_input = 1 << 16;
const int mid_input = 0;
const int min_input = -max_input;

const int cycles_per_second = 1000;

const float joystick_max_degrees = 360.0 / cycles_per_second / 4;
const float joystick_max_radians = joystick_max_degrees * M_PI / 180.0;
const int joystick_resolution = max_input / joystick_max_radians;

static int check(int i) {
    if (i < 0) {
        printf("%s\n", strerror(-i));
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

struct libevdev_uinput* uinput;
void handle_device_3(uint64_t timestamp,
		   device3_event_type event,
		   const device3_ahrs_type* ahrs) {
    if (event == DEVICE3_EVENT_UPDATE) {
        static device3_vec3_type prev_tracked;
        device3_quat_type q = device3_get_orientation(ahrs);
        device3_vec3_type e = device3_get_euler(q);

        float delta_x = degree_delta(prev_tracked.z, e.z);
        float delta_y = degree_delta(prev_tracked.y, e.y);
        float delta_z = degree_delta(prev_tracked.x, e.x);
        libevdev_uinput_write_event(uinput, EV_ABS, ABS_RX, joystick_value(delta_x, joystick_max_degrees));
        libevdev_uinput_write_event(uinput, EV_ABS, ABS_RY, joystick_value(delta_y, joystick_max_degrees));
        libevdev_uinput_write_event(uinput, EV_ABS, ABS_RZ, joystick_value(delta_z, joystick_max_degrees));
        libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);

        prev_tracked = e;
    }
}

void handle_device_4(uint64_t timestamp,
		   device4_event_type event,
		   uint8_t brightness,
		   const char* msg) {
    switch (event) {
        case DEVICE4_EVENT_MESSAGE:
            printf("Message: `%s`\n", msg);
            break;
        case DEVICE4_EVENT_BRIGHTNESS_UP:
            printf("Increase Brightness: %u\n", brightness);
            libevdev_uinput_write_event(uinput, EV_KEY, BTN_A, 1);
            libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
            break;
        case DEVICE4_EVENT_BRIGHTNESS_DOWN:
            printf("Decrease Brightness: %u\n", brightness);
            break;
        default:
            break;
    }
}

device3_type* glasses_imu;
bool driver_disabled=false;

// pthread function to poll the glasses and translate to our virtual controller's joystick input
void *poll_glasses_imu(void *arg) {
    fprintf(stdout, "Device connected, redirecting input to virtual controller...\n");
    fflush(NULL);

    device3_clear(glasses_imu);
    while (!driver_disabled && glasses_imu) {
        if (device3_read(glasses_imu, 1, false) < 0) {
            break;
        }
        fflush(NULL);
    }
}

void parse_config_file(FILE *fp) {
    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (strcmp(key, "disabled") == 0) {
            driver_disabled = strcmp(value, "true") == 0;
            if (driver_disabled)
                fprintf(stdout, "Driver is disabled!\n");
        }
    }
}

// pthread function to monitor the config file for changes
void *monitor_config_file(void *arg) {
    char filename[1024];
    char *home = getenv("HOME");
    snprintf(filename, sizeof(filename), "%s/.xreal_driver_config", home);
    fprintf(stdout, "config file path: %s\n", filename);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        // Parse the parent path from the file path
        char *parent_path = strdup(filename);
        parent_path = dirname(parent_path);

        // Retrieve the permissions of the parent directory
        struct stat st;
        if (stat(parent_path, &st) == -1) {
            perror("stat");
            exit(1);
        }

        fp = fopen(filename, "w");
        if (fp == NULL) {
            perror("Error creating config file");
            exit(1);
        }

        // Set the permissions and ownership of the new file to be the same as the parent directory
        if (chmod(filename, st.st_mode & 0777) == -1) {
            perror("Error setting config file permissions");
            exit(1);
        }
        if (chown(filename, st.st_uid, st.st_gid) == -1) {
            perror("Error setting config file ownership");
            exit(1);
        }
    } else {
        parse_config_file(fp);
    }

    int fd = inotify_init();
    if (fd < 0) {
        perror("Error initializing inotify");
        exit(1);
    }

    int wd = inotify_add_watch(fd, filename, IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB);
    if (wd < 0) {
        perror("Error adding watch");
        exit(1);
    }

    char buffer[EVENT_SIZE];
    while (!driver_disabled) {
        int length = read(fd, buffer, EVENT_SIZE);
        if (length < 0) {
            perror("Error reading events");
            exit(1);
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->mask & IN_DELETE_SELF) {
                // The file has been deleted, so we need to re-add the watch
                wd = inotify_add_watch(fd, filename, IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB);
                if (wd < 0) {
                    perror("Error re-adding watch");
                    exit(1);
                }
            } else {
                fp = freopen(filename, "r", fp);
                parse_config_file(fp);
            }
            i += EVENT_SIZE + event->len;
        }
    }

    fclose(fp);
    inotify_rm_watch(fd, wd);
    close(fd);
}

int main(int argc, const char** argv) {
    struct input_absinfo absinfo;

    absinfo.minimum = min_input;
    absinfo.maximum = max_input;
    absinfo.resolution = joystick_resolution;
    absinfo.value = mid_input;
    absinfo.flat = 2;
    absinfo.fuzz = 0;

    struct libevdev* evdev = libevdev_new();
    check(libevdev_enable_property(evdev, INPUT_PROP_BUTTONPAD));
    libevdev_set_name(evdev, "xReal Air virtual joystick");
    check(libevdev_enable_event_type(evdev, EV_ABS));
    check(libevdev_enable_event_code(evdev, EV_ABS, ABS_X, &absinfo));
    check(libevdev_enable_event_code(evdev, EV_ABS, ABS_Y, &absinfo));
    check(libevdev_enable_event_code(evdev, EV_ABS, ABS_Z, &absinfo));
    check(libevdev_enable_event_code(evdev, EV_ABS, ABS_RX, &absinfo));
    check(libevdev_enable_event_code(evdev, EV_ABS, ABS_RY, &absinfo));
    check(libevdev_enable_event_code(evdev, EV_ABS, ABS_RZ, &absinfo));

    /* do not remove next 3 lines or udev scripts won't assign 0664 permissions -sh */
    check(libevdev_enable_event_type(evdev, EV_KEY));
    check(libevdev_enable_event_code(evdev, EV_KEY, BTN_JOYSTICK, NULL));
    check(libevdev_enable_event_code(evdev, EV_KEY, BTN_TRIGGER, NULL));

    check(libevdev_enable_event_code(evdev, EV_KEY, BTN_A, NULL));

    check(libevdev_uinput_create_from_device(evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput));

    glasses_imu = device3_open(handle_device_3);
    int connection_attempts = 0;
    while (!glasses_imu || !glasses_imu->ready) {
        if (++connection_attempts > 5) {
            fprintf(stderr, "Device not found, exiting...\n");
            break;
        }

        device3_close(glasses_imu);
        fprintf(stderr, "Device not found, sleeping...\n");
        sleep(5);
        glasses_imu = device3_open(handle_device_3);
    }

    if (glasses_imu && glasses_imu->ready) {
        pthread_t glasses_imu_thread;
        pthread_t monitor_config_file_thread;
        pthread_create(&glasses_imu_thread, NULL, poll_glasses_imu, NULL);
        pthread_create(&monitor_config_file_thread, NULL, monitor_config_file, NULL);

        pthread_join(glasses_imu_thread, NULL);
    }

    device3_close(glasses_imu);
    libevdev_uinput_destroy(uinput);
    libevdev_free(evdev);
    return 0;
}
