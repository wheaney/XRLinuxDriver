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

#include "device4.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <hidapi/hidapi.h>

#include "crc32.h"

#define MAX_PACKET_SIZE 64
#define PACKET_HEAD 0xFD

static bool send_payload(device4_type* device, uint8_t size, const uint8_t* payload) {
	int payload_size = size;
	if (payload_size > MAX_PACKET_SIZE) {
		payload_size = MAX_PACKET_SIZE;
	}
	
	int transferred = hid_write(device->handle, payload, payload_size);
	
	if (transferred != payload_size) {
		perror("ERROR: sending payload failed\n");
		return false;
	}
	
	return (transferred == size);
}

static bool recv_payload(device4_type* device, uint8_t size, uint8_t* payload) {
	int payload_size = size;
	if (payload_size > MAX_PACKET_SIZE) {
		payload_size = MAX_PACKET_SIZE;
	}
	
	int transferred = hid_read(device->handle, payload, payload_size);
	
	if (transferred >= payload_size) {
		transferred = payload_size;
	}
	
	if (transferred == 0) {
		return false;
	}
	
	if (transferred != payload_size) {
		perror("ERROR: receiving payload failed\n");
		return false;
	}
	
	return (transferred == size);
}

static bool send_payload_action(device4_type* device, uint16_t msgid, uint8_t len, const uint8_t* data) {
	static device4_packet_type packet;
	
	const uint16_t packet_len = 17 + len;
	const uint16_t payload_len = 5 + packet_len;
	
	packet.head = PACKET_HEAD;
	packet.length = packet_len;
	packet.timestamp = 0;
	packet.msgid = msgid;
	memset(packet.reserved, 0, 5);
	
	memcpy(packet.data, data, len);
	packet.checksum = crc32_checksum((const uint8_t*) (&packet.length), packet.length);

	return send_payload(device, payload_len, (uint8_t*) (&packet));
}

static bool recv_payload_msg(device4_type* device, uint8_t msgid, uint8_t len, uint8_t* data) {
	static device4_packet_type packet;
	
	packet.head = 0;
	packet.length = 0;
	packet.msgid = 0;
	
	const uint16_t packet_len = 18 + len;
	const uint16_t payload_len = 5 + packet_len;
	
	if (!recv_payload(device, payload_len, (uint8_t*) (&packet))) {
		return false;
	}

	if (packet.head != PACKET_HEAD) {
		return false;
	}
	
	if (packet.msgid != msgid) {
		return false;
	}
	
	const uint8_t status = packet.data[0];

	if (status != 0) {
		return false;
	}

	memcpy(data, packet.data + 1, len);
	return true;
}

device4_type* device4_open(device4_event_callback callback) {
	device4_type* device = (device4_type*) malloc(sizeof(device4_type));
	
	if (!device) {
		perror("Not allocated!\n");
		return NULL;
	}
	
	memset(device, 0, sizeof(device4_type));
	device->vendor_id 	= 0x3318;
	device->product_id 	= 0x0424;
	device->callback 	= callback;

	if (0 != hid_init()) {
		perror("Not initialized!\n");
		return device;
	}

	struct hid_device_info* info = hid_enumerate(
		device->vendor_id, 
		device->product_id
	);

	struct hid_device_info* it = info;
	while (it) {
		if (it->interface_number == 4) {
			device->handle = hid_open_path(it->path);
			break;
		}

		it = it->next;
	}

	hid_free_enumeration(info);

	if (!device->handle) {
		perror("No handle!\n");
		return device;
	}

	device4_clear(device);

	if (!send_payload_action(device, DEVICE4_MSG_R_ACTIVATION_TIME, 0, NULL)) {
		perror("Requesting activation time failed!\n");
		return device;
	}

	uint8_t activated;
	if (!recv_payload_msg(device, DEVICE4_MSG_R_ACTIVATION_TIME, 1, &activated)) {
		perror("Receiving activation time failed!\n");
		return device;
	}

	device->activated = (activated != 0);

	if (!device->activated) {
		perror("Device is not activated!\n");
		return device;
	}

	if (!send_payload_action(device, DEVICE4_MSG_R_MCU_APP_FW_VERSION, 0, NULL)) {
		perror("Requesting current MCU app firmware version!\n");
		return device;
	}

	if (!recv_payload_msg(device, DEVICE4_MSG_R_MCU_APP_FW_VERSION, 41, (uint8_t*) device->mcu_app_fw_version)) {
		perror("Receiving current MCU app firmware version failed!\n");
		return device;
	}

	if (!send_payload_action(device, DEVICE4_MSG_R_DP7911_FW_VERSION, 0, NULL)) {
		perror("Requesting current DP firmware version!\n");
		return device;
	}

	if (!recv_payload_msg(device, DEVICE4_MSG_R_DP7911_FW_VERSION, 41, (uint8_t*) device->dp_fw_version)) {
		perror("Receiving current DP firmware version failed!\n");
		return device;
	}

	if (!send_payload_action(device, DEVICE4_MSG_R_DSP_APP_FW_VERSION, 0, NULL)) {
		perror("Requesting current DSP app firmware version!\n");
		return device;
	}

	if (!recv_payload_msg(device, DEVICE4_MSG_R_DSP_APP_FW_VERSION, 41, (uint8_t*) device->dsp_fw_version)) {
		perror("Receiving current DSP app firmware version failed!\n");
		return device;
	}

	printf("MCU: %s\n", device->mcu_app_fw_version);
	printf("DP: %s\n", device->dp_fw_version);
	printf("DSP: %s\n", device->dsp_fw_version);

	if (!send_payload_action(device, DEVICE4_MSG_R_BRIGHTNESS, 0, NULL)) {
		perror("Requesting initial brightness failed!\n");
		return device;
	}

	if (!recv_payload_msg(device, DEVICE4_MSG_R_BRIGHTNESS, 1, &device->brightness)) {
		perror("Receiving initial brightness failed!\n");
		return device;
	}

	return device;
}

