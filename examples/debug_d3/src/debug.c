//
// Created by thejackimonster on 05.04.23.
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

void test3(uint64_t timestamp,
		   device3_event_type event,
		   const device3_ahrs_type* ahrs) {
	device3_quat_type orientation;
	device3_vec3_type euler;
	
	switch (event) {
		case DEVICE3_EVENT_INIT:
			printf("Initialized\n");
			break;
		case DEVICE3_EVENT_UPDATE:
			orientation = device3_get_orientation(ahrs);
			euler = device3_get_euler(orientation);
			printf("Pitch: %.2f; Roll: %.2f; Yaw: %.2f\n", euler.x, euler.y, euler.z);
			break;
		default:
			break;
	}
}

int main(int argc, const char** argv) {
	device3_type* dev3 = device3_open(test3);
	
	if (!dev3) {
		return 1;
	}
	
	device3_clear(dev3);
	
	while (dev3) {
		if (device3_read(dev3, -1) < 0) {
			break;
		}
	}
	
	device3_close(dev3);
	return 0;
}
