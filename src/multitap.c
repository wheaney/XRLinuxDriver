#include "imu.h"
#include "buffer.h"

#include "logging.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define MT_BUFFER_MS 25

#define MT_STATE_IDLE 0
#define MT_STATE_RISE 1
#define MT_STATE_FALL 2
#define MT_STATE_PAUSE 3

int mt_buffer_size;
int imu_cycles_per_s;
buffer_type *mt_buffer = NULL;
int mt_state = MT_STATE_IDLE;
const float mt_detect_threshold = 2000.0;
const float mt_pause_threshold = 100.0;
uint64_t tap_start_time = 0;
uint64_t pause_start_time = 0;
uint64_t last_logged_peak_time = 0;
const int max_tap_period_ms = 750; // longest time-frame to allow between tap starts/rises
const int max_tap_duration_ms = 70; // a single tap should be very quick, ignore long accelerations
const int min_pause_ms = 10; // must detect a pause (~0 acceleration) between taps
float peak_max = 0.0;
int tap_count = 0;

float accel_adjust_constant;

void init_multi_tap(int init_imu_cycles_per_s) {
    imu_cycles_per_s = init_imu_cycles_per_s;
    float desired_buffer_size = (float)MT_BUFFER_MS / 1000.0 * imu_cycles_per_s;
    mt_buffer_size = floor(desired_buffer_size);

    // this is the ratio based on how much we had to round
    accel_adjust_constant = (float)mt_buffer_size/desired_buffer_size;

    if (mt_buffer) {
        free(mt_buffer);
        mt_buffer = NULL;
    }
    mt_buffer = create_buffer(mt_buffer_size);
}

// returns the number of taps observed
int detect_multi_tap(imu_euler_type velocities, uint32_t timestamp, bool debug) {
    if (mt_buffer) {
        // the oldest value is zero/unset if the buffer hasn't been filled yet, so we check prior to doing a
        // push/pop, to know if the value returned will be relevant to our calculations
        bool was_full = is_full(mt_buffer);
        float next_value = sqrt(velocities.roll * velocities.roll + velocities.pitch * velocities.pitch + velocities.yaw * velocities.yaw);
        float oldest_value = push(mt_buffer, next_value);

        if (was_full) {
            // extrapolate out to seconds, so the threshold can stay the same regardless of buffer size
            float acceleration = (next_value - oldest_value) * (float)imu_cycles_per_s / mt_buffer_size * accel_adjust_constant;
            int tap_elapsed_ms = timestamp - tap_start_time;
            if ((tap_count > 0 || mt_state != MT_STATE_IDLE) && tap_elapsed_ms > max_tap_period_ms) {
                peak_max = 0.0;
                mt_state = MT_STATE_IDLE;
                int final_tap_count = tap_count;
                tap_count = 0;

                if (final_tap_count > 0 && debug) log_debug("detected multi-tap of %d\n", final_tap_count);
                return final_tap_count;
            } else {
                switch(mt_state) {
                    case MT_STATE_IDLE: {
                        if (acceleration > mt_detect_threshold) {
                            tap_start_time = timestamp;
                            peak_max = 0.0;
                            mt_state = MT_STATE_RISE;
                            if (debug) log_debug("tap-rise detected %f\n", acceleration);
                        } else {
                            if (debug) {
                                if (acceleration > peak_max) peak_max = acceleration;
                                if ((timestamp - last_logged_peak_time) > 1000) {
                                    log_debug("no-tap detected, peak was %f\n", peak_max);
                                    peak_max = 0.0;
                                    last_logged_peak_time = timestamp;
                                }
                            }
                        }
                        break;
                    }
                    case MT_STATE_RISE: {
                        if (acceleration < 0) // accelerating in the opposite direction
                            mt_state = MT_STATE_FALL;
                        break;
                    }
                    case MT_STATE_FALL: {
                        if (acceleration > 0) { // acceleration switches back, stopping the fall
                            if (tap_elapsed_ms > max_tap_duration_ms) {
                                if (debug) log_debug("rise and fall took %d, too long for a tap\n", tap_elapsed_ms);
                                peak_max = 0.0;
                                mt_state = MT_STATE_IDLE;
                                tap_count == 0;
                            } else {
                                if (debug) log_debug("rise and fall took %d\n", tap_elapsed_ms);
                                tap_count++;
                                pause_start_time = timestamp;
                                mt_state = MT_STATE_PAUSE;
                            }
                        }
                        break;
                    }
                    case MT_STATE_PAUSE: {
                        if (fabs(acceleration) < mt_pause_threshold) {
                            int pause_elapsed_ms = timestamp - pause_start_time;
                            if (pause_elapsed_ms > min_pause_ms)
                                // paused long enough, wrap back around to idle where we can detect the next rise
                                mt_state = MT_STATE_IDLE;
                        } else {
                            // not idle, reset pause state timer
                            pause_start_time = timestamp;
                        }
                    }
                }
            }
        }
    }

    return 0;
}