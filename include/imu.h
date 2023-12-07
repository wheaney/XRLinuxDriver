#pragma once

struct imu_vector_t {
	float x;
	float y;
	float z;
};

struct imu_quat_t {
	float x;
	float y;
	float z;
	float w;
};

typedef struct imu_vector_t imu_vector_type;
typedef struct imu_quat_t imu_quat_type;

float degree_to_radian(float deg);
imu_quat_type normalize_quaternion(imu_quat_type q);
imu_quat_type multiply_quaternions(imu_quat_type q1, imu_quat_type q2);
imu_quat_type euler_to_quaternion(imu_vector_type euler);
imu_vector_type quaternion_to_euler(imu_quat_type q);