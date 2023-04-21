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

#include <Fusion/Fusion.h>
#include <libusb-1.0/libusb.h>

struct device3_calibration_t {
	FusionMatrix gyroscopeMisalignment;
	FusionVector gyroscopeSensitivity;
	FusionVector gyroscopeOffset;
	
	FusionMatrix accelerometerMisalignment;
	FusionVector accelerometerSensitivity;
	FusionVector accelerometerOffset;
	
	FusionMatrix softIronMatrix;
	FusionVector hardIronOffset;
};

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
	
	device->offset = malloc(sizeof(FusionOffset));
	device->ahrs = malloc(sizeof(FusionAhrs));
	
	FusionOffsetInitialise((FusionOffset*) device->offset, SAMPLE_RATE);
	FusionAhrsInitialise((FusionAhrs*) device->ahrs);
	
	device->calibration = malloc(sizeof(device3_calibration_type));
	device3_reset_calibration(device);
	
	const FusionAhrsSettings settings = {
			.convention = FusionConventionNwu,
			.gain = 0.5f,
			.accelerationRejection = 10.0f,
			.magneticRejection = 20.0f,
			.rejectionTimeout = 5 * SAMPLE_RATE, /* 5 seconds */
	};
	
	FusionAhrsSetSettings((FusionAhrs*) device->ahrs, &settings);
	return device;
}

void device3_reset_calibration(device3_type* device) {
	if (!device) {
		perror("No device!\n");
		return;
	}
	
	if (!device->calibration) {
		perror("Not allocated!\n");
		return;
	}
	
	device->calibration->gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
	device->calibration->gyroscopeSensitivity = FUSION_VECTOR_ONES;
	device->calibration->gyroscopeOffset = FUSION_VECTOR_ZERO;
	
	device->calibration->accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
	device->calibration->accelerometerSensitivity = FUSION_VECTOR_ONES;
	device->calibration->accelerometerOffset = FUSION_VECTOR_ZERO;
	
	device->calibration->accelerometerMisalignment.array[2][2] = -1.0f;
	
	device->calibration->softIronMatrix = FUSION_IDENTITY_MATRIX;
	device->calibration->hardIronOffset = FUSION_VECTOR_ZERO;
}

int device3_load_calibration(device3_type* device, const char* path) {
	if (!device) {
		perror("No device!\n");
		return -1;
	}
	
	if (!device->calibration) {
		perror("Not allocated!\n");
		return -2;
	}
	
	FILE* file = fopen(path, "rb");
	if (!file) {
		perror("No file opened!\n");
		return -3;
	}
	
	size_t count;
	count = fread(device->calibration, 1, sizeof(device3_calibration_type), file);
	
	if (sizeof(device3_calibration_type) != count) {
		perror("Not fully loaded!\n");
	}
	
	if (0 != fclose(file)) {
		perror("No file closed!\n");
		return -4;
	}
	
	return 0;
}

int device3_save_calibration(device3_type* device, const char* path) {
	if (!device) {
		perror("No device!\n");
		return -1;
	}
	
	if (!device->calibration) {
		perror("Not allocated!\n");
		return -2;
	}
	
	FILE* file = fopen(path, "wb");
	if (!file) {
		perror("No file opened!\n");
		return -3;
	}
	
	size_t count;
	count = fwrite(device->calibration, 1, sizeof(device3_calibration_type), file);
	
	if (sizeof(device3_calibration_type) != count) {
		perror("Not fully saved!\n");
	}
	
	if (0 != fclose(file)) {
		perror("No file closed!\n");
		return -4;
	}
	
	return 0;
}

static void device3_callback(device3_type* device,
							 uint64_t timestamp,
							 device3_event_type event) {
	if (!device->callback) {
		return;
	}
	
	device->callback(timestamp, event, device->ahrs);
}

static int32_t pack32bit_signed(const uint8_t* data) {
	uint32_t unsigned_value = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	return ((int32_t) unsigned_value);
}

