# Custom user-space Linux driver for the XREAL Air

## Information before use

The code is provided as is and it's free to use. However the contributors can neither guarantee that 
it will work or that it won't damage your device since all of it is based on reverse-engineering 
instead of public documentation. The contributors are not responsible for proper or even official 
support. So use it at your own risk!

## Inspiration and motivation

This is a forked package. See [the original](https://gitlab.com/TheJackiMonster/nrealAirLinuxDriver) for details. 

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

Fusion and hidapi source are included as Git submodules that you'll need to check out using the `git submodules` command. json-c may already be installed with your distro, and you'll want to install libevdev (e.g. `sudo apt install libevdev-dev`)

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
6. Run the driver from a terminal like `/path/to/xrealAirLinuxDriver` (e.g. `~/Downloads/xrealAirLinuxDriver`)
   * If you don't see a constant stream of numbers, try again until you do
  
Steam should now register your glasses as a new controller named `xReal Air virtual joystick`. From my testing so far I've found that games don't really like to have two controllers both providing joystick input, so they'll only use controller #1. So for now I've only gotten this to work by modifying the controller settings for a game and choosing "right joystick" option "joystick as mouse". Clicking the gear icon next to this will allow you to change the sensitivity, I've found it helps to increase the sensitivity a little bit, and you'll also want to change the response curive to linear. Lastly, you'll want to reduce the dead zone (the input has very little wobble to it, so doing this will allow for smaller/fine-grained movement), but this should be done from the general controller settings and not just in a game-specific controller layout; I've found that setting the dead zone somewhere between 2000 and 3000 is best.

### Steam Deck Game Mode

On Steam Deck, once you have the driver working from Desktop mode, you can get it working in Game Mode as well by (from Desktop mode) clicking the `Add a Game` button in Steam, then `Add a Non-Steam Game`, then `Browse...`, navigate to the `xrealAirLinuxDriver` that you downloaded, click `Select`, make sure its checkbox is checked in the `Add Non-Steam Game` dialog, then finally click the `Add Selected Program` button. Now you should be able to find `xrealAirLinuxDriver` from the games sidebar. You'll probably want to flag it as a Favorite so it's easy to locate. From Game Mode, you'll want to find the driver in your favorites, use `Play` to get it running, then hit the `Steam` button, return to your Home or Library and choose the game you want to play. Keep in mind that, for the moment, the driver doesn't always work on the first try, so if you're not getting mouse movement from the glasses while in-game, then double-check you've set up the right xReal joystick using `Joystick as Mouse` in the controller settings for that game, and otherwise, exit and restart the driver program.
