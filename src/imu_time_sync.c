// Implementation of IMU time sync using existing project types and math helpers
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include <stdint.h>

#include "imu_time_sync.h"
#include "imu.h"
#include "buffer.h"

static inline float quat_angular_distance_f(imu_quat_type q1, imu_quat_type q2) {
    // Compute relative quaternion: q_rel = q2 * conj(q1)
    imu_quat_type q_rel = multiply_quaternions(q2, conjugate(q1));
    float w = q_rel.w;
    if (w < 0.0f) w = -w; // |w|
    if (w > 1.0f) w = 1.0f; // clamp
    return 2.0f * acosf(w);
}

// Resample signal to target length using linear interpolation
static float* resample_signal(const float* input, int input_len, int output_len) {
    float* output = (float*)malloc((size_t)output_len * sizeof(float));
    for (int i = 0; i < output_len; i++) {
        float src_idx = (float)i * (float)(input_len - 1) / (float)(output_len - 1);
        int idx0 = (int)src_idx;
        int idx1 = idx0 + 1;
        if (idx1 >= input_len) {
            output[i] = input[input_len - 1];
        } else {
            float t = src_idx - (float)idx0;
            output[i] = input[idx0] * (1.0f - t) + input[idx1] * t;
        }
    }
    return output;
}

// Normalize signal to zero mean and unit variance
static void normalize_signal(float* signal, int length) {
    // Compute mean
    float mean = 0.0f;
    for (int i = 0; i < length; i++) {
        mean += signal[i];
    }
    mean /= (float)length;
    
    // Remove mean
    for (int i = 0; i < length; i++) {
        signal[i] -= mean;
    }
    
    // Compute standard deviation
    float variance = 0.0f;
    for (int i = 0; i < length; i++) {
        variance += signal[i] * signal[i];
    }
    float std = sqrtf(variance / (float)length);
    
    // Normalize to unit variance
    if (std > 1e-6f) {
        for (int i = 0; i < length; i++) {
            signal[i] /= std;
        }
    }
}

// Cross-correlation using FFT (much faster than naive approach)
static float* cross_correlate_fft(const float* signal1, int len1, const float* signal2, int len2, int* out_len) {
    // Output length for full cross-correlation
    *out_len = len1 + len2 - 1;
    
    // FFT size (next power of 2 for efficiency)
    int fft_size = 1;
    while (fft_size < *out_len) {
        fft_size *= 2;
    }
    
    // Allocate aligned memory for FFTW
    float* padded1 = (float*)fftwf_malloc((size_t)fft_size * sizeof(float));
    float* padded2 = (float*)fftwf_malloc((size_t)fft_size * sizeof(float));
    fftwf_complex* freq1 = (fftwf_complex*)fftwf_malloc((size_t)(fft_size/2 + 1) * sizeof(fftwf_complex));
    fftwf_complex* freq2 = (fftwf_complex*)fftwf_malloc((size_t)(fft_size/2 + 1) * sizeof(fftwf_complex));
    fftwf_complex* product = (fftwf_complex*)fftwf_malloc((size_t)(fft_size/2 + 1) * sizeof(fftwf_complex));
    float* result_full = (float*)fftwf_malloc((size_t)fft_size * sizeof(float));
    
    // Zero-pad signals
    memset(padded1, 0, (size_t)fft_size * sizeof(float));
    memset(padded2, 0, (size_t)fft_size * sizeof(float));
    memcpy(padded1, signal1, (size_t)len1 * sizeof(float));
    memcpy(padded2, signal2, (size_t)len2 * sizeof(float));
    
    // Create FFT plans
    fftwf_plan plan_forward1 = fftwf_plan_dft_r2c_1d(fft_size, padded1, freq1, FFTW_ESTIMATE);
    fftwf_plan plan_forward2 = fftwf_plan_dft_r2c_1d(fft_size, padded2, freq2, FFTW_ESTIMATE);
    fftwf_plan plan_inverse = fftwf_plan_dft_c2r_1d(fft_size, product, result_full, FFTW_ESTIMATE);
    
    // Execute forward FFTs
    fftwf_execute(plan_forward1);
    fftwf_execute(plan_forward2);
    
    // Multiply in frequency domain: product = freq1 * conj(freq2)
    for (int i = 0; i < fft_size/2 + 1; i++) {
        float real1 = freq1[i][0];
        float imag1 = freq1[i][1];
        float real2 = freq2[i][0];
        float imag2 = freq2[i][1];
        
        // Complex multiplication with conjugate
        product[i][0] = real1 * real2 + imag1 * imag2;
        product[i][1] = imag1 * real2 - real1 * imag2;
    }
    
    // Inverse FFT to get correlation
    fftwf_execute(plan_inverse);
    
    // Normalize by FFT size
    for (int i = 0; i < fft_size; i++) {
        result_full[i] /= (float)fft_size;
    }
    
    // Extract the relevant part and rearrange for "full" mode
    // FFTW gives [0...N-1], we want it centered
    float* result = (float*)malloc((size_t)(*out_len) * sizeof(float));
    int mid = len1 - 1;
    
    for (int i = 0; i < *out_len; i++) {
        if (i <= mid) {
            result[i] = result_full[fft_size - mid + i];
        } else {
            result[i] = result_full[i - mid - 1];
        }
    }
    
    // Cleanup
    fftwf_destroy_plan(plan_forward1);
    fftwf_destroy_plan(plan_forward2);
    fftwf_destroy_plan(plan_inverse);
    fftwf_free(padded1);
    fftwf_free(padded2);
    fftwf_free(freq1);
    fftwf_free(freq2);
    fftwf_free(product);
    fftwf_free(result_full);
    
    return result;
}

