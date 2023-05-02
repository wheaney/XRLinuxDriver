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

#define DEVICE4_MSG_R_BRIGHTNESS 0x03
#define DEVICE4_MSG_W_BRIGHTNESS 0x04
#define DEVICE4_MSG_R_DISP_MODE 0x07
#define DEVICE4_MSG_W_DISP_MODE 0x08

#define DEVICE4_MSG_R_GLASSID 0x15
#define DEVICE4_MSG_R_DP7911_FW_VERSION 0x16
#define DEVICE4_MSG_R_DSP_VERSION 0x18
#define DEVICE4_MSG_W_CANCEL_ACTIVATION 0x19
#define DEVICE4_MSG_P_HEARTBEAT 0x1A
#define DEVICE4_MSG_W_SLEEP_TIME 0x1E

#define DEVICE4_MSG_R_DSP_APP_FW_VERSION 0x21
#define DEVICE4_MSG_R_MCU_APP_FW_VERSION 0x26
#define DEVICE4_MSG_R_ACTIVATION_TIME 0x29
#define DEVICE4_MSG_W_ACTIVATION_TIME 0x2A

#define DEVICE4_MSG_R_DP7911_FW_IS_UPDATE 0x3C
#define DEVICE4_MSG_W_UPDATE_DP 0x3D
#define DEVICE4_MSG_W_UPDATE_MCU_APP_FW_PREPARE 0x3E
#define DEVICE4_MSG_W_UPDATE_MCU_APP_FW_START 0x3F

#define DEVICE4_MSG_W_UPDATE_MCU_APP_FW_TRANSMIT 0x40
#define DEVICE4_MSG_W_UPDATE_MCU_APP_FW_FINISH 0x41
#define DEVICE4_MSG_W_BOOT_JUMP_TO_APP 0x42
#define DEVICE4_MSG_W_MCU_APP_JUMP_TO_BOOT 0x44
#define DEVICE4_MSG_W_UPDATE_DSP_APP_FW_PREPARE 0x45
#define DEVICE4_MSG_W_UPDATE_DSP_APP_FW_START 0x46
#define DEVICE4_MSG_W_UPDATE_DSP_APP_FW_TRANSMIT 0x47
#define DEVICE4_MSG_W_UPDATE_DSP_APP_FW_FINISH 0x48
#define DEVICE4_MSG_R_IS_NEED_UPGRADE_DSP_FW 0x49

#define DEVICE4_MSG_W_FORCE_UPGRADE_DSP_FW 0x69

#define DEVICE4_MSG_W_BOOT_UPDATE_MODE 0x1100
#define DEVICE4_MSG_W_BOOT_UPDATE_CONFIRM 0x1101
#define DEVICE4_MSG_W_BOOT_UPDATE_PREPARE 0x1102
#define DEVICE4_MSG_W_BOOT_UPDATE_START 0x1103
#define DEVICE4_MSG_W_BOOT_UPDATE_TRANSMIT 0x1104
#define DEVICE4_MSG_W_BOOT_UPDATE_FINISH 0x1105

#define DEVICE4_MSG_P_START_HEARTBEAT 0x6c02
#define DEVICE4_MSG_P_BUTTON_PRESSED 0x6C05
#define DEVICE4_MSG_P_END_HEARTBEAT 0x6c12
#define DEVICE4_MSG_P_ASYNC_TEXT_LOG 0x6c09

#define DEVICE4_MSG_E_DSP_ONE_PACKGE_WRITE_FINISH 0x6C0E
#define DEVICE4_MSG_E_DSP_UPDATE_PROGRES 0x6C10
#define DEVICE4_MSG_E_DSP_UPDATE_ENDING 0x6C11

#define DEVICE4_BUTTON_PHYS_DISPLAY_TOGGLE 0x1
#define DEVICE4_BUTTON_PHYS_BRIGHTNESS_UP 0x2
#define DEVICE4_BUTTON_PHYS_BRIGHTNESS_DOWN 0x3

#define DEVICE4_BUTTON_VIRT_DISPLAY_TOGGLE 0x1
#define DEVICE4_BUTTON_VIRT_BRIGHTNESS_UP 0x6
#define DEVICE4_BUTTON_VIRT_BRIGHTNESS_DOWN 0x7
#define DEVICE4_BUTTON_VIRT_MODE_UP 0x8
#define DEVICE4_BUTTON_VIRT_MODE_DOWN 0x9

#ifdef __cplusplus
extern "C" {
#endif

struct __attribute__((__packed__)) device4_packet_t {
	uint8_t head;
	uint32_t checksum;
	uint16_t length;
	uint64_t timestamp;
	uint16_t msgid;
	uint8_t reserved [5];
	union {
		char text [42];
		uint8_t data [42];
	};
};

enum device4_event_t {
	DEVICE4_EVENT_UNKNOWN 			= 0,
	DEVICE4_EVENT_SCREEN_ON 		= 1,
	DEVICE4_EVENT_SCREEN_OFF 		= 2,
	DEVICE4_EVENT_BRIGHTNESS_UP 	= 3,
	DEVICE4_EVENT_BRIGHTNESS_DOWN 	= 4,
	DEVICE4_EVENT_MESSAGE 			= 5,
};

typedef struct device4_packet_t device4_packet_type;
typedef enum device4_event_t device4_event_type;
typedef void (*device4_event_callback)(
		uint64_t timestamp,
		device4_event_type event,
		uint8_t brightness,
		const char* msg
);

struct device4_t {
	uint16_t vendor_id;
	uint16_t product_id;
	
	void* handle;

	bool activated;
	char mcu_app_fw_version [42];
	char dp_fw_version [42];
	char dsp_fw_version [42];
	
	bool active;
	uint8_t brightness;
	uint8_t disp_mode;
	
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
