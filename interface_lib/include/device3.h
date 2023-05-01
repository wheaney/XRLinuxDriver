#pragma once
//
// Created by thejackimonster on 30.03.23.
//
// Copyright (c) 2023 thejackimonster. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifndef __cplusplus
#include <stdint.h>
#else
#include <cstdint>
#endif

#define DEVICE3_MSG_GET_CAL_DATA_LENGTH 0x14
#define DEVICE3_MSG_CAL_DATA_GET_NEXT_SEGMENT 0x15
#define DEVICE3_MSG_ALLOCATE_CAL_DATA_BUFFER 0x16
#define DEVICE3_MSG_WRITE_CAL_DATA_SEGMENT 0x17
#define DEVICE3_MSG_FREE_CAL_BUFFER 0x18
#define DEVICE3_MSG_START_IMU_DATA 0x19
#define DEVICE3_MSG_GET_STATIC_ID 0x1A
#define DEVICE3_MSG_UNKNOWN 0x1D

#ifdef __cplusplus
extern "C" {
#endif

struct __attribute__((__packed__)) device3_packet_t {
	uint8_t signature [2];
	uint8_t temperature [2];
	uint64_t timestamp;
	uint8_t angular_multiplier [2];
	uint8_t angular_divisor [4];
	uint8_t angular_velocity_x [3];
	uint8_t angular_velocity_y [3];
	uint8_t angular_velocity_z [3];
	uint8_t acceleration_multiplier [2];
	uint8_t acceleration_divisor [4];
	uint8_t acceleration_x [3];
	uint8_t acceleration_y [3];
	uint8_t acceleration_z [3];
	uint8_t magnetic_multiplier [2];
	uint8_t magnetic_divisor [4];
	uint8_t magnetic_x [2];
	uint8_t magnetic_y [2];
	uint8_t magnetic_z [2];
	uint32_t checksum;
	uint8_t _padding [6];
};

enum device3_event_t {
	DEVICE3_EVENT_UNKNOWN 			= 0,
	DEVICE3_EVENT_INIT 				= 1,
	DEVICE3_EVENT_UPDATE 			= 2,
};

struct device3_ahrs_t;
struct device3_calibration_t;

struct device3_vec3_t {
	float x;
	float y;
	float z;
};

struct device3_quat_t {
	float x;
	float y;
	float z;
	float w;
};

typedef struct device3_packet_t device3_packet_type;
typedef enum device3_event_t device3_event_type;

typedef struct device3_ahrs_t device3_ahrs_type;
typedef struct device3_calibration_t device3_calibration_type;

typedef struct device3_vec3_t device3_vec3_type;
typedef struct device3_quat_t device3_quat_type;

typedef void (*device3_event_callback)(
		uint64_t timestamp,
		device3_event_type event,
		const device3_ahrs_type* ahrs
);

struct device3_t {
	uint16_t vendor_id;
	uint16_t product_id;
	
	void* handle;
	
	uint32_t static_id;
	
	uint64_t last_timestamp;
	float temperature; // (in °C)
	
	void* offset;
	device3_ahrs_type* ahrs;
	
	device3_event_callback callback;
	device3_calibration_type* calibration;
};

typedef struct device3_t device3_type;

device3_type* device3_open(device3_event_callback callback);

void device3_reset_calibration(device3_type* device);

int device3_load_calibration(device3_type* device, const char* path);

int device3_save_calibration(device3_type* device, const char* path);

void device3_clear(device3_type* device);

int device3_calibrate(device3_type* device, uint32_t iterations, bool gyro, bool accel, bool magnet);

int device3_read(device3_type* device, int timeout);

device3_vec3_type device3_get_earth_acceleration(const device3_ahrs_type* ahrs);

device3_vec3_type device3_get_linear_acceleration(const device3_ahrs_type* ahrs);

device3_quat_type device3_get_orientation(const device3_ahrs_type* ahrs);

device3_vec3_type device3_get_euler(device3_quat_type quat);

void device3_close(device3_type* device);

#ifdef __cplusplus
} // extern "C"
#endif
