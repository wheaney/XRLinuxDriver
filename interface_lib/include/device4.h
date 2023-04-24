#pragma once
//
// Created by thejackimonster on 29.03.23.
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

#define DEVICE4_ACTION_PASSIVE_POLL_START 	0b00010
#define DEVICE4_ACTION_BRIGHTNESS_COMMAND 	0b00011
#define DEVICE4_ACTION_MANUAL_POLL_CLICK 	0b00101
#define DEVICE4_ACTION_ACTIVE_POLL 			0b01001
#define DEVICE4_ACTION_PASSIVE_POLL_END 	0b10010

#define DEVICE4_BUTTON_DISPLAY_TOGGLE 	0x1
#define DEVICE4_BUTTON_BRIGHTNESS_UP 	0x2
#define DEVICE4_BUTTON_BRIGHTNESS_DOWN 	0x3

#ifdef __cplusplus
extern "C" {
#endif

struct __attribute__((__packed__)) device4_packet_t {
	uint8_t signature;
	uint32_t checksum;
	uint16_t length;
	uint8_t _padding0 [4];
	uint32_t timestamp;
	uint8_t action;
	uint8_t _padding1 [6];
	union {
		char text [42];
		uint8_t data [42];
	};
};

enum device4_event_t {
	DEVICE4_EVENT_UNKNOWN 			= 0,
	DEVICE4_EVENT_SCREEN_ON 		= 1,
	DEVICE4_EVENT_SCREEN_OFF 		= 2,
	DEVICE4_EVENT_BRIGHTNESS_SET 	= 3,
	DEVICE4_EVENT_BRIGHTNESS_UP 	= 4,
	DEVICE4_EVENT_BRIGHTNESS_DOWN 	= 5,
	DEVICE4_EVENT_MESSAGE 			= 6,
};

typedef struct device4_packet_t device4_packet_type;
typedef enum device4_event_t device4_event_type;
typedef void (*device4_event_callback)(
		uint32_t timestamp,
		device4_event_type event,
		uint8_t brightness,
		const char* msg
);

struct device4_t {
	uint16_t vendor_id;
	uint16_t product_id;
	
	void* context;
	void* handle;
	
	uint8_t interface_number;
	
	uint8_t endpoint_address_in;
	uint8_t max_packet_size_in;
	
	uint8_t endpoint_address_out;
	uint8_t max_packet_size_out;
	
	bool detached;
	bool claimed;
	bool active;
	
	uint8_t brightness;
	
	device4_event_callback callback;
};

typedef struct device4_t device4_type;

device4_type* device4_open(device4_event_callback callback);

void device4_clear(device4_type* device);

int device4_read(device4_type* device, int timeout);

void device4_close(device4_type* device);

#ifdef __cplusplus
} // extern "C"
#endif
