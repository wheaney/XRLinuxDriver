#pragma once

#include "buffer.h"
#include "imu.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // per-stream scalar signal buffers (angular deltas)
    buffer_type* buffer1;
    buffer_type* buffer2;

    // per-stream previous samples for delta computations
    imu_quat_type prev_quat[2];
    bool has_prev_quat[2];
    imu_euler_type prev_euler[2];
    bool has_prev_euler[2];

    float sampling_rate1;
    float sampling_rate2;
    int window_samples;
    float window_duration;
} IMUTimeSync;

IMUTimeSync* imu_time_sync_create(float window_duration,
                                  float sampling_rate1,
                                  float sampling_rate2);
void imu_time_sync_destroy(IMUTimeSync* sync);

// Add a quaternion sample for a specific source (0 or 1)
void imu_time_sync_add_quaternion_sample(IMUTimeSync* sync,
                                         int source_index,
                                         imu_quat_type q);

// Add an Euler sample (degrees) for a specific source (0 or 1)
void imu_time_sync_add_euler_sample(IMUTimeSync* sync,
                                    int source_index,
                                    imu_euler_type euler);

bool imu_time_sync_is_ready(IMUTimeSync* sync);

// Returns true on success; out_offset_seconds is positive when signal2 lags signal1
bool imu_time_sync_compute_offset(IMUTimeSync* sync,
                                  float* out_offset_seconds,
                                  float* out_confidence);

void imu_time_sync_reset(IMUTimeSync* sync);

// --- Sampling rate estimation helpers ---
typedef struct imu_rate_estimator_t imu_rate_estimator_t;

// Create an estimator that aggregates up to max_samples timestamps (in ms)
imu_rate_estimator_t* imu_rate_estimator_create(size_t max_samples);
void imu_rate_estimator_destroy(imu_rate_estimator_t* est);

// Add a new IMU sample timestamp in milliseconds
void imu_rate_estimator_add(imu_rate_estimator_t* est, uint32_t timestamp_ms);

// Return true when at least 2 samples exist to compute a rate
bool imu_rate_estimator_is_ready(imu_rate_estimator_t* est);

// Returns estimated sampling rate in Hz using (N-1) / ((t_last - t_first)/1000)
float imu_rate_estimator_get_rate_hz(imu_rate_estimator_t* est);

// Number of timestamps currently accumulated
size_t imu_rate_estimator_count(imu_rate_estimator_t* est);

// Span covered by accumulated timestamps in seconds (0 if <2 samples)
float imu_rate_estimator_duration_seconds(imu_rate_estimator_t* est);

#ifdef __cplusplus
}
#endif
