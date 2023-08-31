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
#include <stdbool.h>
#include <string.h>

#include <Fusion/Fusion.h>
#include <json-c/json.h>

#include <../hidapi/hidapi/hidapi.h>

#include "crc32.h"

struct device3_calibration_t {
	FusionMatrix gyroscopeMisalignment;
	FusionVector gyroscopeSensitivity;
	FusionVector gyroscopeOffset;
	
	FusionMatrix accelerometerMisalignment;
	FusionVector accelerometerSensitivity;
	FusionVector accelerometerOffset;
	
	FusionMatrix magnetometerMisalignment;
	FusionVector magnetometerSensitivity;
	FusionVector magnetometerOffset;
	
	FusionMatrix softIronMatrix;
	FusionVector hardIronOffset;
	
	FusionQuaternion noises;
};

#define MAX_PACKET_SIZE 64

static bool send_payload(device3_type* device, uint8_t size, const uint8_t* payload) {
	int payload_size = size;
	if (payload_size > MAX_PACKET_SIZE) {
		payload_size = MAX_PACKET_SIZE;
	}
	
	int transferred = hid_write(device->handle, payload, payload_size);
	
	if (transferred != payload_size) {
		fprintf(stderr, "ERROR: sending payload failed\n");
		return false;
	}
	
	return (transferred == size);
}

static bool recv_payload(device3_type* device, uint8_t size, uint8_t* payload) {
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
		fprintf(stderr, "ERROR: receiving payload failed\n");
		return false;
	}
	
	return (transferred == size);
}

struct __attribute__((__packed__)) device3_payload_packet_t {
	uint8_t head;
	uint32_t checksum;
	uint16_t length;
	uint8_t msgid;
	uint8_t data [56];
};

typedef struct device3_payload_packet_t device3_payload_packet_type;

static bool send_payload_msg(device3_type* device, uint8_t msgid, uint8_t len, const uint8_t* data) {
	static device3_payload_packet_type packet;
	
	const uint16_t packet_len = 3 + len;
	const uint16_t payload_len = 5 + packet_len;
	
	packet.head = 0xAA;
	packet.length = packet_len;
	packet.msgid = msgid;
	
	memcpy(packet.data, data, len);
	packet.checksum = crc32_checksum((const uint8_t*) (&packet.length), packet.length);
	
	return send_payload(device, payload_len, (uint8_t*) (&packet));
}

static bool send_payload_msg_signal(device3_type* device, uint8_t msgid, uint8_t signal) {
	return send_payload_msg(device, msgid, 1, &signal);
}

static bool recv_payload_msg(device3_type* device, uint8_t msgid, uint8_t len, uint8_t* data) {
	static device3_payload_packet_type packet;
	
	packet.head = 0;
	packet.length = 0;
	packet.msgid = 0;
	
	const uint16_t packet_len = 3 + len;
	const uint16_t payload_len = 5 + packet_len;
	
	if (!recv_payload(device, payload_len, (uint8_t*) (&packet))) {
		return false;
	}
	
	if (packet.msgid != msgid) {
		return false;
	}
	
	memcpy(data, packet.data, len);
	return true;
}

static FusionVector json_object_get_vector(struct json_object* obj) {
	if ((!json_object_is_type(obj, json_type_array)) ||
		(json_object_array_length(obj) != 3)) {
		return FUSION_VECTOR_ZERO;
	}
	
	FusionVector vector;
	vector.axis.x = (float) json_object_get_double(json_object_array_get_idx(obj, 0));
	vector.axis.y = (float) json_object_get_double(json_object_array_get_idx(obj, 1));
	vector.axis.z = (float) json_object_get_double(json_object_array_get_idx(obj, 2));
	return vector;
}

static FusionQuaternion json_object_get_quaternion(struct json_object* obj) {
	if ((!json_object_is_type(obj, json_type_array)) ||
		(json_object_array_length(obj) != 4)) {
		return FUSION_IDENTITY_QUATERNION;
	}
	
	FusionQuaternion quaternion;
	quaternion.element.x = (float) json_object_get_double(json_object_array_get_idx(obj, 0));
	quaternion.element.y = (float) json_object_get_double(json_object_array_get_idx(obj, 1));
	quaternion.element.z = (float) json_object_get_double(json_object_array_get_idx(obj, 2));
	quaternion.element.w = (float) json_object_get_double(json_object_array_get_idx(obj, 3));
	return quaternion;
}

