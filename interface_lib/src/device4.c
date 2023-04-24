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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

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
	
	if (0 != libusb_init((libusb_context**) &(device->context))) {
		perror("No context!\n");
		return device;
	}
	
	device->handle = libusb_open_device_with_vid_pid(
			device->context,
			device->vendor_id,
			device->product_id
	);
	
	if (!device->handle) {
		perror("No handle!\n");
		return device;
	}
	
	libusb_device* dev = libusb_get_device(device->handle);
	if (!dev) {
		perror("No dev!\n");
		return device;
	}
	
	struct libusb_device_descriptor desc;
	if (0 != libusb_get_device_descriptor(dev, &desc)) {
		perror("No desc!\n");
		return device;
	}
	
	struct libusb_config_descriptor* config;
	for (uint8_t i = 0; i < desc.bNumConfigurations; i++) {
		if (0 != libusb_get_config_descriptor(dev, i, &config)) {
			continue;
		}
		
		const struct libusb_interface* interface;
		for (uint8_t j = 0; j < config->bNumInterfaces; j++) {
			interface = &(config->interface[j]);
			
			const struct libusb_interface_descriptor* setting;
			for (int k = 0; k < interface->num_altsetting; k++) {
				setting = &(interface->altsetting[k]);
				
				if (LIBUSB_CLASS_HID != setting->bInterfaceClass) {
					continue;
				}
				
				if (4 != setting->bInterfaceNumber) {
					continue;
				}
				
				device->interface_number = setting->bInterfaceNumber;
				
				if (2 != setting->bNumEndpoints) {
					continue;
				}
				
				device->endpoint_address_in = setting->endpoint[0].bEndpointAddress;
				device->max_packet_size_in = setting->endpoint[0].wMaxPacketSize;
				
				device->endpoint_address_out = setting->endpoint[1].bEndpointAddress;
				device->max_packet_size_out = setting->endpoint[1].wMaxPacketSize;
			}
		}
	}
	
	if (4 != device->interface_number) {
		perror("No interface!\n");
		return device;
	}
	
	if (1 == libusb_kernel_driver_active(device->handle, device->interface_number)) {
		if (0 == libusb_detach_kernel_driver(device->handle, device->interface_number)) {
			device->detached = true;
		} else {
			perror("Not detached!\n");
			return device;
		}
	}
	
	if (0 == libusb_claim_interface(device->handle, device->interface_number)) {
		device->claimed = true;
	}
	
	if (device->claimed) {
		uint8_t initial_brightness_payload [16] = {
				0xfd, 0x1e, 0xb9, 0xf0,
				0x68, 0x11, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x03
		};
		
		int size = device->max_packet_size_out;
		if (sizeof(initial_brightness_payload) < size) {
			size = sizeof(initial_brightness_payload);
		}
		
		int transferred = 0;
		int error = libusb_bulk_transfer(
				device->handle,
				device->endpoint_address_out,
				initial_brightness_payload,
				size,
				&transferred,
				0
		);
		
		if ((0 != error) || (transferred != sizeof(initial_brightness_payload))) {
			perror("ERROR\n");
			return device;
		}
	}
	
	return device;
}

static void device4_callback(device4_type* device,
							 uint32_t timestamp,
							 device4_event_type event,
							 uint8_t brightness,
							 const char* msg) {
	if (!device->callback) {
		return;
	}
	
	device->callback(timestamp, event, brightness, msg);
}

void device4_clear(device4_type* device) {
	device4_read(device, 0);
}

int device4_read(device4_type* device, int timeout) {
	if (!device) {
		return -1;
	}
	
	if (!device->claimed) {
		perror("Not claimed!\n");
		return -2;
	}
	
	if (device->max_packet_size_in != sizeof(device4_packet_type)) {
		perror("Not proper size!\n");
		return -3;
	}
	
	device4_packet_type packet;
	memset(&packet, 0, sizeof(device4_packet_type));
	
	int transferred = 0;
	int error = libusb_bulk_transfer(
			device->handle,
			device->endpoint_address_in,
			(uint8_t*) &packet,
			device->max_packet_size_in,
			&transferred,
			timeout
	);
	
	if (error == LIBUSB_ERROR_TIMEOUT) {
		return 1;
	}
	
	if ((0 != error) || (device->max_packet_size_in != transferred)) {
		perror("Not expected issue!\n");
		return -4;
	}
	
	const uint32_t timestamp = packet.timestamp;
	const size_t data_len = (size_t) &(packet.data) - (size_t) &(packet.length);
	
	switch (packet.action) {
		case DEVICE4_ACTION_PASSIVE_POLL_START: {
			break;
		}
		case DEVICE4_ACTION_BRIGHTNESS_COMMAND: {
			const uint8_t brightness = packet.data[1];
			
			device->brightness = brightness;
			
			device4_callback(
					device,
					timestamp,
					DEVICE4_EVENT_BRIGHTNESS_SET,
					device->brightness,
					NULL
			);
			break;
		}
		case DEVICE4_ACTION_MANUAL_POLL_CLICK: {
			const uint8_t button = packet.data[0];
			const uint8_t brightness = packet.data[8];
			
			switch (button) {
				case DEVICE4_BUTTON_DISPLAY_TOGGLE:
					device->active = !device->active;
					
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
				case DEVICE4_BUTTON_BRIGHTNESS_UP:
					device->brightness = brightness;
					
					device4_callback(
							device,
							timestamp,
							DEVICE4_EVENT_BRIGHTNESS_UP,
							device->brightness,
							NULL
					);
					break;
				case DEVICE4_BUTTON_BRIGHTNESS_DOWN:
					device->brightness = brightness;
					
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
		case DEVICE4_ACTION_ACTIVE_POLL: {
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
		case DEVICE4_ACTION_PASSIVE_POLL_END: {
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
	
	if ((device->claimed) &&
		(0 == libusb_release_interface(device->handle, device->interface_number))) {
		device->claimed = false;
	}
	
	if ((device->detached) &&
		(0 == libusb_attach_kernel_driver(device->handle, device->interface_number))) {
		device->detached = false;
	}
	
	if (device->handle) {
		libusb_close(device->handle);
		device->handle = NULL;
	}
	
	if (device->context) {
		libusb_exit(device->context);
		device->context = NULL;
	}
	
	free(device);
}
