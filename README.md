# XR Linux Driver

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/U7U8OVC0L)

[![Chat](https://img.shields.io/badge/chat-on%20discord-7289da.svg)](https://discord.gg/azSBTXNXMt)

[![Documentation Status](https://readthedocs.org/projects/xrlinuxdriver/badge/?version=latest)](https://xrlinuxdriver.readthedocs.io/en/latest/?badge=latest)

## What is this?

This driver allows your Linux device (including Steam Deck) to automatically recognize supported XR glasses (see [Supported Devices](#supported-devices)) when they're plugged in, and convert the movements of the glasses into mouse movements and an external broadcast that games or any application can utilize.

If you're looking for a 3DoF virtual display, this driver by itself does not provide that functionality; instead, see [Breezy Desktop](https://github.com/wheaney/breezy-desktop) or [use the Steam Deck plugin](#steam-deck-via-decky-loader).

## Supported Devices
Check below to see if your device is supported. **Note: be sure you're on the latest firmware for your device.**

| Brand    | Model             | Support?            | Recommend?   | x86_64 (AMD64) | AARCH64 (ARM64) | Firmware updates                                          | Notes                                                                                                                                   |
| -------- | ----------------- | -----------------   | ------------ | -------------- | --------------- | --------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| VITURE   | One<br/>One&nbsp;Lite<br/>Pro&nbsp;XR<br/>Luma<br/>Luma&nbsp;Pro | :heavy_check_mark: | :+1: | :heavy_check_mark: | :heavy_check_mark: | [Official update site](https://static.viture.com/dfu-util/). Requires Chrome on Windows/Mac. | Official collaboration. [Closed source SDK](https://www.viture.com/developer/viture-one-sdk-for-linux). |
| VITURE   | Luma&nbsp;Ultra | :heavy_check_mark:&nbsp;XR driver v2.6.x<br/><br/>:heavy_check_mark:&nbsp;Breezy v2.6.x<br/><br/>:heavy_check_mark:&nbsp;Decky XR Gaming v1.4.x | :+1: | :heavy_check_mark: | :heavy_check_mark: | [Official update site](https://static.viture.com/dfu-util/). Requires Chrome on Windows/Mac. | Official collaboration. [Closed source SDK](https://www.viture.com/developer/viture-one-sdk-for-linux). |
| VITURE   | Beast | :clock4: Coming soon... |  |  |  |  | Support requires updates to the official SDK. |
| TCL/RayNeo | NXTWEAR&nbsp;S/S+<br/>Air&nbsp;2<br/>2s<br/>3s/Pro | :heavy_check_mark: | :+1: | :heavy_check_mark: |             |                                                       | Official collaboration, closed source SDK.                                                                                              |
| Rokid    | Max<br/>Air          | :heavy_check_mark: | :+1: | :heavy_check_mark:              |             |                                                       | Official collaboration, closed source SDK.                                                                                              |
| XREAL    | Air<br/>Air&nbsp;2<br/>Air&nbsp;2&nbsp;Pro<br/>Air&nbsp;2&nbsp;Ultra  | :heavy_check_mark: | :-1: | :heavy_check_mark: | :heavy_check_mark: | [Official update site](https://www.xreal.com/support/update/). Requires Chrome. | Unwilling to collaborate. [Unofficial, open-source SDK](https://gitlab.com/TheJackiMonster/nrealAirLinuxDriver). Exhibits drift. |
| XREAL    | One<br/>One&nbsp;Pro | :heavy_check_mark:&nbsp;XR&nbsp;driver&nbsp;v2.4.x<br/><br/>:heavy_check_mark:&nbsp;Breezy&nbsp;Desktop&nbsp;v2.4.x<br/><br/>:heavy_check_mark:&nbsp;Decky XR Gaming v1.3.x<br/><br/>**See notes before use** | :-1: | :heavy_check_mark: | :heavy_check_mark: | [Official update site](https://www.xreal.com/support/update/). Requires Chrome. | **Important** - Disable stabilizer/anchor features on glasses. Must be on latest firmware. |

## Setup

### Manual installation

*Note: this installation is for just the base driver with mouse/joystick support. If you're looking for virtual display or workspace tools, check out [Breezy Desktop](https://github.com/wheaney/breezy-desktop).*

1. [Download the setup script](https://github.com/wheaney/XRLinuxDriver/releases/latest/download/xr_driver_setup) and set the execute flag (e.g. from the terminal: `chmod +x ~/Downloads/xr_driver_setup`)
2. Run the setup script as root (e.g. `sudo ~/Downloads/xr_driver_setup`)
  
Your device should now automatically recognize when your glasses are plugged in and translate their movements to mouse movements. If you're not seeing this, check the log at `$XDG_STATE_HOME/xr_driver/driver.log` and report an Issue here with its contents.

#### Turning automatic driver usage on or off

To disable the driver and turn off mouse movements without completely removing it, you can use the config script (e.g. `xr_driver_cli -d` to disable, and `-e` to re-enable). Run this script without arguments to see its usage. Configs are stored in the file `$XDG_CONFIG_HOME/xr_driver/config.ini`.

### Updating

If using Decky, updates are installed through Decky.

Otherwise, just rerun the `xr_driver_setup` file. No need to redownload this script, as it will automatically download the latest installation binary for you.

### Uninstalling

If you wish to completely remove the installation, run the following script as root: `~/.local/bin/xr_driver_uninstall`. For Steam Deck users, you can uninstall the plugin via the Decky interface.

## Data Privacy Notice

Your right to privacy and the protection of your personal data are baked into every decision around how your personal data is collected, handled and stored. Your personal data will never be shared, sold, or distributed in any form.

### Data Collected

In order to provide you with Supporter Tier features, this application and its backend services have to collect the following pieces of personal information:

* Your email address is sent to this application's backend server from either the payment vendor (Ko-fi) or from your device (at your request). Your email address may be used immediately upon receipt in its unaltered form to send you a transactional email, but it is then hashed prior to storage. The unaltered form of your email address is never stored and can no longer be referenced. The hashed value is stored for later reference.
  * Other personal data may be sent from the payment vendor, but is never utilized nor stored. 
* Your device's MAC address is hashed on your device. It never leaves your device in its original, unaltered form. The hashed value is sent to this application's backend server and stored for later reference.
* Metrics are collected using Google Analytics. No personal information is explicitly collected, and Google Analytics will anonymize the IP address that's sent as part of the HTTP request. You can disable metrics collection using `xr_driver_cli --no-metrics`.

Hashing functions are a one-way process that serve to anonymize your personal data by irreversibly changing them. Once hashed, they can never be unhashed or traced back to their original values.

### Contact

For inquires about data privacy or any related concerns, please contact:

Wayne Heaney - **wayne@xronlinux.com**

## Credits

This driver wouldn't have been possible without the work of Tobias Frisch for his [base Linux driver](https://gitlab.com/TheJackiMonster/nrealAirLinuxDriver) that this uses under the hood, Matt Smith for his [Windows driver](https://github.com/MSmithDev/AirAPI_Windows/), and others that worked to [reverse engineer the glasses](https://github.com/edwatt/real_utilities/).