device3_type* device3_open(device3_event_callback callback) {
	device3_type* device = (device3_type*) malloc(sizeof(device3_type));
	
	if (!device) {
		return NULL;
	}
	
	memset(device, 0, sizeof(device3_type));
	device->vendor_id 	= 0x3318;
	device->product_id 	= 0x0424;
	device->callback 	= callback;
	
	if (0 != hid_init()) {
		return device;
	}

	struct hid_device_info* info = hid_enumerate(
		device->vendor_id, 
		device->product_id
	);

	struct hid_device_info* it = info;
	while (it) {
		if (it->interface_number == 3) {
			device->handle = hid_open_path(it->path);
			break;
		}

		it = it->next;
	}

	hid_free_enumeration(info);
	
	if (!device->handle) {
		return device;
	}

	device3_clear(device);
	
	if (!send_payload_msg(device, DEVICE3_MSG_GET_STATIC_ID, 0, NULL)) {
		return device;
	}
	
	uint32_t static_id = 0;
	if (recv_payload_msg(device, DEVICE3_MSG_GET_STATIC_ID, 4, (uint8_t*) &static_id)) {
		device->static_id = static_id;
	} else {
		device->static_id = 0x20220101;
	}
	
	device->calibration = malloc(sizeof(device3_calibration_type));
	device3_reset_calibration(device);
	
	if (!send_payload_msg(device, DEVICE3_MSG_GET_CAL_DATA_LENGTH, 0, NULL)) {
		return device;
	}
	
	uint32_t calibration_len = 0;
	if (recv_payload_msg(device, DEVICE3_MSG_GET_CAL_DATA_LENGTH, 4, (uint8_t*) &calibration_len)) {
		char* calibration_data = malloc(calibration_len);
		
		uint32_t position = 0;
		while (position < calibration_len) {
			const uint32_t remaining = (calibration_len - position);
			
			if (!send_payload_msg(device, DEVICE3_MSG_CAL_DATA_GET_NEXT_SEGMENT, 0, NULL)) {
				break;
			}
			
			const uint8_t next = (remaining > 56? 56 : remaining);
			
			if (!recv_payload_msg(device, DEVICE3_MSG_CAL_DATA_GET_NEXT_SEGMENT, next, (uint8_t*) calibration_data + position)) {
				break;
			}
			
			position += next;
		}
		
		struct json_tokener* tokener = json_tokener_new();
		struct json_object* root = json_tokener_parse_ex(tokener, calibration_data, calibration_len);
		struct json_object* imu = json_object_object_get(root, "IMU");
		struct json_object* dev1 = json_object_object_get(imu, "device_1");
		
		FusionVector accel_bias = json_object_get_vector(json_object_object_get(dev1, "accel_bias"));
		FusionVector gyro_bias = json_object_get_vector(json_object_object_get(dev1, "gyro_bias"));
		FusionQuaternion imu_noises = json_object_get_quaternion(json_object_object_get(dev1, "imu_noises"));
		FusionVector mag_bias = json_object_get_vector(json_object_object_get(dev1, "mag_bias"));
		FusionVector scale_accel = json_object_get_vector(json_object_object_get(dev1, "scale_accel"));
		FusionVector scale_gyro = json_object_get_vector(json_object_object_get(dev1, "scale_gyro"));
		FusionVector scale_mag = json_object_get_vector(json_object_object_get(dev1, "scale_mag"));
		
		device->calibration->gyroscopeSensitivity = scale_gyro;
		device->calibration->gyroscopeOffset = gyro_bias;
		
		device->calibration->accelerometerSensitivity = scale_accel;
		device->calibration->accelerometerOffset = accel_bias;
		
		device->calibration->magnetometerSensitivity = scale_mag;
		device->calibration->magnetometerOffset = mag_bias;
		
		device->calibration->noises = imu_noises;
		
		json_tokener_free(tokener);
		free(calibration_data);
	}
	
	if (!send_payload_msg_signal(device, DEVICE3_MSG_START_IMU_DATA, 0x1)) {
		return device;
	}
	
	const uint32_t SAMPLE_RATE = 1000;
	
	device->offset = malloc(sizeof(FusionOffset));
	device->ahrs = malloc(sizeof(FusionAhrs));
	
	FusionOffsetInitialise((FusionOffset*) device->offset, SAMPLE_RATE);
	FusionAhrsInitialise((FusionAhrs*) device->ahrs);
	
	const FusionAhrsSettings settings = {
			.convention = FusionConventionNwu,
			.gain = 0.5f,
			.accelerationRejection = 10.0f,
			.magneticRejection = 20.0f,
			.recoveryTriggerPeriod = 5 * SAMPLE_RATE, /* 5 seconds */
	};
	
	FusionAhrsSetSettings((FusionAhrs*) device->ahrs, &settings);

	device->ready = true;

	return device;
}

