#!/bin/bash

# docker buildx rm xrdriverbuilder
docker buildx use xrdriverbuilder
docker buildx build --platform linux/amd64,linux/arm64 -f ./docker-build/Dockerfile -t "xr-driver" .