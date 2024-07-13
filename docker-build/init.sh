#!/bin/bash

if [[ "$1" == "--init" || ! $(docker buildx inspect xrdriverbuilder &>/dev/null; echo $?) -eq 0 ]]; then
    # start fresh
    echo "Creating new docker builder instance"
    docker buildx rm xrdriverbuilder 2>/dev/null || true
    docker buildx create --name xrdriverbuilder --use
else
    echo "Using existing docker builder instance"
    docker buildx use xrdriverbuilder
fi

echo "Building docker image"
docker buildx build --platform linux/amd64,linux/arm64 -f ./docker-build/Dockerfile -t "xr-driver" .