void device3_reset_calibration(device3_type* device) {
	if (!device) {
		fprintf(stderr, "No device!\n");
		return;
	}
	
	if (!device->calibration) {
		fprintf(stderr, "Not allocated!\n");
		return;
	}
	
	device->calibration->gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
	device->calibration->gyroscopeSensitivity = FUSION_VECTOR_ONES;
	device->calibration->gyroscopeOffset = FUSION_VECTOR_ZERO;
	
	device->calibration->accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
	device->calibration->accelerometerSensitivity = FUSION_VECTOR_ONES;
	device->calibration->accelerometerOffset = FUSION_VECTOR_ZERO;
	
	device->calibration->magnetometerMisalignment = FUSION_IDENTITY_MATRIX;
	device->calibration->magnetometerSensitivity = FUSION_VECTOR_ONES;
	device->calibration->magnetometerOffset = FUSION_VECTOR_ZERO;
	
	device->calibration->softIronMatrix = FUSION_IDENTITY_MATRIX;
	device->calibration->hardIronOffset = FUSION_VECTOR_ZERO;
	
	device->calibration->noises = FUSION_IDENTITY_QUATERNION;
	device->calibration->noises.element.w = 0.0f;
}

int device3_load_calibration(device3_type* device, const char* path) {
	if (!device) {
		fprintf(stderr, "No device!\n");
		return -1;
	}
	
	if (!device->calibration) {
		fprintf(stderr, "Not allocated!\n");
		return -2;
	}
	
	FILE* file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "No file opened!\n");
		return -3;
	}
	
	size_t count;
	count = fread(device->calibration, 1, sizeof(device3_calibration_type), file);
	
	if (sizeof(device3_calibration_type) != count) {
		fprintf(stderr, "Not fully loaded!\n");
	}
	
	if (0 != fclose(file)) {
		fprintf(stderr, "No file closed!\n");
		return -4;
	}
	
	return 0;
}