static int32_t pack24bit_signed(const uint8_t* data) {
	uint32_t unsigned_value = (data[0]) | (data[1] << 8) | (data[2] << 16);
	if ((data[2] & 0x80) != 0) unsigned_value |= (0xFF << 24);
	return ((int32_t) unsigned_value);
}

static int16_t pack16bit_signed(const uint8_t* data) {
	uint16_t unsigned_value = (data[0]) | (data[1] << 8);
	return (int16_t) unsigned_value;
}

static int32_t pack32bit_signed_swap(const uint8_t* data) {
	uint32_t unsigned_value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
	return ((int32_t) unsigned_value);
}

static int16_t pack16bit_signed_swap(const uint8_t* data) {
	uint16_t unsigned_value = (data[0] << 8) | (data[1]);
	return (int16_t) unsigned_value;
}

static void readIMU_from_packet(const device3_packet_type* packet,
								FusionVector* gyroscope,
								FusionVector* accelerometer,
								FusionVector* magnetometer) {
	int32_t vel_m = pack16bit_signed(packet->angular_multiplier);
	int32_t vel_d = pack32bit_signed(packet->angular_divisor);
	
	int32_t vel_x = pack24bit_signed(packet->angular_velocity_x);
	int32_t vel_y = pack24bit_signed(packet->angular_velocity_y);
	int32_t vel_z = pack24bit_signed(packet->angular_velocity_z);
	
	gyroscope->axis.x = (float) vel_x * (float) vel_m / (float) vel_d;
	gyroscope->axis.y = (float) vel_y * (float) vel_m / (float) vel_d;
	gyroscope->axis.z = (float) vel_z * (float) vel_m / (float) vel_d;
	
	int32_t accel_m = pack16bit_signed(packet->acceleration_multiplier);
	int32_t accel_d = pack32bit_signed(packet->acceleration_divisor);
	
	int32_t accel_x = pack24bit_signed(packet->acceleration_x);
	int32_t accel_y = pack24bit_signed(packet->acceleration_y);
	int32_t accel_z = pack24bit_signed(packet->acceleration_z);
	
	accelerometer->axis.x = (float) accel_x * (float) accel_m / (float) accel_d;
	accelerometer->axis.y = (float) accel_y * (float) accel_m / (float) accel_d;
	accelerometer->axis.z = (float) accel_z * (float) accel_m / (float) accel_d;
	
	int32_t magnet_m = pack16bit_signed_swap(packet->magnetic_multiplier);
	int32_t magnet_d = pack32bit_signed_swap(packet->magnetic_divisor);
	
	int16_t magnet_x = pack16bit_signed_swap(packet->magnetic_x);
	int16_t magnet_y = pack16bit_signed_swap(packet->magnetic_y);
	int16_t magnet_z = pack16bit_signed_swap(packet->magnetic_z);
	
	magnetometer->axis.x = (float) magnet_x * (float) magnet_m / (float) magnet_d;
	magnetometer->axis.y = (float) magnet_y * (float) magnet_m / (float) magnet_d;
	magnetometer->axis.z = (float) magnet_z * (float) magnet_m / (float) magnet_d;
}

#define min(x, y) ((x) < (y)? (x) : (y))
#define max(x, y) ((x) > (y)? (x) : (y))

