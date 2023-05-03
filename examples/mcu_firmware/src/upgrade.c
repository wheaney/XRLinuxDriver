//
// Created by thejackimonster on 03.05.23.
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
#include <string.h>

int main(int argc, const char** argv) {
    if (argc <= 1) {
        printf("HOW TO USE IT:\n$ nrealAirUpgradeMCU <PATH>\n");
        return 0;
    }

    const char* path = argv[1];

    printf(
        "PLEASE BEWARE:\n\n"
        "This tool might damage your device when using wrong files replacing\n"
        "your firmware! Please make sure you have the right files selected and\n"
        "you know what you are doing!\n\n"
        "Also please DO NOT disconnect your device while updating the firmware!\n"
        "This might also damage your device! Just wait until it has finished.\n"
    );

    const char* filename = strrchr(path, '/');

    if (filename) {
        filename++;
    } else {
        filename = path;
    }

    const size_t filename_len = strlen(filename);

    if ((filename_len < 8) || 
        (strncmp(filename, "air_", 4) != 0) || 
        (strncmp(filename + filename_len - 4, ".bin", 4) != 0)) {
        printf("\nTHE SELECTED FILE DOES NOT LOOK RIGHT!\n%s\n", filename);
    }

    printf("\nYou selected the file at: %s\n", path);
    printf("\nDO YOU WANT TO CONTINUE? (y/n)\n");

    if (getchar() != 'y') {
        return 0;
    }

	device4_type* dev4 = device4_open(NULL);
	
	if (!dev4) {
		return 1;
	}
	
	device4_clear(dev4);
	device4_update_mcu_firmware(dev4, path);
	device4_close(dev4);
	return 0;
}
