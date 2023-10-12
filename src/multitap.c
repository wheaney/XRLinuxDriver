#include "device3.h"
#include "buffer.h"

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

// by buffering over a small number of cycles, we can smooth out noise and confidently detect quick taps
#define MT_BUFFER_SIZE 25
buffer_type *mt_buffer = NULL;
const int mt_state_idle = 0;
const int mt_state_rise = 1;
const int mt_state_fall = 2;
const int mt_state_pause = 3;
int mt_state = mt_state_idle;
const float mt_detect_threshold = 2500.0;
const float mt_pause_threshold = 100.0;
uint64_t tap_start_time = 0;
uint64_t pause_start_time = 0;
uint64_t last_logged_peak_time = 0;
const int max_tap_period_ms = 500; // longest time-frame to allow between tap starts/rises
const int max_tap_rise_ms = 100; // a single tap should be very quick, ignore long accelerations
const int min_pause_ms = 50 - MT_BUFFER_SIZE; // must detect a pause (~0 acceleration) between taps
float peak_max = 0.0;
int tap_count = 0;

// returns the number of taps observed
int detect_multi_tap(device3_vec3_type velocities, uint64_t timestamp, const int cycles_per_second, bool debug_multi_tap) {
    if (mt_buffer == NULL) {
        mt_buffer = create_buffer(MT_BUFFER_SIZE);
    }

    // the oldest value is zero/unset if the buffer hasn't been filled yet, so we check prior to doing a
    // push/pop, to know if the value returned will be relevant to our calculations
    bool was_full = is_full(mt_buffer);
    float next_value = sqrt(velocities.x * velocities.x + velocities.y * velocities.y + velocities.z * velocities.z);
    float oldest_value = push(mt_buffer, next_value);

    if (was_full) {
        // extrapolate out to seconds, so the threshold can stay the same regardless of buffer size
        float acceleration = (next_value - oldest_value) * cycles_per_second / MT_BUFFER_SIZE;
        int tap_elapsed_ms = (timestamp - tap_start_time) / 1000000;
        if ((tap_count > 0 || mt_state != mt_state_idle) && tap_elapsed_ms > max_tap_period_ms) {
            peak_max = 0.0;
            mt_state = mt_state_idle;
            int final_tap_count = tap_count;
            tap_count = 0;

            if (final_tap_count > 0 && debug_multi_tap) fprintf(stdout, "\tdebug: detected multi-tap of %d\n", final_tap_count);
            return final_tap_count;
        } else {
            switch(mt_state) {
                case mt_state_idle: {
                    if (acceleration > mt_detect_threshold) {
                        tap_start_time = timestamp;
                        peak_max = 0.0;
                        mt_state = mt_state_rise;
                        if (debug_multi_tap) fprintf(stdout, "\tdebug: tap-rise detected %f\n", acceleration);
                    } else {
                        if (debug_multi_tap) {
                            if (acceleration > peak_max) peak_max = acceleration;
                            if ((timestamp - last_logged_peak_time)/1000000000 > 1) {
                                fprintf(stdout, "\tdebug: no-tap detected, peak was %f\n", peak_max);
                                peak_max = 0.0;
                                last_logged_peak_time = timestamp;
                            }
                        }
                    }
                    break;
                }
                case mt_state_rise: {
                    if (acceleration < 0) // accelerating in the opposite direction
                        if (tap_elapsed_ms > max_tap_rise_ms) {
                            if (debug_multi_tap) fprintf(stdout, "\tdebug: rise took %d, too long for a tap\n", tap_elapsed_ms);
                            peak_max = 0.0;
                            mt_state = mt_state_idle;
                            tap_count == 0;
                        } else {
                            tap_count++;
                            mt_state = mt_state_fall;
                        }
                    break;
                }
                case mt_state_fall: {
                    if (acceleration > 0) { // acceleration switches back, stopping the fall
                        pause_start_time = timestamp;
                        mt_state = mt_state_pause;
                    }
                    break;
                }
                case mt_state_pause: {
                    if (fabs(acceleration) < mt_pause_threshold) {
                        int pause_elapsed_ms = (timestamp - pause_start_time) / 1000000;
                        if (pause_elapsed_ms > min_pause_ms)
                            // paused long enough, wrap back around to idle where we can detect the next rise
                            mt_state = mt_state_idle;
                    } else {
                        // not idle, reset pause state timer
                        pause_start_time = timestamp;
                    }
                }
            }
        }
    }

    return 0;
}