static void apply_calibration(const device3_type* device,
							  FusionVector* gyroscope,
							  FusionVector* accelerometer,
							  FusionVector* magnetometer) {
	FusionMatrix gyroscopeMisalignment;
	FusionVector gyroscopeSensitivity;
	FusionVector gyroscopeOffset;
	
	FusionMatrix accelerometerMisalignment;
	FusionVector accelerometerSensitivity;
	FusionVector accelerometerOffset;
	
	FusionMatrix softIronMatrix;
	FusionVector hardIronOffset;
	
	if (device->calibration) {
		gyroscopeMisalignment = device->calibration->gyroscopeMisalignment;
		gyroscopeSensitivity = device->calibration->gyroscopeSensitivity;
		gyroscopeOffset = device->calibration->gyroscopeOffset;
		
		accelerometerMisalignment = device->calibration->accelerometerMisalignment;
		accelerometerSensitivity = device->calibration->accelerometerSensitivity;
		accelerometerOffset = device->calibration->accelerometerOffset;
		
		softIronMatrix = device->calibration->softIronMatrix;
		hardIronOffset = device->calibration->hardIronOffset;
	} else {
		gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
		gyroscopeSensitivity = FUSION_VECTOR_ONES;
		gyroscopeOffset = FUSION_VECTOR_ZERO;
		
		accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
		accelerometerSensitivity = FUSION_VECTOR_ONES;
		accelerometerOffset = FUSION_VECTOR_ZERO;
		
		accelerometerMisalignment.array[2][2] = -1.0f;
		
		softIronMatrix = FUSION_IDENTITY_MATRIX;
		hardIronOffset = FUSION_VECTOR_ZERO;
	}
	
	*gyroscope = FusionCalibrationInertial(
			*gyroscope,
			gyroscopeMisalignment,
			gyroscopeSensitivity,
			gyroscopeOffset
	);
	
	*accelerometer = FusionCalibrationInertial(
			*accelerometer,
			accelerometerMisalignment,
			accelerometerSensitivity,
			accelerometerOffset
	);
	
	*magnetometer = FusionCalibrationMagnetic(
			*magnetometer,
			softIronMatrix,
			hardIronOffset
	);
}

