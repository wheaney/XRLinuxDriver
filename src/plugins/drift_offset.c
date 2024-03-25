#include "imu.h"
#include "plugins.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define STEP_TIME_MS 25

uint32_t drift_offset_start_time = 0;
bool drift_offset_set = false;
bool drift_offset_should_capture_first = false;
bool drift_offset_should_capture_last = false;
imu_quat_type drift_offset_start_quat = {0, 0, 0, 1};
imu_quat_type drift_offset_step_quat = {0, 0, 0, 0};

uint32_t drift_offset_last_step_time = 0;

imu_quat_type drift_offset_modify_screen_center(uint32_t timestamp_ms, imu_quat_type quat, imu_quat_type screen_center) {
    if (drift_offset_should_capture_last) {
        uint32_t elapsed_time = timestamp_ms - drift_offset_start_time;
        printf("Capturing drift offset across %d ms\n", elapsed_time);

        float drift_offset_steps = (float) elapsed_time / (float) STEP_TIME_MS;
        imu_quat_type quat_delta = multiply_quaternions(conjugate(drift_offset_start_quat), quat);
        drift_offset_step_quat = quaternion_pow(quat_delta, 1.0f / drift_offset_steps);
        drift_offset_last_step_time = timestamp_ms;
        drift_offset_set = true;
        drift_offset_should_capture_last = false;
    } else if (drift_offset_set) {
        if (timestamp_ms - drift_offset_last_step_time > STEP_TIME_MS) {
            drift_offset_last_step_time += STEP_TIME_MS;

            // "W-what are you doing, step-quat??"
            return multiply_quaternions(screen_center, drift_offset_step_quat);
        }
    } else if (drift_offset_should_capture_first) {
        printf("Initializing drift offset\n");
        drift_offset_start_time = timestamp_ms;
        drift_offset_start_quat = screen_center;
        drift_offset_should_capture_first = false;
    }

    return screen_center;
}

void drift_offset_handle_multi_tap(int tap_count) {
    if (tap_count == 3) {
        drift_offset_set = false;
        drift_offset_should_capture_last = false;
        drift_offset_start_time = 0;
    } else if (drift_offset_start_time == 0) {
        drift_offset_should_capture_first = tap_count == 2;
    } else {
        drift_offset_should_capture_last = tap_count == 2;
    }
}


const plugin_type drift_offset_plugin = {
    .id = "drift_offset",
    .modify_screen_center = drift_offset_modify_screen_center,
    .handle_multi_tap = drift_offset_handle_multi_tap
};