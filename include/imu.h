#pragma once

#include <stdbool.h>

struct imu_euler_t {
	float roll;
	float pitch;
	float yaw;
};

struct imu_quat_t {
	float x;
	float y;
	float z;
	float w;
};

extern const float imu_reset_data[16];

typedef struct imu_euler_t imu_euler_type;
typedef struct imu_quat_t imu_quat_type;

float degree_to_radian(float deg);
float radian_to_degree(float rad);
imu_quat_type normalize_quaternion(imu_quat_type q);
imu_quat_type conjugate(imu_quat_type q);
imu_quat_type multiply_quaternions(imu_quat_type q1, imu_quat_type q2);
imu_quat_type euler_to_quaternion(imu_euler_type euler);
imu_euler_type quaternion_to_euler(imu_quat_type q);
imu_quat_type device_pitch_adjustment(float adjustment_degrees);
bool quat_equal(imu_quat_type q1, imu_quat_type q2);