int device3_calibrate(device3_type* device, uint32_t iterations, bool gyro, bool accel, bool magnet) {
	if (!device) {
		perror("No device!\n");
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
	int transferred, error;
	
	bool initialized = false;
	
	FusionVector cal_gyroscope;
	FusionVector cal_accelerometer;
	FusionVector cal_magnetometer [2];
	
	const float factor = iterations > 0? 1.0f / ((float) iterations) : 0.0f;
	
	while (iterations > 0) {
		memset(&packet, 0, sizeof(device3_packet_type));
		
		transferred = 0;
		error = libusb_bulk_transfer(
				device->handle,
				device->endpoint_address_in,
				(uint8_t*) &packet,
				device->max_packet_size_in,
				&transferred,
				0
		);
		
		if (error == LIBUSB_ERROR_TIMEOUT) {
			continue;
		}
		
		if ((0 != error) || (device->max_packet_size_in != transferred)) {
			perror("Not expected issue!\n");
			return -4;
		}
		
		if ((packet.signature[0] != 0x01) || (packet.signature[1] != 0x02)) {
			continue;
		}
		
		FusionVector gyroscope;
		FusionVector accelerometer;
		FusionVector magnetometer;
		
		readIMU_from_packet(&packet, &gyroscope, &accelerometer, &magnetometer);
		
		if (initialized) {
			cal_gyroscope = FusionVectorAdd(cal_gyroscope, gyroscope);
			cal_accelerometer = FusionVectorAdd(cal_accelerometer, accelerometer);
		} else {
			cal_gyroscope = gyroscope;
			cal_accelerometer = accelerometer;
		}
		
		apply_calibration(device, &gyroscope, &accelerometer, &magnetometer);
		
		if (initialized) {
			cal_magnetometer[0].axis.x = min(cal_magnetometer[0].axis.x, magnetometer.axis.x);
			cal_magnetometer[0].axis.y = min(cal_magnetometer[0].axis.y, magnetometer.axis.y);
			cal_magnetometer[0].axis.z = min(cal_magnetometer[0].axis.z, magnetometer.axis.z);
			cal_magnetometer[1].axis.x = max(cal_magnetometer[1].axis.x, magnetometer.axis.x);
			cal_magnetometer[1].axis.y = max(cal_magnetometer[1].axis.y, magnetometer.axis.y);
			cal_magnetometer[1].axis.z = max(cal_magnetometer[1].axis.z, magnetometer.axis.z);
		} else {
			cal_magnetometer[0] = magnetometer;
			cal_magnetometer[1] = magnetometer;
			initialized = true;
		}
		
		iterations--;
	}
	
	if (factor > 0.0f) {
		if (gyro) {
			device->calibration->gyroscopeOffset = FusionVectorAdd(
					device->calibration->gyroscopeOffset,
					FusionVectorMultiplyScalar(
							cal_gyroscope,
							factor
					)
			);
		}
		
		if (accel) {
			device->calibration->accelerometerOffset = FusionVectorAdd(
					device->calibration->accelerometerOffset,
					FusionVectorMultiplyScalar(
							cal_accelerometer,
							factor
					)
			);
		}
		
		if (magnet) {
			device->calibration->hardIronOffset = FusionVectorAdd(
					device->calibration->hardIronOffset,
					FusionVectorMultiplyScalar(
							FusionVectorAdd(cal_magnetometer[0], cal_magnetometer[1]),
							0.5f
					)
			);
		}
	}
	
	return 0;
}

int device3_read(device3_type* device, int timeout) {
	if (!device) {
		perror("No device!\n");
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
		device3_callback(device, timestamp, DEVICE3_EVENT_INIT);
		return 0;
	}
	
	if ((packet.signature[0] != 0x01) || (packet.signature[1] != 0x02)) {
		perror("Not matching signature!\n");
		return -5;
	}
	
	const uint64_t delta = timestamp - device->last_timestamp;
	const float deltaTime = (float) ((double) delta / 1e9);
	
	device->last_timestamp = timestamp;
	
	int16_t temperature = pack16bit_signed(packet.temperature);
	
	// According to the ICM-42688-P datasheet: (offset: 25 °C, sensitivity: 132.48 LSB/°C)
	device->temperature = ((float) temperature) / 132.48f + 25.0f;
	
	FusionVector gyroscope;
	FusionVector accelerometer;
	FusionVector magnetometer;
	
	readIMU_from_packet(&packet, &gyroscope, &accelerometer, &magnetometer);
	apply_calibration(device, &gyroscope, &accelerometer, &magnetometer);
	
	gyroscope = FusionOffsetUpdate((FusionOffset*) device->offset, gyroscope);
	
	FusionAhrsUpdate((FusionAhrs*) device->ahrs, gyroscope, accelerometer, magnetometer, deltaTime);
	
	device3_callback(device, timestamp, DEVICE3_EVENT_UPDATE);
	return 0;
}

device3_vec3_type device3_get_earth_acceleration(const device3_ahrs_type* ahrs) {
	FusionVector acceleration = FusionAhrsGetEarthAcceleration((const FusionAhrs*) ahrs);
	device3_vec3_type a;
	a.x = acceleration.axis.x;
	a.y = acceleration.axis.y;
	a.z = acceleration.axis.z;
	return a;
}

device3_vec3_type device3_get_linear_acceleration(const device3_ahrs_type* ahrs) {
	FusionVector acceleration = FusionAhrsGetLinearAcceleration((const FusionAhrs*) ahrs);
	device3_vec3_type a;
	a.x = acceleration.axis.x;
	a.y = acceleration.axis.y;
	a.z = acceleration.axis.z;
	return a;
}

device3_quat_type device3_get_orientation(const device3_ahrs_type* ahrs) {
	FusionQuaternion quaternion = FusionAhrsGetQuaternion((const FusionAhrs*) ahrs);
	device3_quat_type q;
	q.x = quaternion.element.x;
	q.y = quaternion.element.y;
	q.z = quaternion.element.z;
	q.w = quaternion.element.w;
	return q;
}

device3_vec3_type device3_get_euler(device3_quat_type quat) {
	FusionQuaternion quaternion;
	quaternion.element.x = quat.x;
	quaternion.element.y = quat.y;
	quaternion.element.z = quat.z;
	quaternion.element.w = quat.w;
	FusionEuler euler = FusionQuaternionToEuler(quaternion);
	device3_vec3_type e;
	e.x = euler.angle.pitch;
	e.y = euler.angle.roll;
	e.z = euler.angle.yaw;
	return e;
}

void device3_close(device3_type* device) {
	if (!device) {
		perror("No device!\n");
		return;
	}
	
	if (device->calibration) {
		free(device->calibration);
	}
	
	if (device->ahrs) {
		free(device->ahrs);
	}
	
	if (device->offset) {
		free(device->offset);
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