static void device4_callback(device4_type* device,
							 uint64_t timestamp,
							 device4_event_type event,
							 uint8_t brightness,
							 const char* msg) {
	if (!device->callback) {
		return;
	}
	
	device->callback(timestamp, event, brightness, msg);
}

void device4_clear(device4_type* device) {
	device4_read(device, 10);
}

int device4_read(device4_type* device, int timeout) {
	if ((!device) || (!device->handle)) {
		return -1;
	}
	
	if (MAX_PACKET_SIZE != sizeof(device4_packet_type)) {
		perror("Not proper size!\n");
		return -2;
	}
	
	device4_packet_type packet;
	memset(&packet, 0, sizeof(device4_packet_type));
	
	int transferred = hid_read_timeout(
			device->handle,
			(uint8_t*) &packet,
			MAX_PACKET_SIZE,
			timeout
	);

	if (transferred == 0) {
		return 1;
	}
	
	if (MAX_PACKET_SIZE != transferred) {
		perror("Reading failed!\n");
		return -3;
	}

	if (packet.head != PACKET_HEAD) {
		perror("Wrong packet!\n");
		return -4;
	}

	const uint32_t timestamp = packet.timestamp;
	const size_t data_len = (size_t) &(packet.data) - (size_t) &(packet.length);

#ifndef NDEBUG
	printf("MSG: %d = %04x (%d)\n", packet.msgid, packet.msgid, packet.length);

	if (packet.length > 11) {
		for (int i = 0; i < packet.length - 11; i++) {
			printf("%02x ", packet.data[i]);
		}

		printf("\n");
	}
#endif
	
	switch (packet.msgid) {
		case DEVICE4_MSG_P_START_HEARTBEAT: {
			break;
		}
		case DEVICE4_MSG_P_BUTTON_PRESSED: {
			const uint8_t phys_button = packet.data[0];
			const uint8_t virt_button = packet.data[4];
			const uint8_t value = packet.data[8];
			
			switch (virt_button) {
				case DEVICE4_BUTTON_VIRT_DISPLAY_TOGGLE:
					device->active = value;
					
					if (device->active) {
						device4_callback(
								device,
								timestamp,
								DEVICE4_EVENT_SCREEN_ON,
								device->brightness,
								NULL
						);
					} else {
						device4_callback(
								device,
								timestamp,
								DEVICE4_EVENT_SCREEN_OFF,
								device->brightness,
								NULL
						);
					}
					break;
				case DEVICE4_BUTTON_VIRT_BRIGHTNESS_UP:
					device->brightness = value;
					
					device4_callback(
							device,
							timestamp,
							DEVICE4_EVENT_BRIGHTNESS_UP,
							device->brightness,
							NULL
					);
					break;
				case DEVICE4_BUTTON_VIRT_BRIGHTNESS_DOWN:
					device->brightness = value;
					
					device4_callback(
							device,
							timestamp,
							DEVICE4_EVENT_BRIGHTNESS_DOWN,
							device->brightness,
							NULL
					);
					break;
				default:
					break;
			}
			
			break;
		}
		case DEVICE4_MSG_P_ASYNC_TEXT_LOG: {
			const char* text = packet.text;
			const size_t text_len = strlen(text);
			
			device->active = true;
			
			if (data_len + text_len != packet.length) {
				perror("Not matching length!\n");
				return -5;
			}
			
			device4_callback(
					device,
					timestamp,
					DEVICE4_EVENT_MESSAGE,
					device->brightness,
					text
			);
			break;
		}
		case DEVICE4_MSG_P_END_HEARTBEAT: {
			break;
		}
		default:
			device4_callback(
					device,
					timestamp,
					DEVICE4_EVENT_UNKNOWN,
					device->brightness,
					NULL
			);
			break;
	}
	
	return 0;
}

void device4_close(device4_type* device) {
	if (!device) {
		return;
	}
	
	if (device->handle) {
		hid_close(device->handle);
		device->handle = NULL;
	}
	
	free(device);
	hid_exit();
}
