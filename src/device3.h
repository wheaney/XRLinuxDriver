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

#include <stdbool.h>
#include <stdint.h>

#include <Fusion/Fusion.h>
#include <libusb-1.0/libusb.h>

struct __attribute__((__packed__)) device3_packet_t {
	uint8_t signature [2];
	uint8_t _padding0 [2];
	uint64_t timestamp;
	uint8_t _padding1 [6];
	uint8_t angular_velocity_x [3];
	uint8_t angular_velocity_y [3];
	uint8_t angular_velocity_z [3];
	uint8_t _padding2 [6];
	uint8_t acceleration_x [3];
	uint8_t acceleration_y [3];
	uint8_t acceleration_z [3];
	uint8_t _padding3 [6];
	int16_t magnetic_x;
	int16_t magnetic_y;
	int16_t magnetic_z;
	uint32_t checksum;
	uint8_t _padding4 [6];
};

enum device3_event_t {
	DEVICE3_EVENT_UNKNOWN 			= 0,
	DEVICE3_EVENT_INIT 				= 1,
	DEVICE3_EVENT_UPDATE 			= 2,
};

typedef struct device3_packet_t device3_packet_type;
typedef enum device3_event_t device3_event_type;
typedef void (*device3_event_callback)(
		uint64_t timestamp,
		device3_event_type event,
		const FusionAhrs* ahrs
);

struct device3_t {
	uint16_t vendor_id;
	uint16_t product_id;
	
	libusb_context* context;
	libusb_device_handle* handle;
	
	uint8_t interface_number;
	
	uint8_t endpoint_address_in;
	uint8_t max_packet_size_in;
	
	uint8_t endpoint_address_out;
	uint8_t max_packet_size_out;
	
	bool detached;
	bool claimed;
	
	uint64_t last_timestamp;
	
	FusionOffset offset;
	FusionAhrs ahrs;
	
	device3_event_callback callback;
};

typedef struct device3_t device3_type;

device3_type* device3_open(device3_event_callback callback);

int device3_read(device3_type* device, int timeout);

void device3_close(device3_type* device);