// Find index of maximum value in array
static int find_max_index(const float* array, int length) {
    int max_idx = 0;
    float max_val = array[0];
    
    for (int i = 1; i < length; i++) {
        if (array[i] > max_val) {
            max_val = array[i];
            max_idx = i;
        }
    }
    
    return max_idx;
}

// Parabolic interpolation for sub-sample accuracy
static float parabolic_interpolation(float y1, float y2, float y3) {
    float denom = y1 - 2.0f*y2 + y3;
    if (fabsf(denom) < 1e-6f) {
        return 0.0;
    }
    return 0.5f * (y1 - y3) / denom;
}

IMUTimeSync* imu_time_sync_create(float window_duration,
                                  float sampling_rate1,
                                  float sampling_rate2) {
    IMUTimeSync* sync = (IMUTimeSync*)malloc(sizeof(IMUTimeSync));
    if (!sync) return NULL;
    sync->window_duration = window_duration;
    sync->sampling_rate1 = sampling_rate1;
    sync->sampling_rate2 = sampling_rate2;
    // Use the higher sampling rate to determine buffer size (20% margin)
    float max_rate = fmaxf(sampling_rate1, sampling_rate2);
    sync->window_samples = (int)(window_duration * max_rate * 1.2f);
    if (sync->window_samples < 8) sync->window_samples = 8;
    sync->buffer1 = create_buffer(sync->window_samples);
    sync->buffer2 = create_buffer(sync->window_samples);
    sync->has_prev_quat[0] = false;
    sync->has_prev_quat[1] = false;
    sync->has_prev_euler[0] = false;
    sync->has_prev_euler[1] = false;
    return sync;
}

void imu_time_sync_destroy(IMUTimeSync* sync) {
    if (!sync) return;
    // buffer.h does not expose a destructor; rely on program lifetime or extend API if needed.
    // For now, just free the struct.
    free(sync);
}

void imu_time_sync_add_quaternion_sample(IMUTimeSync* sync,
                                         int source_index,
                                         imu_quat_type q) {
    if (!sync) return;
    buffer_type* buf = (source_index == 0) ? sync->buffer1 : sync->buffer2;
    // Normalize
    q = normalize_quaternion(q);
    if (!sync->has_prev_quat[source_index]) {
        sync->prev_quat[source_index] = q;
        sync->has_prev_quat[source_index] = true;
        push(buf, 0.0f);
        return;
    }
    float dist = quat_angular_distance_f(sync->prev_quat[source_index], q);
    push(buf, dist);
    sync->prev_quat[source_index] = q;
}

void imu_time_sync_add_euler_sample(IMUTimeSync* sync,
                                    int source_index,
                                    imu_euler_type euler) {
    if (!sync) return;
    buffer_type* buf = (source_index == 0) ? sync->buffer1 : sync->buffer2;
    if (!sync->has_prev_euler[source_index]) {
        sync->prev_euler[source_index] = euler;
        sync->has_prev_euler[source_index] = true;
        push(buf, 0.0f);
        return;
    }
    // Compute differences with wraparound handling (degrees)
    float dx = atan2f(sinf(degree_to_radian(euler.roll - sync->prev_euler[source_index].roll)), cosf(degree_to_radian(euler.roll - sync->prev_euler[source_index].roll)));
    float dy = atan2f(sinf(degree_to_radian(euler.pitch - sync->prev_euler[source_index].pitch)), cosf(degree_to_radian(euler.pitch - sync->prev_euler[source_index].pitch)));
    float dz = atan2f(sinf(degree_to_radian(euler.yaw - sync->prev_euler[source_index].yaw)), cosf(degree_to_radian(euler.yaw - sync->prev_euler[source_index].yaw)));
    float mag = sqrtf(dx*dx + dy*dy + dz*dz);
    push(buf, mag);
    sync->prev_euler[source_index] = euler;
}

bool imu_time_sync_is_ready(IMUTimeSync* sync) {
    int min_samples1 = (int)(sync->window_duration * sync->sampling_rate1);
    int min_samples2 = (int)(sync->window_duration * sync->sampling_rate2);
    return (sync->buffer1 && sync->buffer2 &&
            (sync->buffer1->count >= min_samples1 ||
            sync->buffer2->count >= min_samples2));
}

