#include "imu.h"

#include <math.h>

imu_quat_type multiply_quaternions(imu_quat_type q1, imu_quat_type q2) {
	imu_quat_type result;

	result.w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z;
	result.x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y;
	result.y = q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x;
	result.z = q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w;

	// Normalize the quaternion
	float magnitude = sqrt(result.w*result.w + result.x*result.x + result.y*result.y + result.z*result.z);
	result.w /= magnitude;
	result.x /= magnitude;
	result.y /= magnitude;
	result.z /= magnitude;

	return result;
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