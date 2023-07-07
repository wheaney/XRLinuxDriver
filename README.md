# Custom user-space driver for the nreal Air to use it on Linux

## Information before use

The code is provided as is and it's free to use. However the contributors can neither guarantee that 
it will work or that it won't damage your device since all of it is based on reverse-engineering 
instead of public documentation. The contributors are not responsible for proper or even official 
support. So use it at your own risk!

## Inspiration and motivation

Because I've written a user-space driver before for a [graphics tablet](https://gitlab.com/TheJackiMonster/HuionGT191LinuxDriver), 
I thought I might look into it. To my surprise video output, audio output (stereo) and audio input (microphone) already 
worked using drivers from the [Linux kernel](https://linux-hardware.org/?id=usb:3318-0424). So the only piece missing 
for full use was the IMU sensor data in theory.

A big help for implementing this piece of software was the source code and feedback from a custom 
driver for Windows [here](https://github.com/MSmithDev/AirAPI_Windows/). Without that I would have 
needed to find out payloads my own. So big thanks to such projects being open-source!

Another huge help was the reverse engineering [here](https://github.com/edwatt/real_utilities/) to 
send different payloads to the device and even read calibration data from the local storage. Without 
that calibrating would be quite a hassle for end-users as well as efforts from developers tweaking 
the values manually. So big thanks as well!

## Features

The driver will read, parse and interpret sensor data from two HID interfaces to feed custom event 
callbacks with data which can be used in user-space applications (for example whether buttons have 
been pressed, the current brightness level and the orientation quaternion/matrix/euler angles).

It's still a work-in-progress project since the goal would be to wire such events up into a 
compositor to render whole screens virtually depending on your 6-DoF orientation (without position).

Also keep in mind that this software will only run on Linux including devices like the Steam Deck, 
Pinephone or other mobile Linux devices.

## Dependencies

You can build the binary using `cmake` and there are a few dependencies for now:
 - [hidapi](https://github.com/libusb/hidapi)
 - [json-c](https://github.com/json-c/json-c/)
 - [Fusion](https://github.com/xioTechnologies/Fusion)
 - [libevdev](https://gitlab.freedesktop.org/libevdev/libevdev)

Fusion and hidapi source are included as Git submodules that you'll need to check out using the `git submodules` command. json-c may already be installed with your distro, and you'll want to install libevdev (e.g. 'sudo apt install libevdev-dev`)

## Build

The build process should be straight forward:

```
mkdir build
cd build
cmake ..
make
```

## Usage

To use this driver:
1. If on Steam Deck, switch to Desktop Mode
2. Download the latest driver from the releases page
3. From a terminal, check if you have the `uinput` module already installed `lsmod | grep uinput`; if not, install it
4. Add the following rule to a udev rules file: `SUBSYSTEMS=="usb", ATTRS{idVendor}=="3318", ATTRS{idProduct}=="0424", TAG+="uaccess"`
   * Modify or create a new file in `/etc/udev/rules.d/`
   * Reload the udev rules using something like `sudo udevadm control --reload` and `sudo udevadm trigger`
5. Plug in your glasses, wait a few seconds
6. Run the driver from a terminal like `/path/to/nrealAirLinuxDriver` (e.g. `~/Downloads/nrealAirLinuxDriver`)
   * If you don't see a constant stream of numbers, try again until you do
  
Steam should now register your glasses as a new controller named `xReal Air virtual joystick`. From my testing so far I've found that games don't really like to have two controllers both providing joystick input, so they'll only use controller #1. So for now I've only gotten this to work by modifying the controller settings for a game and choosing "right joystick" option "joystick as mouse". Clicking the gear icon next to this will allow you to change the sensitivity, I've found it helps to increase the sensitivity a little bit, and you'll also want to change the response curive to linear and reduce the dead zone (the input has very little wobble to it, so doing this will allow for smaller/fine-grained movement).
