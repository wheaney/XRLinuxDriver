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

#include "device3.h"
#include "device4.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <math.h>

void test3(uint64_t timestamp,
		   device3_event_type event,
		   const device3_ahrs_type* ahrs) {
	static device3_quat_type old;
	static float dmax = -1.0f;
	
	if (event != DEVICE3_EVENT_UPDATE) {
		return;
	}
	
	device3_quat_type q = device3_get_orientation(ahrs);
	
	const float dx = (old.x - q.x) * (old.x - q.x);
	const float dy = (old.y - q.y) * (old.y - q.y);
	const float dz = (old.z - q.z) * (old.z - q.z);
	const float dw = (old.w - q.w) * (old.w - q.w);
	
	const float d = sqrtf(dx*dx + dy*dy + dz*dz + dw*dw);
	
	if (dmax < 0.0f) {
		dmax = 0.0f;
	} else {
		dmax = (d > dmax? d : dmax);
	}
	
	device3_vec3_type e = device3_get_euler(q);
	
	if (d >= 0.00005f) {
		device3_vec3_type e0 = device3_get_euler(old);
		
		printf("Pitch: %f; Roll: %f; Yaw: %f\n", e0.x, e0.y, e0.z);
		printf("Pitch: %f; Roll: %f; Yaw: %f\n", e.x, e.y, e.z);
		printf("D = %f; ( %f )\n", d, dmax);
		
		printf("X: %f; Y: %f; Z: %f; W: %f;\n", old.x, old.y, old.z, old.w);
		printf("X: %f; Y: %f; Z: %f; W: %f;\n", q.x, q.y, q.z, q.w);
	} else {
		printf("Pitch: %.2f; Roll: %.2f; Yaw: %.2f\n", e.x, e.y, e.z);
	}
	
	old = q;
}

void test4(uint64_t timestamp,
		   device4_event_type event,
		   uint8_t brightness,
		   const char* msg) {
	switch (event) {
		case DEVICE4_EVENT_MESSAGE:
			printf("Message: `%s`\n", msg);
			break;
		case DEVICE4_EVENT_BRIGHTNESS_UP:
			printf("Increase Brightness: %u\n", brightness);
			break;
		case DEVICE4_EVENT_BRIGHTNESS_DOWN:
			printf("Decrease Brightness: %u\n", brightness);
			break;
		default:
			break;
	}
}

int main(int argc, const char** argv) {
	pid_t pid = fork();
	
	if (pid == -1) {
		perror("Could not fork!\n");
		return 1;
	}
	
	if (pid == 0) {
		device3_type* dev3 = device3_open(test3);
		device3_clear(dev3);
		
		while (dev3) {
			if (device3_read(dev3, 0) < 0) {
				break;
			}
		}
		
		device3_close(dev3);
		return 0;
	} else {
		device4_type* dev4 = device4_open(test4);
		device4_clear(dev4);
		
		while (dev4) {
			if (device4_read(dev4, 0) < 0) {
				break;
			}
		}
		
		device4_close(dev4);
		
		int status = 0;
		if (pid != waitpid(pid, &status, 0)) {
			return 1;
		}
		
		return status;
	}
}
