/*
 * Copyright (C) 2025 VITURE Inc. All rights reserved.
 */

#ifndef VITURE_DEVICE_CARINA_H
#define VITURE_DEVICE_CARINA_H

#include "viture_glasses_provider.h"

// Callback function types for Carina device
typedef void (*XRPoseCallback)(float* pose, double timestamp);

typedef void (*XRVSyncCallback)(double timestamp);

typedef void (*XRImuCallback)(float* imu, double timestamp);

typedef void (*XRCameraCallback)(char* image_left0,
                                 char* image_right0,
                                 char* image_left1,
                                 char* image_right1,
                                 double timestamp,
                                 int width,
                                 int height);

/**
 * Register callbacks for Carina device (ignored for other device types)
 * @param handle Handle to the XRDeviceProvider instance
 * @param pose_callback Pose data callback
 * @param vsync_callback VSync callback
 * @param imu_callback IMU data callback
 * @param camera_callback Camera data callback
 * @return 0 on success, -1 on failure
 */
int register_callbacks_carina(XRDeviceProviderHandle handle,
                              XRPoseCallback pose_callback,
                              XRVSyncCallback vsync_callback,
                              XRImuCallback imu_callback,
                              XRCameraCallback camera_callback);

/**
 * Execute reset pose command synchronously
 * @param handle Handle to the XRDeviceProvider instance
 * @return 0 on success, -1 on failure   ----> carina
 */
int xr_device_provider_reset_pose(XRDeviceProviderHandle handle);

/**
 * Get GL pose with prediction time
 * @param handle Handle to the XRDeviceProvider instance
 * @param pose Array to store pose data (7 floats for position and quaternion)
 * @param predict_time Prediction time in seconds
 * @return 0 on success, -1 on failure ---->>carina
 */
int xr_device_provider_get_gl_pose(XRDeviceProviderHandle handle, float* pose, double predict_time);

#endif // VITURE_DEVICE_CARINA_H
