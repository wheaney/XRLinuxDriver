#!/bin/bash

set -e

USER=${SUDO_USER:-$USER}
GROUP=$(id -gn $USER)

# Run containers for each architecture
if [[ "$1" == "x86_64" || -z "$1" ]]; then
    docker run --rm -t -v ./:/source --platform linux/amd64 -e UA_API_SECRET -e UA_API_SECRET_INTENTIONALLY_EMPTY -e BREEZY_DESKTOP "xr-driver:amd64"
    sudo chown -R $USER:$GROUP out/
fi

if [[ "$1" == "aarch64" || -z "$1"  ]]; then
    docker run --rm -t -v ./:/source --platform linux/arm64 -e UA_API_SECRET -e UA_API_SECRET_INTENTIONALLY_EMPTY -e BREEZY_DESKTOP "xr-driver:arm64"
    sudo chown -R $USER:$GROUP out/
fi