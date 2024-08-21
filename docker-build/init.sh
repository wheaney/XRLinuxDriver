#!/bin/bash

# might be needed on a fresh docker setup:
#   install qemu and qemu-user-static packages
#   sudo docker context rm default
#   docker run --privileged --rm tonistiigi/binfmt --install all
#   sudo docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
#   ls -l /proc/sys/fs/binfmt_misc/ # should contain qemu-<arch> files

if [[ "$1" == "--init" || ! $(docker buildx inspect xrdriverbuilder &>/dev/null; echo $?) -eq 0 ]]; then
    # start fresh
    echo "Creating new docker builder instance"
    docker buildx rm xrdriverbuilder 2>/dev/null || true
    docker buildx create --use --name xrdriverbuilder --driver docker-container --driver-opt image=moby/buildkit:latest
else
    echo "Using existing docker builder instance"
    docker buildx use xrdriverbuilder
fi

echo "Building docker image"
docker buildx build --platform linux/amd64 -f ./docker-build/Dockerfile -t "xr-driver:amd64" --load .
docker buildx build --platform linux/arm64 -f ./docker-build/Dockerfile -t "xr-driver:arm64" --load .