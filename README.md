# XR Linux Driver

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/U7U8OVC0L)

[![Chat](https://img.shields.io/badge/chat-on%20discord-7289da.svg)](https://discord.gg/azSBTXNXMt)

## What is this?

This driver allows your Linux device (including Steam Deck) to automatically recognize supported XR glasses (see [Supported Devices](#supported-devices)) when they're plugged in, and convert the movements of the glasses into mouse movements and an external broadcast that games or any application can utilize.

If you're looking for a 3dof virtual display, this driver by itself does not provide that functionality; instead, see [Breezy](https://github.com/wheaney/breezy-desktop) or [use the Steam Deck plugin](#steam-deck-via-decky-loader) which installs Breezy under the hood.

## Supported Devices
Check below to see if your device is supported. **Note: be sure you're on the latest firmware for your device.**

| Brand    | Model             | Status            | Recommended?       | x86_64 (AMD64) | AARCH64 (ARM64) | Firmware updates                                                | Notes                                                                                                                                   |
| -------- | ----------------- | ----------------- | ------------------ | ------ | ------------- | --------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| VITURE   | One,One Lite, Pro | **Live** | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | [Official update site](https://static.viture.com/dfu-util/). Requires Chrome on Windows/Mac. | Official collaboration. [Closed source SDK available](https://www.viture.com/developer/viture-one-sdk-for-linux). |
| TCL/RayNeo | NXTWEAR S/S+; Air 2 | **Live** | :heavy_check_mark: | :heavy_check_mark: |  |                                                         | Official collaboration, closed source SDK                                                                                               |
| Rokid    | Max, Air          | **Live** (v0.10.7, not yet in decky)   | :heavy_check_mark: | :heavy_check_mark: |  |                                             | Official collaboration, closed source SDK                                                                                               |
| XREAL    | Air 1, 2, 2 Pro   | **Live**          | :x:                | :heavy_check_mark: | :heavy_check_mark: | [Officlal update site](https://www.xreal.com/support/update/). Requires Chrome. | Unwilling to collaborate. [Unofficial, open-source SDK](https://gitlab.com/TheJackiMonster/nrealAirLinuxDriver). Exhibits drift, noise. |

## Usage

### Steam Deck via Decky Loader

For Steam Deck users, the driver is available via the [Decky plugin loader](https://github.com/SteamDeckHomebrew/decky-loader). Just search "xr" in the Decky store to install and use without leaving Gaming Mode. You can now enable or disable the driver and manage other driver settings via the Decky sidebar menu.

You may still opt to do a manual installation using the instructions below if you enter Desktop Mode.

### Manual installation

*Note: this installation is for just the base driver with mouse/joystick support. If you're looking for virtual display mode, check out the [breezy-desktop setup](https://github.com/wheaney/breezy-desktop#setup).*

1. [Download the setup script](https://github.com/wheaney/XRLinuxDriver/releases/latest/download/xr_driver_setup) and set the execute flag (e.g. from the terminal: `chmod +x ~/Downloads/xr_driver_setup`)
2. Run the setup script as root (e.g. `sudo ~/Downloads/xr_driver_setup`)
  
Your device should now automatically recognize when your glasses are plugged in and translate their movements to mouse movements. If you're not seeing this, check the log at `$XDG_STATE_HOME/xr_driver/driver.log` and report an Issue here with its contents.

#### Turning automatic driver usage on or off

To disable the driver and turn off mouse movements without completely removing it, you can use the config script (e.g. `xr_driver_cli -d` to disable, and `-e` to re-enable). Run this script without arguments to see its usage. Configs are stored in the file `$XDG_CONFIG_HOME/xr_driver/config.ini`.

### Practical Usage

Since the device movements are converted to mouse movements, they should be recognized by any PC game that supports keyboard/mouse input. This will work most naturally for games where mouse movements is used to control "look"/camera movements. For point-and-click style games, you may want to disable the driver so your glasses act as just a simple display.

To adjust the sensitivity of mapping head movements to mouse movements, use the Decky UI on Steam Deck, or `xr_driver_cli --mouse-sensitivity 20` via the terminal.

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

One last alternative if mouse input just won't work is to enable the driver's joystick mode, using the Decky UI on Steam Deck, or `xr_driver_cli --use-joystick` via the terminal (you can revert this with `--use-mouse`). This will create a virtual gamepad whose right joystick is driven by movements from the glasses. This is less ideal because joystick movements are technically capped (you can only move a joystick so far...) and because it's a *second* controller on your PC. If the game you're trying to play is okay being driven by two controllers, then this may work, but if your game interprets another controller as a second player then its movements won't get combined with your real controller's movements.

### Updating

If using Decky, updates are installed through Decky.

Otherwise, just rerun the `xr_driver_setup` file. No need to redownload this script, as it will automatically download the latest installation binary for you.

### Uninstalling

If you wish to completely remove the installation, run the following script as root: `~/.local/bin/xr_driver_uninstall`. For Steam Deck users, you can uninstall the plugin via the Decky interface, but you'll still need to manually run the terminal command from Desktop Mode to complete the uninstall until [this Decky feature request](https://github.com/SteamDeckHomebrew/decky-loader/issues/536) is addressed.

## Development

### Dependencies

You can build the binary using `cmake` and there are a few dependencies for now:
 - [hidapi](https://github.com/libusb/hidapi)
 - [json-c](https://github.com/json-c/json-c/)
 - [Fusion](https://github.com/xioTechnologies/Fusion)
 - [libevdev](https://gitlab.freedesktop.org/libevdev/libevdev)

Fusion and hidapi source are included as Git submodules that you'll need to check out using the `git submodules` command. json-c may already be installed with your distro, and you'll want to install libevdev (e.g. `sudo apt install libevdev-dev`).

### Build & Testing

For local testing, you'll want to use the same package as for deployment: run `bin/package` to create the gzip, then run `sudo bin/xr_driver_setup $(pwd)/build/xrDriver.tar.gz` to install and test it.

### Deployment

To create the deployment gzip file, run `bin/package`. Upload the resulting gzip file and the `bin/xr_driver_setup` file to a new Release.

## Data Privacy Notice

Your right to privacy and the protection of your personal data are baked into every decision around how your personal data is collected, handled and stored. Your personal data will never be shared, sold, or distributed in any form.

### Data Collected

In order to provide you with Supporter Tier features, this application and its backend services have to collect the following pieces of personal information:

* Your email address is sent to this application's backend server from either the payment vendor (Ko-fi) or from your device (at your request). Your email address may be used immediately upon receipt in its unaltered form to send you a transactional email, but it is then hashed prior to storage. The unaltered form of your email address is never stored and can no longer be referenced. The hashed value is stored for later reference.
  * Other personal data may be sent from the payment vendor, but is never utilized nor stored. 
* Your device's MAC address is hashed on your device. It never leaves your device in its original, unaltered form. The hashed value is sent to this application's backend server and stored for later reference.

Hashing functions are a one-way process that serve to anonymize your personal data by irreversibly changing them. Once hashed, they can never be unhashed or traced back to their original values.

### Contact

For inquires about data privacy or any related concerns, please contact:

Wayne Heaney - **wayne@xronlinux.com**

## Credits

This driver wouldn't have been possible without the work of Tobias Frisch for his [base Linux driver](https://gitlab.com/TheJackiMonster/nrealAirLinuxDriver) that this uses under the hood, Matt Smith for his [Windows driver](https://github.com/MSmithDev/AirAPI_Windows/), and others that worked to [reverse engineer the glasses](https://github.com/edwatt/real_utilities/).
