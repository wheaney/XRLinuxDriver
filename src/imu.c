#include "imu.h"

#include <math.h>

float degree_to_radian(float deg) {
    return deg * M_PI / 180.0f;
}

imu_quat_type normalize_quaternion(imu_quat_type q) {
	float magnitude = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
	q.w /= magnitude;
	q.x /= magnitude;
	q.y /= magnitude;
	q.z /= magnitude;

    return q;
}

imu_quat_type euler_to_quaternion(imu_vector_type euler) {
    // Convert degrees to radians
    float x = degree_to_radian(euler.x);
    float y = degree_to_radian(euler.y);
    float z = degree_to_radian(euler.z);

    // Compute the half angles
    float cx = cos(x * 0.5f);
    float cy = cos(y * 0.5f);
    float cz = cos(z * 0.5f);
    float sx = sin(x * 0.5f);
    float sy = sin(y * 0.5f);
    float sz = sin(z * 0.5f);

    // Compute the quaternion components
    imu_quat_type q = {
        .w = cx * cy * cz + sx * sy * sz,
        .x = sx * cy * cz - cx * sy * sz,
        .y = cx * sy * cz + sx * cy * sz,
        .z = cx * cy * sz - sx * sy * cz
    };

    return normalize_quaternion(q);
}

imu_quat_type multiply_quaternions(imu_quat_type q1, imu_quat_type q2) {
	imu_quat_type q = {
        .w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z,
        .x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
        .y = q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
        .z = q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w
	};

	return normalize_quaternion(q);
}

imu_vector_type quaternion_to_euler(imu_quat_type q) {
	imu_vector_type euler;

	// roll (x-axis rotation)
	double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	euler.x = atan2(sinr_cosp, cosr_cosp) * (180.0 / M_PI);

	// pitch (y-axis rotation)
	double sinp = 2 * (q.w * q.y - q.z * q.x);
	if (fabs(sinp) >= 1)
		euler.y = copysign(M_PI / 2, sinp) * (180.0 / M_PI); // use 90 degrees if out of range
	else
		euler.y = asin(sinp) * (180.0 / M_PI);

	// yaw (z-axis rotation)
	double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	euler.z = atan2(siny_cosp, cosy_cosp) * (180.0 / M_PI);

	return euler;
}