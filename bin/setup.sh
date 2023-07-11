#!/usr/bin/env bash

# Make sure only root can run our script
if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

if lsmod | grep -q uinput; then
  echo "Setting up uinput kernel module"
  modprobe uinput
fi

# create temp directory
tmp_dir=$(mktemp -d -t xreal-air-XXXXXXXXXX)
pushd $tmp_dir > /dev/null
echo "Created temp directory: ${tmp_dir}"

# download and unzip the latest driver
echo "Downloading latest release to: ${tmp_dir}/xrealAirLinuxDriver.tar.gz"
wget -q https://github.com/wheaney/xrealAirLinuxDriver/releases/latest/download/xrealAirLinuxDriver.tar.gz

echo "Extracting to: ${tmp_dir}/driver_air_glasses"
tar -xf xrealAirLinuxDriver.tar.gz

pushd driver_air_glasses > /dev/null
UDEV_FILE=/etc/udev/rules.d/xreal_air.rules
if test -f "$UDEV_FILE"; then
  rm $UDEV_FILE
fi

echo "Copying udev file to ${UDEV_FILE}"
cp udev/xreal_air.rules $UDEV_FILE
udevadm control --reload
udevadm trigger

USER_HOME=$(getent passwd ${SUDO_USER:-$USER} | cut -d: -f6)
echo "Copying driver to ${USER_HOME}/bin"
if test ! -d "$USER_HOME/bin"; then
  mkdir $USER_HOME/bin
fi

cp xrealAirLinuxDriver $USER_HOME/bin

echo "Deleting temp directory: ${tmp_dir}"
rm -rf $tmp_dir