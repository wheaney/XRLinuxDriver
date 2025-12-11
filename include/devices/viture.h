#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "devices.h"
#include "imu.h"

extern const device_properties_type viture_one_properties;
extern const device_driver_type viture_driver;

#ifdef VITURE_INTERNAL
static bool viture_display_mode_is_sbs(int mode);
static void viture_refresh_sbs_state_locked(void);
static void viture_publish_pose(imu_quat_type orientation, bool has_position, imu_vec3_type position, uint32_t timestamp_ms);
static void viture_legacy_imu_callback(float *imu, float *euler, uint64_t ts, uint64_t vsync);
static void viture_carina_pose_callback(float *pose, double timestamp);
static void viture_carina_vsync_callback(double timestamp);
static void viture_carina_imu_callback(float *imu, double timestamp);
static void viture_carina_camera_callback(char *image_left0,
										  char *image_right0,
										  char *image_left1,
										  char *image_right1,
										  double timestamp,
										  int width,
										  int height);
static bool viture_initialize_provider_locked(uint16_t product_id);
static bool viture_start_stream_locked(void);
static void viture_stop_stream_locked(void);
static void viture_shutdown_provider_locked(bool soft);
static void viture_update_device_properties(device_properties_type *device);
static bool viture_open_imu_locked();
static void viture_capture_and_override_display_mode_locked(void);
static void viture_restore_display_mode_locked(void);
#endif