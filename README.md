# Custom user-space Linux driver for the XREAL Air

## What is this?

This driver allows your Linux device (including Steam Deck) to automatically recognize XREAL Air glasses when they're plugged in, and convert the accelerometer movements of the glasses into controller joystick movements that Steam (and probably non-Steam platforms) can recognize.

## Before use

The driver is provided as-is and it's free to use. However the contributors can neither guarantee that 
it will work or that it won't damage your device since all of it is based on reverse-engineering 
instead of public documentation. The contributors are not responsible for proper or even official 
support. So use it at your own risk!

## Usage

### Installation

To use this driver:
1. If your glasses are already plugged in, unplug them
2. If on Steam Deck, switch to Desktop Mode
3. [Download the setup script](https://github.com/wheaney/xrealAirLinuxDriver/releases/latest/download/xreal_driver_setup) and set the execute flag (e.g. from the terminal: `chmod +x ~/Downloads/xreal_driver_setup`)
4. Run the setup script as root (e.g. `sudo ~/Downloads/xreal_driver_setup`)
5. Plug in your glasses, wait a few seconds
  
Steam should now register your glasses as a new controller named `xReal Air virtual joystick`. If you're not seeing this, check the log at `~/.xreal_driver_log` and report an Issue here with its contents.

### Turning automatic driver usage on or off

After initial installation, the driver will automatically run whenever the glasses are plugged in. To disable the driver without completely removing it, you can use the script installed at `~/bin/xreal_driver_config`. From this script, you can enable the driver, disable it, or report its status. Run this file without arguments to see its usage. Configs are stored in the file `~/.xreal_driver_config`.

### Practical Usage

From my testing so far I've found that games don't really like to have two controllers both providing joystick input, so they'll only use controller #1. So for now I've only gotten this to work by modifying the controller settings for a game and choosing "right joystick" option "joystick as mouse". Clicking the gear icon next to this will allow you to change the sensitivity, I've found it helps to increase the sensitivity a little bit, and you'll also want to change the response curve to Linear. Lastly, you'll want to reduce the dead zone or simply disable the dead zone if you find that doesn't result in drift, but this should be done from the general controller settings and not just in a game-specific controller layout.

### Steam Deck Game Mode

On Steam Deck, once you have the driver working from Desktop mode, it should also work automatically from Game Mode.

### Updating

If you already installed **release 0.2.2 and later**: to update to the latest version of the driver, just rerun the `xreal_driver_setup` file (no need to re-download the setup script).

**Prior to release 0.2.2**: remove any old versions of `xreal_driver_setup` from your browser's downloads directory, then repeat the [Installation steps](#installation). After this, you can always re-use the same setup script to update.

### Uninstalling

If you wish to completely remove the installation, run the following script as root: `~/bin/xreal_driver_uninstall`

## Development

### Dependencies

You can build the binary using `cmake` and there are a few dependencies for now:
 - [hidapi](https://github.com/libusb/hidapi)
 - [json-c](https://github.com/json-c/json-c/)
 - [Fusion](https://github.com/xioTechnologies/Fusion)
 - [libevdev](https://gitlab.freedesktop.org/libevdev/libevdev)

Fusion and hidapi source are included as Git submodules that you'll need to check out using the `git submodules` command. json-c may already be installed with your distro, and you'll want to install libevdev (e.g. `sudo apt install libevdev-dev`).

### Build

The build process should be straight forward:

```
mkdir build
cd build
cmake ..
make
```

### Deployment

To create the deployment gzip file, run `bin/package`. Upload the resulting gzip file and the `bin/xreal_driver_setup` file to a new Release.
