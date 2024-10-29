#!/usr/bin/env bash

set -e

function libs_to_fpm_args {
    local lib_dir=$1
    local fpm_args=""
    for lib in $(ls $lib_dir); do
        fpm_args="$fpm_args lib/$lib=/usr/lib/$lib"
    done
    echo $fpm_args
}

function udev_files_to_fpm_args {
    local udev_dir=$1
    local fpm_args=""
    for udev_file in $(ls $udev_dir); do
        fpm_args="$fpm_args udev/$udev_file=/usr/lib/udev/rules.d/$udev_file"
    done
    echo $fpm_args
}

# Run containers for each architecture
if [[ "$1" == "x86_64" || -z "$1" ]]; then
    sudo rm -rf build/
    docker run --rm -t -v ./:/source --platform linux/amd64 -e UA_API_SECRET -e UA_API_SECRET_INTENTIONALLY_EMPTY "xr-driver:amd64" ./fpm/build

    lib_args=$(libs_to_fpm_args "build/xr_driver/lib")
    udev_args=$(udev_files_to_fpm_args "build/xr_driver/udev")
    fpm --architecture x86_64 --version 1.1.0 --iteration 1 \
        -t deb \
        --no-auto-depends \
        --depends libssl3 \
        --depends libevdev2 \
        --depends libusb-1.0-0 \
        --depends libjson-c5 \
        --depends libcurl4 \
        --depends libwayland-client0 \
        --depends libsystemd0 \
        $lib_args $udev_args

    # this fails without my fix in https://github.com/jordansissel/fpm/pull/2082
    fpm --architecture x86_64 --version 1.1.0 --iteration 1 \
        -t rpm \
        --no-auto-depends \
        --depends openssl-libs \
        --depends libevdev \
        --depends libusbx \
        --depends json-c \
        --depends libcurl \
        --depends libwayland-client \
        --depends systemd-libs \
        $lib_args $udev_args
fi

if [[ "$1" == "aarch64" || -z "$1"  ]]; then
    sudo rm -rf build/
    docker run --rm -t -v ./:/source --platform linux/arm64 -e UA_API_SECRET -e UA_API_SECRET_INTENTIONALLY_EMPTY "xr-driver:arm64" ./fpm/build

    lib_args=$(libs_to_fpm_args "build/xr_driver/lib")
    udev_args=$(udev_files_to_fpm_args "build/xr_driver/udev")
    fpm --architecture aarch64 --version 1.1.0 --iteration 1 \
        -t deb \
        --no-auto-depends \
        --depends libssl3 \
        --depends libevdev2 \
        --depends libusb-1.0-0 \
        --depends libjson-c5 \
        --depends libcurl4 \
        --depends libwayland-client0 \
        --depends libsystemd0 \
        $lib_args $udev_args

    # this fails without my fix in https://github.com/jordansissel/fpm/pull/2082
    fpm --architecture aarch64 --version 1.1.0 --iteration 1 \
        -t rpm \
        --no-auto-depends \
        --depends openssl-libs \
        --depends libevdev \
        --depends libusbx \
        --depends json-c \
        --depends libcurl \
        --depends libwayland-client \
        --depends systemd-libs \
        $lib_args $udev_args
fi

mv xr-driver*.deb out/
mv xr-driver*.rpm out/

# build directory structure is all owned by root because of docker, delete it all now
sudo rm -rf build/