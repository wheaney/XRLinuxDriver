#include "imu.h"

#include <math.h>
#include <stdbool.h>

const float pose_orientation_reset_data[16] = {
    0.0, 0.0, 0.0, 1.0,
    0.0, 0.0, 0.0, 1.0,
    0.0, 0.0, 0.0, 1.0,
    0.0, 0.0, 0.0, 1.0
};
const float pose_position_reset_data[3] = {
    0.0, 0.0, 0.0
};

float degree_to_radian(float deg) {
    return deg * M_PI / 180.0f;
}

float radian_to_degree(float rad) {
    return rad * 180.0f / M_PI;
}

imu_quat_type normalize_quaternion(imu_quat_type q) {
    float magnitude = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (!isfinite(magnitude) || magnitude <= 0.0f) {
        imu_quat_type identity = { .w = 1.0f, .x = 0.0f, .y = 0.0f, .z = 0.0f };
        return identity;
    }
    q.w /= magnitude;
    q.x /= magnitude;
    q.y /= magnitude;
    q.z /= magnitude;

    return q;
}

imu_quat_type conjugate(imu_quat_type q) {
    imu_quat_type q_conj = {
        .w = q.w,
        .x = -q.x,
        .y = -q.y,
        .z = -q.z
    };

    return q_conj;
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

imu_quat_type euler_to_quaternion_xyz(imu_euler_type euler) {
    // Convert degrees to radians
    float roll = degree_to_radian(euler.roll);
    float pitch = degree_to_radian(euler.pitch);
    float yaw = degree_to_radian(euler.yaw);

    // Compute the half angles
    float cx = cos(roll * 0.5f);
    float cy = cos(pitch * 0.5f);
    float cz = cos(yaw * 0.5f);
    float sx = sin(roll * 0.5f);
    float sy = sin(pitch * 0.5f);
    float sz = sin(yaw * 0.5f);

    // Compute the quaternion components
    imu_quat_type q = {
        .x = sx * cy * cz + cx * sy * sz,
        .y = cx * sy * cz - sx * cy * sz,
        .z = cx * cy * sz + sx * sy * cz,
        .w = cx * cy * cz - sx * sy * sz
    };

    return normalize_quaternion(q);
}

imu_quat_type euler_to_quaternion_zyx(imu_euler_type euler) {
    // Convert degrees to radians
    float roll = degree_to_radian(euler.roll);
    float pitch = degree_to_radian(euler.pitch);
    float yaw = degree_to_radian(euler.yaw);

    // Compute the half angles
    float cx = cos(roll * 0.5f);
    float cy = cos(pitch * 0.5f);
    float cz = cos(yaw * 0.5f);
    float sx = sin(roll * 0.5f);
    float sy = sin(pitch * 0.5f);
    float sz = sin(yaw * 0.5f);

    // Compute the quaternion components
    imu_quat_type q = {
        .x = sx * cy * cz - cx * sy * sz,
        .y = cx * sy * cz + sx * cy * sz,
        .z = cx * cy * sz - sx * sy * cz,
        .w = cx * cy * cz + sx * sy * sz
    };

    return normalize_quaternion(q);
}

imu_quat_type euler_to_quaternion_zxy(imu_euler_type euler) {
    // Convert degrees to radians
    float roll = degree_to_radian(euler.roll);
    float pitch = degree_to_radian(euler.pitch);
    float yaw = degree_to_radian(euler.yaw);

    // Compute the half angles
    float cx = cos(roll * 0.5f);
    float cy = cos(pitch * 0.5f);
    float cz = cos(yaw * 0.5f);
    float sx = sin(roll * 0.5f);
    float sy = sin(pitch * 0.5f);
    float sz = sin(yaw * 0.5f);

    // Compute the quaternion components
    imu_quat_type q = {
        .x = sx * cy * cz - cx * sy * sz,
        .y = cx * sy * cz + sx * cy * sz,
        .z = cx * cy * sz + sx * sy * cz,
        .w = cx * cy * cz - sx * sy * sz
    };

    return normalize_quaternion(q);
}

imu_euler_type quaternion_to_euler_xyz(imu_quat_type q) {
    imu_euler_type euler;
    
    // Calculate roll (x-axis rotation)
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler.roll = radian_to_degree(atan2f(sinr_cosp, cosr_cosp));
    
    // Calculate pitch (y-axis rotation)
    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (fabsf(sinp) >= 1.0f) {
        // Use 90 degrees if out of range
        euler.pitch = radian_to_degree(copysignf(M_PI / 2.0f, sinp));
    } else {
        euler.pitch = radian_to_degree(asinf(sinp));
    }
    
    // Calculate yaw (z-axis rotation)
    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.yaw = radian_to_degree(atan2f(siny_cosp, cosy_cosp));
    
    return euler;
}

imu_euler_type quaternion_to_euler_zyx(imu_quat_type q) {
    imu_euler_type euler;
    
    // Calculate pitch (y-axis rotation)
    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (fabsf(sinp) >= 1.0f) {
        // Use 90 degrees if out of range
        euler.pitch = radian_to_degree(copysignf(M_PI / 2.0f, sinp));
        
        // Gimbal lock case
        // In the gimbal lock case with pitch at +/-90 degrees,
        // yaw and roll rotate around the same axis
        // We can choose any convention; here we set roll to 0
        // and calculate yaw
        euler.roll = 0.0f;
        float siny = 2.0f * (q.w * q.z + q.x * q.y);
        float cosy = 2.0f * (q.w * q.x - q.y * q.z);
        euler.yaw = radian_to_degree(atan2f(siny, cosy));
    } else {
        // Normal case
        euler.pitch = radian_to_degree(asinf(sinp));
        
        // Calculate yaw (z-axis rotation)
        float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
        float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        euler.yaw = radian_to_degree(atan2f(siny_cosp, cosy_cosp));
        
        // Calculate roll (x-axis rotation)
        float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
        float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        euler.roll = radian_to_degree(atan2f(sinr_cosp, cosr_cosp));
    }
    
    return euler;
}

imu_euler_type quaternion_to_euler_zxy(imu_quat_type q) {
    imu_euler_type euler;
    
    // Calculate roll (x-axis rotation)
    float sinr = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    euler.roll = radian_to_degree(atan2f(sinr, cosr));
    
    // Calculate pitch (y-axis rotation)
    float sinp = 2.0f * (q.w * q.y - q.x * q.z);
    if (fabsf(sinp) >= 1.0f) {
        // Use 90 degrees if out of range
        euler.pitch = radian_to_degree(copysignf(M_PI / 2.0f, sinp));
    } else {
        euler.pitch = radian_to_degree(asinf(sinp));
    }
    
    // Calculate yaw (z-axis rotation)
    float siny = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.yaw = radian_to_degree(atan2f(siny, cosy));
    
    return euler;
}

imu_quat_type device_pitch_adjustment(float adjustment_degrees) {
    float half = degree_to_radian(adjustment_degrees) * 0.5f;
    imu_quat_type q = {
        .w = cosf(half),
        .x = 0.0f,
        .y = sinf(half),
        .z = 0.0f
    };
    return q;
}

bool quat_equal(imu_quat_type q1, imu_quat_type q2) {
    return q1.w == q2.w && q1.x == q2.x && q1.y == q2.y && q1.z == q2.z;
}

imu_vec3_type vector_rotate(imu_vec3_type v, imu_quat_type q) {
    q = normalize_quaternion(q);

    float w = q.w;
    float qx = q.x, qy = q.y, qz = q.z;

    float tx = 2.0f * (qy * v.z - qz * v.y);
    float ty = 2.0f * (qz * v.x - qx * v.z);
    float tz = 2.0f * (qx * v.y - qy * v.x);

    imu_vec3_type out;
    out.x = v.x + w * tx + (qy * tz - qz * ty);
    out.y = v.y + w * ty + (qz * tx - qx * tz);
    out.z = v.z + w * tz + (qx * ty - qy * tx);

    return out;
}

float quat_small_angle_rad(imu_quat_type q1, imu_quat_type q2) {
    imu_quat_type q_rel = multiply_quaternions(conjugate(q1), q2);
    float v_norm = sqrtf(q_rel.x * q_rel.x + q_rel.y * q_rel.y + q_rel.z * q_rel.z);
    float w_abs = fabsf(q_rel.w);
    return 2.0f * atan2f(v_norm, w_abs);
}