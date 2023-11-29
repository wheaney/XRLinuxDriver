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