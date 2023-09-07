# Custom user-space Linux driver for the XREAL Air

## What is this?

This driver allows your Linux device (including Steam Deck) to automatically recognize XREAL Air glasses when they're plugged in, and convert the accelerometer movements of the glasses into mouse movements that PC games can use.

## Before use

The driver is provided as-is and it's free to use. However the contributors can neither guarantee that 
it will work or that it won't damage your device since all of it is based on reverse-engineering 
instead of public documentation. The contributors are not responsible for proper or even official 
support. So use it at your own risk!

## Usage

### Steam Deck via Decky Loader

For Steam Deck users, the driver is now available via the [Decky plugin loader](https://github.com/SteamDeckHomebrew/decky-loader). Just search "xreal" in the Decky store to install and use without leaving Gaming Mode. You can now enable or disable the driver and manage other driver settings via the Decky sidebar menu.

You may still opt to do a manual installation using the instructions below if you enter Desktop Mode.

### Manual installation

1. [Download the setup script](https://github.com/wheaney/xrealAirLinuxDriver/releases/latest/download/xreal_driver_setup) and set the execute flag (e.g. from the terminal: `chmod +x ~/Downloads/xreal_driver_setup`)
2. Run the setup script as root (e.g. `sudo ~/Downloads/xreal_driver_setup`)
  
Your device should now automatically recognize when your glasses are plugged in and translate their movements to mouse movements. If you're not seeing this, check the log at `~/.xreal_driver_log` and report an Issue here with its contents.

#### Turning automatic driver usage on or off

To disable the driver and turn off mouse movements without completely removing it, you can use the config script (e.g. `~/bin/xreal_driver_config -d` to disable, and `-e` to re-enable). Run this script without arguments to see its usage. Configs are stored in the file `~/.xreal_driver_config`.

### Practical Usage

Since the device movements are converted to mouse movements, they should be recognized by any PC game that supports keyboard/mouse input. This will work most naturally for games where mouse movements is used to control "look"/camera movements. For point-and-click style games, you may want to disable the driver so your glasses act as just a simple display.

To adjust the sensitivity of mapping head movements to mouse movements, use the Decky UI on Steam Deck, or `~/bin/xreal_driver_config --mouse-sensitivity 20` via the terminal.

If you're using keyboard and mouse to control your games, then the mouse movements from this driver will simply add to your own mouse movements and they should work naturally together.

If you're using a game controller, Valve pushes pretty heavily for PC games support mouse input *in addition to* controller input, so you should find that most modern games will just work with this driver straight out of the box.

If your game doesn't support blending mouse and controller movement well, the best option may be to convert your controller's input to keyboard and mouse. Read on for how to do this for Steam and non-Steam games.

#### Steam

1. Open your game's controller configuration in Steam
2. Open the Layouts view
3. Choose a keyboard/mouse template (e.g. "Keyboard (WASD) and Mouse"). Be sure to edit the configuration and set "Gyro Behavior" to "As Mouse" for any games where you want to use gyro.

#### Non-Steam

You'll probably want to use a utility that does what Steam's controller layouts are doing behind the scenes: mapping controller buttons, joystick, and gyro inputs to keyboard/mouse inputs. One popular tool is [JoyShockMapper](https://github.com/Electronicks/JoyShockMapper).

#### Enable joystick mode

One last alternative if mouse input just won't work is to enable the driver's joystick mode, using the Decky UI on Steam Deck, or `~/bin/xreal_driver_config --use-joystick` via the terminal (you can revert this with `--use-mouse`). This will create a virtual gamepad whose right joystick is driven by movements from the glasses. This is less ideal because joystick movements are technically capped (you can only move a joystick so far...) and because it's a *second* controller on your PC. If the game you're trying to play is okay being driven by two controllers, then this may work, but if your game interprets another controller as a second player then its movements won't get combined with your real controller's movements.

### Updating

If using Decky, updates are installed through Decky.

Otherwise, just rerun the `xreal_driver_setup` file. No need to redownload this script, as it will automatically download the latest installation binary for you.

### Uninstalling

If you wish to completely remove the installation, run the following script as root: `~/bin/xreal_driver_uninstall`. For Steam Deck users, you can uninstall the plugin via the Decky interface, but you'll still need to manually run the terminal command from Desktop Mode to complete the uninstall until [this Decky feature request](https://github.com/SteamDeckHomebrew/decky-loader/issues/536) is addressed.

## Development

### Dependencies

You can build the binary using `cmake` and there are a few dependencies for now:
 - [hidapi](https://github.com/libusb/hidapi)
 - [json-c](https://github.com/json-c/json-c/)
 - [Fusion](https://github.com/xioTechnologies/Fusion)
 - [libevdev](https://gitlab.freedesktop.org/libevdev/libevdev)

Fusion and hidapi source are included as Git submodules that you'll need to check out using the `git submodules` command. json-c may already be installed with your distro, and you'll want to install libevdev (e.g. `sudo apt install libevdev-dev`).

### Build & Testing

For local testing, you'll want to use the same package as for deployment: run `bin/package` to create the gzip, then run `sudo bin/xreal_driver_setup $(pwd)/build/xrealAirLinuxDriver.tar.gz` to install and test it.

### Deployment

To create the deployment gzip file, run `bin/package`. Upload the resulting gzip file and the `bin/xreal_driver_setup` file to a new Release.
