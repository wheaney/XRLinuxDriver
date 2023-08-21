#!/usr/bin/env bash

# exit when any command fails
set -e

# check out submodules, recursively for nested ones
git submodule update --init --recursive

# build the driver
BUILD_PATH=build
if [ ! -d "$BUILD_PATH" ]; then
  mkdir $BUILD_PATH
fi

pushd $BUILD_PATH
cmake ..
make

# create package
PACKAGE_DIR=driver_air_glasses
if [ ! -d "$PACKAGE_DIR" ]; then
  mkdir $PACKAGE_DIR
fi

# move and rename the compiled driver to the driver directory
mv xrealAirLinuxDriver $PACKAGE_DIR

# copy user-relevant scripts
cp ../bin/xreal_driver_config $PACKAGE_DIR

# copy the udev rule that's needed for the USB integration
cp -r ../udev $PACKAGE_DIR

# bundle up the driver directory
tar -zcvf xrealAirLinuxDriver.tar.gz $PACKAGE_DIR

popd