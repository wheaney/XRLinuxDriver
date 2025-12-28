#pragma once

#include <stdbool.h>
#include <stdint.h>

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

struct imu_vec3_t {
	float x;
	float y;
	float z;
};

struct imu_pose_t {
	struct imu_quat_t orientation;
	struct imu_vec3_t position;
	struct imu_euler_t euler;
	bool has_orientation;
	bool has_position;
	uint32_t timestamp_ms;
};

extern const float pose_orientation_reset_data[16];
extern const float pose_position_reset_data[3];

typedef struct imu_euler_t imu_euler_type;
typedef struct imu_quat_t imu_quat_type;
typedef struct imu_vec3_t imu_vec3_type;
typedef struct imu_pose_t imu_pose_type;

float degree_to_radian(float deg);
float radian_to_degree(float rad);
imu_quat_type normalize_quaternion(imu_quat_type q);
imu_quat_type conjugate(imu_quat_type q);
imu_quat_type multiply_quaternions(imu_quat_type q1, imu_quat_type q2);
imu_quat_type euler_to_quaternion_xyz(imu_euler_type euler);
imu_quat_type euler_to_quaternion_zyx(imu_euler_type euler);
imu_quat_type euler_to_quaternion_zxy(imu_euler_type euler);
imu_euler_type quaternion_to_euler_xyz(imu_quat_type q);
imu_euler_type quaternion_to_euler_zyx(imu_quat_type q);
imu_euler_type quaternion_to_euler_zxy(imu_quat_type q);
imu_quat_type device_pitch_adjustment(float adjustment_degrees);
imu_vec3_type vector_rotate(imu_vec3_type v, imu_quat_type q);
bool quat_equal(imu_quat_type q1, imu_quat_type q2);

static inline void imu_pose_sync_euler_from_orientation(imu_pose_type *p) {
	if (!p) return;
	if (p->has_orientation) {
		p->euler = quaternion_to_euler_zyx(p->orientation);
	}
}
static inline void imu_pose_sync_orientation_from_euler(imu_pose_type *p) {
	if (!p) return;
	if (p->has_orientation) {
		p->orientation = euler_to_quaternion_zyx(p->euler);
	}
}