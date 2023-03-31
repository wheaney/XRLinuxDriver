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

#include "device3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

device3_type* device3_open(device3_event_callback callback) {
	device3_type* device = (device3_type*) malloc(sizeof(device3_type));
	
	if (!device) {
		perror("Not allocated!\n");
		return NULL;
	}
	
	memset(device, 0, sizeof(device3_type));
	device->vendor_id 	= 0x3318;
	device->product_id 	= 0x0424;
	device->callback 	= callback;
	
	if (0 != libusb_init(&(device->context))) {
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
				
				if (3 != setting->bInterfaceNumber) {
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
	
	if (3 != device->interface_number) {
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
		uint8_t initial_imu_payload [9] = {
				0xaa, 0xc5, 0xd1, 0x21,
				0x42, 0x04, 0x00, 0x19,
				0x01
		};
		
		int size = device->max_packet_size_out;
		if (sizeof(initial_imu_payload) < size) {
			size = sizeof(initial_imu_payload);
		}
		
		int transferred = 0;
		int error = libusb_bulk_transfer(
				device->handle,
				device->endpoint_address_out,
				initial_imu_payload,
				size,
				&transferred,
				0
		);
		
		if ((0 != error) || (transferred != sizeof(initial_imu_payload))) {
			perror("ERROR\n");
			return device;
		}
	}
	
	const uint32_t SAMPLE_RATE = 1000;
	
	FusionOffsetInitialise(&(device->offset), SAMPLE_RATE);
	FusionAhrsInitialise(&(device->ahrs));
	
	const FusionAhrsSettings settings = {
			.gain = 0.5f,
			.accelerationRejection = 10.0f,
			.magneticRejection = 20.0f,
			.rejectionTimeout = 5 * SAMPLE_RATE, /* 5 seconds */
	};
	
	FusionAhrsSetSettings(&(device->ahrs), &settings);
	return device;
}

static void device3_callback(device3_type* device,
							 uint64_t timestamp,
							 device3_event_type event,
							 const FusionAhrs* ahrs) {
	if (!device->callback) {
		return;
	}
	
	device->callback(timestamp, event, ahrs);
}

static int32_t pack24bit_signed(const uint8_t* data) {
	uint32_t unsigned_value = (data[0]) | (data[1] << 8) | (data[2] << 16);
	if ((data[2] & 0x80) != 0) unsigned_value |= (0xFF << 24);
	return (int32_t) unsigned_value;
}

// based on 24bit signed int w/ FSR = +/-2000 dps, datasheet option
#define GYRO_SCALAR (1.0f / 8388608.0f * 2000.0f)

// based on 24bit signed int w/ FSR = +/-16 g, datasheet option
#define ACCEL_SCALAR (1.0f / 8388608.0f * 16.0f)

// based on 16bit signed int w/ FSR = +/-16 gauss, datasheet option
#define MAGNO_SCALAR (1.0f / 8388608.0f * 16.0f)

int device3_read(device3_type* device, int timeout) {
	if (!device) {
		return -1;
	}
	
	if (!device->claimed) {
		perror("Not claimed!\n");
		return -2;
	}
	
	if (device->max_packet_size_in != sizeof(device3_packet_type)) {
		perror("Not proper size!\n");
		return -3;
	}
	
	device3_packet_type packet;
	memset(&packet, 0, sizeof(device3_packet_type));
	
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
	
	const uint64_t timestamp = packet.timestamp;
	
	if ((packet.signature[0] == 0xaa) && (packet.signature[1] == 0x53)) {
		device3_callback(device, timestamp, DEVICE3_EVENT_INIT, &(device->ahrs));
		return 0;
	}
	
	if ((packet.signature[0] != 0x01) || (packet.signature[1] != 0x02)) {
		perror("Not matching signature!\n");
		return -5;
	}
	
	const uint64_t delta = timestamp - device->last_timestamp;
	const float deltaTime = (float) ((double) delta / 1e9);
	
	device->last_timestamp = timestamp;
	
	int32_t vel_x = pack24bit_signed(packet.angular_velocity_x);
	int32_t vel_y = pack24bit_signed(packet.angular_velocity_y);
	int32_t vel_z = pack24bit_signed(packet.angular_velocity_z);
	
	FusionVector gyroscope;
	gyroscope.axis.x = vel_x * GYRO_SCALAR;
	gyroscope.axis.y = vel_y * GYRO_SCALAR;
	gyroscope.axis.z = vel_z * GYRO_SCALAR;
	
	int32_t accel_x = pack24bit_signed(packet.acceleration_x);
	int32_t accel_y = pack24bit_signed(packet.acceleration_y);
	int32_t accel_z = pack24bit_signed(packet.acceleration_z);
	
	FusionVector accelerometer;
	accelerometer.axis.x = accel_x * ACCEL_SCALAR;
	accelerometer.axis.y = accel_y * ACCEL_SCALAR;
	accelerometer.axis.z = accel_z * ACCEL_SCALAR;
	
	int16_t magnet_x = packet.magnetic_x;
	int16_t magnet_y = packet.magnetic_y;
	int16_t magnet_z = packet.magnetic_z;
	
	FusionVector magnetometer;
	magnetometer.axis.x = magnet_x * MAGNO_SCALAR;
	magnetometer.axis.y = magnet_y * MAGNO_SCALAR;
	magnetometer.axis.z = magnet_z * MAGNO_SCALAR;
	
	const FusionMatrix gyroscopeMisalignment = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const FusionVector gyroscopeSensitivity = { 1.0f, 1.0f, 1.0f };
	const FusionVector gyroscopeOffset = { 0.0f, 0.0f, 0.0f };
	const FusionMatrix accelerometerMisalignment = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const FusionVector accelerometerSensitivity = { 1.0f, 1.0f, 1.0f };
	const FusionVector accelerometerOffset = { 0.0f, 0.0f, 0.0f };
	const FusionMatrix magnetometerMisalignment = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const FusionVector magnetometerSensitivity = { 1.0f, 1.0f, 1.0f };
	const FusionVector magnetometerOffset = { 0.0f, 0.0f, 0.0f };
	const FusionMatrix softIronMatrix = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const FusionVector hardIronOffset = { 0.0f, 0.0f, 0.0f };
	
	gyroscope = FusionCalibrationInertial(
			gyroscope,
			gyroscopeMisalignment,
			gyroscopeSensitivity,
			gyroscopeOffset
	);
	
	accelerometer = FusionCalibrationInertial(
			accelerometer,
			accelerometerMisalignment,
			accelerometerSensitivity,
			accelerometerOffset
	);
	
	magnetometer = FusionCalibrationInertial(
			magnetometer,
			magnetometerMisalignment,
			magnetometerSensitivity,
			magnetometerOffset
	);
	
	gyroscope = FusionOffsetUpdate(&(device->offset), gyroscope);
	
	FusionAhrsUpdate(&(device->ahrs), gyroscope, accelerometer, magnetometer, deltaTime);
	
	device3_callback(device, timestamp, DEVICE3_EVENT_UPDATE, &(device->ahrs));
	return 0;
}

void device3_close(device3_type* device) {
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
