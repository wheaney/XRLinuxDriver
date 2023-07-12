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
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

#include <math.h>

const int max_input = 1 << 16;
const int mid_input = 0;
const int min_input = -max_input;

const int cycles_per_second = 1000;

// anyone rotating their head more than 360 degrees per second is probably dead
const float max_head_movement_degrees_per_cycle = 360.0 / cycles_per_second;

const float joystick_max_degrees = max_head_movement_degrees_per_cycle / 4;
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
        static device3_vec3_type prev;
        device3_quat_type q = device3_get_orientation(ahrs);
        device3_vec3_type e = device3_get_euler(q);

        float this_delta_x = prev.z - e.z;
        float this_delta_y = prev.y - e.y;
        prev = e;

        // Ignore anomalous data from the glasses.
        bool valid_movement = fabs(this_delta_x) < max_head_movement_degrees_per_cycle &&
                              fabs(this_delta_y) < max_head_movement_degrees_per_cycle;
        if (valid_movement) {
            float delta_x = degree_delta(prev_tracked.z, e.z);
            float delta_y = degree_delta(prev_tracked.y, e.y);
            float delta_z = degree_delta(prev_tracked.x, e.x);
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RX, joystick_value(delta_x, joystick_max_degrees));
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RY, joystick_value(delta_y, joystick_max_degrees));
            libevdev_uinput_write_event(uinput, EV_ABS, ABS_RZ, joystick_value(delta_z, joystick_max_degrees));
            libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);

            prev_tracked = e;
        } else {
            // adjust our tracked values by the delta of the anomaly so we can pick right back up on the next cycle
            prev_tracked.z -= this_delta_x;
            prev_tracked.y -= this_delta_y;
        }
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

    device3_type* dev3 = device3_open(handle_device_3);
    int connection_attempts = 0;
    while (!dev3 || !dev3->ready) {
        if (++connection_attempts > 5) {
            fprintf(stderr, "Device not found, exiting...\n");
            exit(1);
        }

        fprintf(stderr, "Device not found, sleeping...\n");
        sleep(5);
        dev3 = device3_open(handle_device_3);
    }
    fprintf(stdout, "Device connected, redirecting input to virtual joystick...\n");

    device3_clear(dev3);

    while (dev3) {
        if (device3_read(dev3, 0, false) < 0) {
            break;
        }
    }

    device3_close(dev3);
    libevdev_uinput_destroy(uinput);
    libevdev_free(evdev);
    return 0;
}
