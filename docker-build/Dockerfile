# To run the build from the package root:
#    docker buildx build --platform linux/amd64,linux/arm64 -f ./docker-build/Dockerfile -t "xr-driver" .
#    docker run --rm -t -v ./:/source -v --platform linux/amd64 "xr-driver:amd64"
#    docker run --rm -t -v ./:/source -v --platform linux/arm64 "xr-driver:arm64"

FROM --platform=$TARGETPLATFORM debian:latest

ARG TARGETPLATFORM
RUN echo "Target platform: $TARGETPLATFORM"

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libudev-dev \
    libusb-1.0-0-dev \
    libcurl4-openssl-dev \
    libevdev-dev \
    libssl-dev \
    libjson-c-dev \
    python3 \
    python3-yaml

WORKDIR /source

CMD bin/package