int device3_save_calibration(device3_type* device, const char* path) {
	if (!device) {
		fprintf(stderr, "No device!\n");
		return -1;
	}
	
	if (!device->calibration) {
		fprintf(stderr, "Not allocated!\n");
		return -2;
	}
	
	FILE* file = fopen(path, "wb");
	if (!file) {
		fprintf(stderr, "No file opened!\n");
		return -3;
	}
	
	size_t count;
	count = fwrite(device->calibration, 1, sizeof(device3_calibration_type), file);
	
	if (sizeof(device3_calibration_type) != count) {
		fprintf(stderr, "Not fully saved!\n");
	}
	
	if (0 != fclose(file)) {
		fprintf(stderr, "No file closed!\n");
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
	uint16_t unsigned_value = (data[1] << 8) | (data[0]);
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

static int16_t pack16bit_signed_bizarre(const uint8_t* data) {
	uint16_t unsigned_value = (data[0]) | ((data[1] ^ 0x80) << 8);
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
	
	int16_t magnet_x = pack16bit_signed_bizarre(packet->magnetic_x);
	int16_t magnet_y = pack16bit_signed_bizarre(packet->magnetic_y);
	int16_t magnet_z = pack16bit_signed_bizarre(packet->magnetic_z);
	
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
	
	FusionMatrix magnetometerMisalignment;
	FusionVector magnetometerSensitivity;
	FusionVector magnetometerOffset;
	
	FusionMatrix softIronMatrix;
	FusionVector hardIronOffset;
	
	if (device->calibration) {
		gyroscopeMisalignment = device->calibration->gyroscopeMisalignment;
		gyroscopeSensitivity = device->calibration->gyroscopeSensitivity;
		gyroscopeOffset = device->calibration->gyroscopeOffset;
		
		accelerometerMisalignment = device->calibration->accelerometerMisalignment;
		accelerometerSensitivity = device->calibration->accelerometerSensitivity;
		accelerometerOffset = device->calibration->accelerometerOffset;
		
		magnetometerMisalignment = device->calibration->magnetometerMisalignment;
		magnetometerSensitivity = device->calibration->magnetometerSensitivity;
		magnetometerOffset = device->calibration->magnetometerOffset;
		
		softIronMatrix = device->calibration->softIronMatrix;
		hardIronOffset = device->calibration->hardIronOffset;
	} else {
		gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
		gyroscopeSensitivity = FUSION_VECTOR_ONES;
		gyroscopeOffset = FUSION_VECTOR_ZERO;
		
		accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
		accelerometerSensitivity = FUSION_VECTOR_ONES;
		accelerometerOffset = FUSION_VECTOR_ZERO;
		
		magnetometerMisalignment = FUSION_IDENTITY_MATRIX;
		magnetometerSensitivity = FUSION_VECTOR_ONES;
		magnetometerOffset = FUSION_VECTOR_ZERO;
		
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
	
	*magnetometer = FusionCalibrationInertial(
			*magnetometer,
			magnetometerMisalignment,
			magnetometerSensitivity,
			magnetometerOffset
	);
	
	static FusionVector max = { 0.077f, -0.129f, -0.446f }, min = { 0.603f, 0.447f, 0.085f };
	for (int i = 0; i < 3; i++) {
		if (magnetometer->array[i] > max.array[i]) max.array[i] = magnetometer->array[i];
		if (magnetometer->array[i] < min.array[i]) min.array[i] = magnetometer->array[i];
	}
	
	const float mx = (max.axis.x - min.axis.x) / 2.0f;
	const float my = (max.axis.y - min.axis.y) / 2.0f;
	const float mz = (max.axis.z - min.axis.z) / 2.0f;
	
	const float cx = (min.axis.x + max.axis.x) / 2.0f;
	const float cy = (min.axis.y + max.axis.y) / 2.0f;
	const float cz = (min.axis.z + max.axis.z) / 2.0f;
	
	softIronMatrix.element.xx = 1.0f / mx;
	softIronMatrix.element.yy = 1.0f / my;
	softIronMatrix.element.zz = 1.0f / mz;
	
	hardIronOffset.axis.x = cx / mx;
	hardIronOffset.axis.y = cy / my;
	hardIronOffset.axis.z = cz / mz;
	
	if (device->calibration) {
		device->calibration->softIronMatrix = softIronMatrix;
		device->calibration->hardIronOffset = hardIronOffset;
	}
	
	*magnetometer = FusionCalibrationMagnetic(
			*magnetometer,
			softIronMatrix,
			hardIronOffset
	);

	const FusionAxesAlignment alignment = FusionAxesAlignmentNXNYPZ;
	
	*gyroscope = FusionAxesSwap(*gyroscope, alignment);
	*accelerometer = FusionAxesSwap(*accelerometer, alignment);
	*magnetometer = FusionAxesSwap(*magnetometer, alignment);
}

void device3_clear(device3_type* device) {
	device3_read(device, 10, true);
}

int device3_calibrate(device3_type* device, uint32_t iterations, bool gyro, bool accel, bool magnet) {
	if (!device) {
		fprintf(stderr, "No device!\n");
		return -1;
	}
	
	if (MAX_PACKET_SIZE != sizeof(device3_packet_type)) {
		fprintf(stderr, "Not proper size!\n");
		return -2;
	}
	
	device3_packet_type packet;
	int transferred;
	
	bool initialized = false;
	
	FusionVector cal_gyroscope;
	FusionVector cal_accelerometer;
	FusionVector cal_magnetometer [2];
	
	const float factor = iterations > 0? 1.0f / ((float) iterations) : 0.0f;
	
	while (iterations > 0) {
		memset(&packet, 0, sizeof(device3_packet_type));
		
		transferred = hid_read(
			device->handle, 
			(uint8_t*) &packet, 
			MAX_PACKET_SIZE
		);

		if (transferred == -1) {
			fprintf(stderr, "HID read error: device may be unplugged\n");
			return -5;
		}

		if (transferred == 0) {
			continue;
		}

		if (MAX_PACKET_SIZE != transferred) {
			fprintf(stderr, "HID read error: unexpected packet size\n");
			return -3;
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

int device3_read(device3_type* device, int timeout, bool silent) {
	if (!device || !device->ready) {
		if (!silent) {
			fprintf(stderr, "No device!\n");
		}
		return -1;
	}
	
	if (MAX_PACKET_SIZE != sizeof(device3_packet_type)) {
		if (!silent) {
			fprintf(stderr, "Not proper size!\n");
		}
		return -2;
	}
	
	device3_packet_type packet;
	memset(&packet, 0, sizeof(device3_packet_type));
	
	int transferred = hid_read_timeout(
		device->handle, 
		(uint8_t*) &packet, 
		MAX_PACKET_SIZE,
		timeout
	);

	if (transferred == -1) {
		if (!silent) {
			fprintf(stderr, "HID read error: device may be unplugged\n");
		}
		return -5;
	}
	
	if (transferred == 0) {
		return 1;
	}
	
	if (MAX_PACKET_SIZE != transferred) {
		if (!silent) {
			fprintf(stderr, "HID read error: unexpected packet size\n");
		}
		return -3;
	}
	
	const uint64_t timestamp = packet.timestamp;
	
	if ((packet.signature[0] == 0xaa) && (packet.signature[1] == 0x53)) {
		device3_callback(device, timestamp, DEVICE3_EVENT_INIT);
		return 0;
	}
	
	if ((packet.signature[0] != 0x01) || (packet.signature[1] != 0x02)) {
		if (!silent) {
			fprintf(stderr, "Mismatched signature! Try unplugging then replugging your device.\n");
		}
		return -4;
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
	
	if (device->offset) {
		gyroscope = FusionOffsetUpdate((FusionOffset*) device->offset, gyroscope);
	}
	
	//printf("G: %.2f %.2f %.2f\n", gyroscope.axis.x, gyroscope.axis.y, gyroscope.axis.z);
	//printf("A: %.2f %.2f %.2f\n", accelerometer.axis.x, accelerometer.axis.y, accelerometer.axis.z);
	//printf("M: %.2f %.2f %.2f\n", magnetometer.axis.x, magnetometer.axis.y, magnetometer.axis.z);
	
	if (device->ahrs) {
		FusionAhrsUpdateNoMagnetometer((FusionAhrs*) device->ahrs, gyroscope, accelerometer, deltaTime);

		// TODO: fix detection of this case; quat.x as a nan value is only a side-effect of some issue with ahrs or
		//       the gyro/accel/magnet readings
		if (isnan(device3_get_orientation(device->ahrs).x)) {
			if (!silent) {
				fprintf(stderr, "Invalid device reading\n");
			}
			return -5;
		}
	}
	
	device3_callback(device, timestamp, DEVICE3_EVENT_UPDATE);
	return 0;
}

device3_vec3_type device3_get_earth_acceleration(const device3_ahrs_type* ahrs) {
	FusionVector acceleration = ahrs? FusionAhrsGetEarthAcceleration((const FusionAhrs*) ahrs) : FUSION_VECTOR_ZERO;
	device3_vec3_type a;
	a.x = acceleration.axis.x;
	a.y = acceleration.axis.y;
	a.z = acceleration.axis.z;
	return a;
}

device3_vec3_type device3_get_linear_acceleration(const device3_ahrs_type* ahrs) {
	FusionVector acceleration = ahrs? FusionAhrsGetLinearAcceleration((const FusionAhrs*) ahrs) : FUSION_VECTOR_ZERO;
	device3_vec3_type a;
	a.x = acceleration.axis.x;
	a.y = acceleration.axis.y;
	a.z = acceleration.axis.z;
	return a;
}

device3_quat_type device3_get_orientation(const device3_ahrs_type* ahrs) {
	FusionQuaternion quaternion = ahrs? FusionAhrsGetQuaternion((const FusionAhrs*) ahrs) : FUSION_IDENTITY_QUATERNION;
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

	if (device->handle) {
		hid_close(device->handle);
		device->handle = NULL;
	}
	
	free(device);
	hid_exit();
}
