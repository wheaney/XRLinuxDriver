/*
 * Copyright (C) 2025 VITURE Inc. All rights reserved.
 */

#ifndef VITURE_DEVICE_H
#define VITURE_DEVICE_H

#include "viture_glasses_provider.h"

// Callback function type for Viture device (combined IMU and VSync)
typedef void (*XRIMUAndVsyncCallback)(float* imu, float* euler, uint64_t ts, uint64_t vsync);

/**
 * Register callback for Viture device
 * @param handle Handle to the XRDeviceProvider instance
 * @param imu_vsync_callback Combined IMU and VSync callback
 * @return 0 on success, -1 on failure
 */
int register_callback(XRDeviceProviderHandle handle, XRIMUAndVsyncCallback imu_vsync_callback);

#endif // VITURE_DEVICE_H
