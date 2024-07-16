#!/bin/bash

set -e

# Create output directories
mkdir -p out/x86_64 out/aarch64 out/armv7

# Run containers for each architecture
sudo rm -rf build/
docker run --rm -t -v ./:/source -v ./out/x86_64:/out --platform linux/amd64 "xr-driver:amd64"

sudo rm -rf build/
docker run --rm -t -v ./:/source -v ./out/aarch64:/out --platform linux/arm64 "xr-driver:arm64"

sudo rm -rf build/
docker run --rm -t -v ./:/source -v ./out/armv7:/out --platform linux/arm/v7 "xr-driver:armv7"

# build directory structure is all owned by root because of docker, delete it all now
sudo rm -rf build/