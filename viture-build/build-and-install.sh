#!/bin/bash

set -e

if [ "$EUID" -eq 0 ]; then
    echo "Please run this script as a regular (non-root) user"
    exit 1
fi

if [ -z "$1" ] || [ ! -f $1 ]; then
    echo "Usage: $0 <path-to-viture-sdk-static-lib>"
    exit 1
fi

rm -rf build/
cp $1 lib/libviture_one_sdk.a

docker run --rm -t -v ./:/source "viture-xr-driver"

sudo bin/xreal_driver_setup build/xrealAirLinuxDriver.tar.gz

sudo rm -rf build/

# enable and set to mouse-mode for testing
~/bin/xreal_driver_config -e
~/bin/xreal_driver_config -m