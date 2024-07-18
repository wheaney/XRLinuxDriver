#!/bin/bash

set -e

# Create output directories
mkdir -p out

# Run containers for each architecture
if [[ "$1" == "x86_64" || -z "$1" ]]; then
    sudo rm -rf build/
    docker run --rm -t -v ./:/source -v ./out:/out --platform linux/amd64 -e UA_API_SECRET -e UA_API_SECRET_INTENTIONALLY_EMPTY -e BREEZY_DESKTOP "xr-driver:amd64"
fi

if [[ "$1" == "aarch64" || -z "$1"  ]]; then
    sudo rm -rf build/
    docker run --rm -t -v ./:/source -v ./out:/out --platform linux/arm64 -e UA_API_SECRET -e UA_API_SECRET_INTENTIONALLY_EMPTY -e BREEZY_DESKTOP "xr-driver:arm64"
fi

# build directory structure is all owned by root because of docker, delete it all now
sudo rm -rf build/