bool imu_time_sync_compute_offset(IMUTimeSync* sync,
                                  float* out_offset_seconds,
                                  float* out_confidence) {
    if (!imu_time_sync_is_ready(sync)) {
        return false;  // Not enough data
    }

    // Extract data from ring buffers
    int len1 = sync->buffer1->count;
    int len2 = sync->buffer2->count;

    float* signal1 = (float*)malloc((size_t)len1 * sizeof(float));
    float* signal2 = (float*)malloc((size_t)len2 * sizeof(float));

    // Reconstruct chronological order from circular buffer
    for (int i = 0; i < len1; i++) {
        int idx = (sync->buffer1->index - len1 + i + sync->buffer1->size) % sync->buffer1->size;
        signal1[i] = sync->buffer1->values[idx];
    }
    for (int i = 0; i < len2; i++) {
        int idx = (sync->buffer2->index - len2 + i + sync->buffer2->size) % sync->buffer2->size;
        signal2[i] = sync->buffer2->values[idx];
    }

    // Resample to common length if needed
    int target_len = (int)fmaxf((float)len1, (float)len2);
    float* resampled1 = signal1;
    float* resampled2 = signal2;

    if (len1 != target_len) {
        resampled1 = resample_signal(signal1, len1, target_len);
        free(signal1);
    }
    if (len2 != target_len) {
        resampled2 = resample_signal(signal2, len2, target_len);
        free(signal2);
    }

    // Normalize signals
    normalize_signal(resampled1, target_len);
    normalize_signal(resampled2, target_len);

    // Compute cross-correlation using FFT (FAST!)
    int corr_len;
    float* correlation = cross_correlate_fft(resampled1, target_len,
                                             resampled2, target_len,
                                             &corr_len);

    // Find peak
    int max_idx = find_max_index(correlation, corr_len);

    // Refine with parabolic interpolation
    float lag_offset = 0.0f;
    if (max_idx > 0 && max_idx < corr_len - 1) {
        lag_offset = parabolic_interpolation(
            correlation[max_idx - 1],
            correlation[max_idx],
            correlation[max_idx + 1]
        );
    }

    // Convert lag to time offset
    int zero_lag = target_len - 1;
    float lag_samples = (float)(max_idx - zero_lag) + lag_offset;

    // Use the average sampling rate for conversion
    float avg_rate = (sync->sampling_rate1 + sync->sampling_rate2) / 2.0f;
    *out_offset_seconds = lag_samples / avg_rate;

    // Compute confidence (normalized correlation)
    *out_confidence = correlation[max_idx] / (float)target_len;

    // Cleanup
    free(resampled1);
    free(resampled2);
    free(correlation);

    return true;  // Success
}

void imu_time_sync_reset(IMUTimeSync* sync) {
    if (sync->buffer1) { sync->buffer1->index = 0; sync->buffer1->count = 0; }
    if (sync->buffer2) { sync->buffer2->index = 0; sync->buffer2->count = 0; }
    sync->has_prev_quat[0] = sync->has_prev_quat[1] = false;
    sync->has_prev_euler[0] = sync->has_prev_euler[1] = false;
}

// ---------------- Sampling rate estimator ----------------
struct imu_rate_estimator_t {
    uint32_t* ts_ms;
    size_t max_samples;
    size_t count;
};

imu_rate_estimator_t* imu_rate_estimator_create(size_t max_samples) {
    if (max_samples < 2) max_samples = 2;
    imu_rate_estimator_t* est = (imu_rate_estimator_t*)calloc(1, sizeof(*est));
    est->ts_ms = (uint32_t*)calloc(max_samples, sizeof(uint32_t));
    est->max_samples = max_samples;
    est->count = 0;
    return est;
}

void imu_rate_estimator_destroy(imu_rate_estimator_t* est) {
    if (!est) return;
    free(est->ts_ms);
    free(est);
}

void imu_rate_estimator_add(imu_rate_estimator_t* est, uint32_t timestamp_ms) {
    if (!est) return;
    if (est->count < est->max_samples) {
        est->ts_ms[est->count++] = timestamp_ms;
    } else {
        // shift left, drop oldest
        memmove(&est->ts_ms[0], &est->ts_ms[1], (est->max_samples - 1) * sizeof(uint32_t));
        est->ts_ms[est->max_samples - 1] = timestamp_ms;
    }
}

bool imu_rate_estimator_is_ready(imu_rate_estimator_t* est) {
    return est && est->count >= 100;
}

float imu_rate_estimator_get_rate_hz(imu_rate_estimator_t* est) {
    if (!imu_rate_estimator_is_ready(est)) return 0.0f;
    uint32_t first = est->ts_ms[0];
    uint32_t last = est->ts_ms[est->count - 1];
    if (last == first) return 0.0f;
    float seconds = (float)(last - first) / 1000.0f;
    float samples = (float)(est->count - 1);
    float rate = samples / seconds;
    // guard absurd rates
    if (rate < 0.1f) rate = 0.1f;
    return rate;
}

size_t imu_rate_estimator_count(imu_rate_estimator_t* est) {
    return est ? est->count : 0;
}

float imu_rate_estimator_duration_seconds(imu_rate_estimator_t* est) {
    if (!imu_rate_estimator_is_ready(est)) return 0.0f;
    uint32_t first = est->ts_ms[0];
    uint32_t last = est->ts_ms[est->count - 1];
    return (float)(last - first) / 1000.0f;
}