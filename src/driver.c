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
#include <sys/file.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

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
    if (uinput && event == DEVICE3_EVENT_UPDATE) {
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

// pthread function to create the virtual controller and poll the glasses
void *poll_glasses_imu(void *arg) {
    printf("Device connected, redirecting input to virtual controller...\n");

    // create our virtual device
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

    device3_clear(glasses_imu);
    while (!driver_disabled) {
        if (device3_read(glasses_imu, 1, false) < 0) {
            break;
        }
    }

    glasses_imu = NULL;
    libevdev_uinput_destroy(uinput);
    libevdev_free(evdev);
}

void parse_config_file(FILE *fp) {
    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (strcmp(key, "disabled") == 0) {
            bool was_disabled = driver_disabled;
            driver_disabled = strcmp(value, "true") == 0;
            if (!was_disabled && driver_disabled)
                printf("Driver has been disabled, see ~/bin/xreal_driver_config\n");
            if (was_disabled && !driver_disabled)
                printf("Driver has been re-enabled, see ~/bin/xreal_driver_config\n");
        }
    }
}

// creates a file, if it doesn't already exist, in the user home directory with home directory permissions and ownership.
// this is helpful since the driver may be run with sudo, so we don't create files owned by root:root
static FILE* get_or_create_home_file(char *filename, char *mode, char *full_path) {
    char *home_directory = getenv("HOME");
    snprintf(full_path, 1024, "%s/%s", home_directory, filename);
    FILE *fp = fopen(full_path, "r");
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

        // Set the permissions and ownership of the new file to be the same as the parent directory
        if (chmod(full_path, st.st_mode & 0777) == -1) {
            perror("Error setting file permissions");
            return NULL;
        }
        if (chown(full_path, st.st_uid, st.st_gid) == -1) {
            perror("Error setting file ownership");
            return NULL;
        }
    }

    return fp;
}

// pthread function to monitor the config file for changes
void *monitor_config_file(void *arg) {
    char filename[1024];
    FILE *fp = get_or_create_home_file(".xreal_driver_config", "r", &filename[0]);
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
    while (glasses_imu || driver_disabled) {
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

    fclose(fp);
    inotify_rm_watch(fd, wd);
    close(fd);
}

int main(int argc, const char** argv) {
    // ensure the log file exists, reroute stdout and stderr there
    char log_file_path[1024];
    FILE *log_file = get_or_create_home_file(".xreal_driver_log", NULL, &log_file_path[0]);
    fclose(log_file);
    freopen(log_file_path, "a", stdout);
    freopen(log_file_path, "a", stderr);

    // when redirecting stdout/stderr to a file, it becomes fully buffered, requiring lots of manual flushing of the
    // stream, this makes them unbuffered, which is fine since we log so little
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // set a lock so only one instance of the driver can be running at a time
    char lock_file_path[1024];
    FILE *lock_file = get_or_create_home_file(".xreal_driver_lock", "r", &lock_file_path[0]);
    int rc = flock(fileno(lock_file), LOCK_EX | LOCK_NB);
    if(rc) {
        if(EWOULDBLOCK == errno)
            printf("Another instance of this program is already running.\n");
        exit(1);
    }

    while (1) {
        glasses_imu = device3_open(handle_device_3);
        while (!glasses_imu || !glasses_imu->ready) {
            // TODO - move to a blocking check, rather than polling for device availability
            // retry every 5 seconds until the device becomes available
            device3_close(glasses_imu);
            sleep(5);
            glasses_imu = device3_open(handle_device_3);
        }

        // kick off threads to monitor glasses and config file, wait for both to finish (glasses disconnected)
        pthread_t glasses_imu_thread;
        pthread_t monitor_config_file_thread;
        pthread_create(&glasses_imu_thread, NULL, poll_glasses_imu, NULL);
        pthread_create(&monitor_config_file_thread, NULL, monitor_config_file, NULL);
        pthread_join(glasses_imu_thread, NULL);
        pthread_join(monitor_config_file_thread, NULL);

        device3_close(glasses_imu);
    }

    return 0;
}
