# Custom user-space Linux driver for the XREAL Air

## What is this?

This driver allows your Linux device (including Steam Deck) to automatically recognize XREAL Air glasses when they're plugged in, and convert the accelerometer movements of the glasses into mouse movements that PC games can use.

## Before use

The driver is provided as-is and it's free to use. However the contributors can neither guarantee that 
it will work or that it won't damage your device since all of it is based on reverse-engineering 
instead of public documentation. The contributors are not responsible for proper or even official 
support. So use it at your own risk!

## Usage

### Installation

To use this driver:
1. If on Steam Deck, switch to Desktop Mode
2. [Download the setup script](https://github.com/wheaney/xrealAirLinuxDriver/releases/latest/download/xreal_driver_setup) and set the execute flag (e.g. from the terminal: `chmod +x ~/Downloads/xreal_driver_setup`)
3. Run the setup script as root (e.g. `sudo ~/Downloads/xreal_driver_setup`)
  
Steam should now register your glasses as a new controller named `xReal Air virtual joystick`. If you're not seeing this, check the log at `~/.xreal_driver_log` and report an Issue here with its contents.

### Turning automatic driver usage on or off

After initial installation, the driver will automatically run whenever the glasses are plugged in. To disable the driver without completely removing it, you can use the script installed at `~/bin/xreal_driver_config`. From this script, you can enable the driver, disable it, or report its status. Run this file without arguments to see its usage. Configs are stored in the file `~/.xreal_driver_config`.

### Practical Usage

Since the device movements are converted to mouse movements, they should be recognized by any PC game that supports keyboard/mouse input. This will work most naturally for games where mouse movements is used to control "look"/camera movements. For point-and-click style games, you may want to disable the driver so your glasses act as just a simple display.

To adjust the sensitivity of mapping head movements to mouse movements, use the config script: `~/bin/xreal_driver_config --mouse-sensitivity 20` 

If you're using keyboard and mouse to control your games, then the mouse movements from this driver will simply add to your own mouse movements and they should work naturally together.

If you're using a game controller, Valve pushes pretty heavily for PC games support mouse input *in addition to* controller input, so you should find that most modern games will just work with this driver straight out of the box.

If your game doesn't support blending mouse and controller movement well, the best option may be to convert your controller's input to keyboard and mouse. Read on for how to do this for Steam and non-Steam games.

#### Steam

1. Open your game's controller configuration in Steam
2. Open the layouts view
3. Choose a keyboard/mouse template (e.g. "Keyboard (WASD) and Mouse"). Be sure to edit the configuration and set "Gyro Behavior" to "As Mouse" for any games where you want to use gyro.

#### Non-Steam

You'll probably want to use a utility that does what Steam's controller layouts are doing behind the scenes: mapping controller buttons, joystick, and gyro inputs to keyboard/mouse inputs. One popular tool is [JoyShockMapper](https://github.com/Electronicks/JoyShockMapper).

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
