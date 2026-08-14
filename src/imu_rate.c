#include "epoch.h"
#include "imu_rate.h"
#include "logging.h"
#include "runtime_context.h"

#include <math.h>
#include <stdint.h>

#define IMU_RATE_WINDOW_MS 1000
#define IMU_RATE_MIN_WINDOW_SAMPLES 10
#define IMU_RATE_MAX_WINDOW_MS (IMU_RATE_WINDOW_MS * 3)

// how far the measured rate must sit from the device's current rate before we consider changing it
#define IMU_RATE_DEVIATION_RATIO 0.10f

// how closely consecutive measurements must agree to be considered stable
#define IMU_RATE_STABILITY_RATIO 0.05f

// how many agreeing measurements are needed before the device is updated
#define IMU_RATE_STABLE_WINDOWS 3

static uint64_t window_start_ms = 0;
static uint32_t window_samples = 0;
static float candidate_rate = 0.0f;
static int stable_windows = 0;

void imu_rate_reset() {
    window_start_ms = 0;
    window_samples = 0;
    candidate_rate = 0.0f;
    stable_windows = 0;
}

static void start_window(uint64_t now_ms) {
    window_start_ms = now_ms;
    window_samples = 0;
}

static bool apply_rate(device_properties_type* device, int rate) {
    int previous_rate = device->imu_cycles_per_s;
    if (rate == previous_rate) return false;

    int buffer_size = previous_rate > 0 ?
        (int)lroundf((float)device->imu_buffer_size * rate / previous_rate) :
        device->imu_buffer_size;
    if (buffer_size < 1) buffer_size = 1;

    device->imu_cycles_per_s = rate;
    device->imu_buffer_size = buffer_size;

    if (config()->debug_device) log_debug("Device is delivering IMU data at %dHz, was expecting %dHz\n", rate, previous_rate);
    return true;
}

bool imu_rate_observe_pose(device_properties_type* device) {
    if (device == NULL) return false;

    uint64_t now_ms = get_epoch_time_ms();
    if (window_start_ms == 0) {
        start_window(now_ms);
        return false;
    }

    window_samples++;
    uint64_t elapsed_ms = now_ms - window_start_ms;
    if (elapsed_ms < IMU_RATE_WINDOW_MS) return false;

    // a stalled or resumed stream says nothing about the steady-state rate
    if (elapsed_ms > IMU_RATE_MAX_WINDOW_MS || window_samples < IMU_RATE_MIN_WINDOW_SAMPLES) {
        start_window(now_ms);
        candidate_rate = 0.0f;
        stable_windows = 0;
        return false;
    }

    float measured_rate = (float)window_samples * 1000.0f / (float)elapsed_ms;
    start_window(now_ms);

    if (stable_windows == 0 || fabsf(measured_rate - candidate_rate) > candidate_rate * IMU_RATE_STABILITY_RATIO) {
        candidate_rate = measured_rate;
        stable_windows = 1;
    } else {
        candidate_rate += (measured_rate - candidate_rate) / (float)(stable_windows + 1);
        stable_windows++;
    }

    if (stable_windows < IMU_RATE_STABLE_WINDOWS) return false;

    float deviation = fabsf(candidate_rate - (float)device->imu_cycles_per_s);
    if (deviation <= (float)device->imu_cycles_per_s * IMU_RATE_DEVIATION_RATIO) return false;

    int rate = (int)lroundf(candidate_rate);
    if (rate < 1) return false;

    return apply_rate(device, rate);
}
