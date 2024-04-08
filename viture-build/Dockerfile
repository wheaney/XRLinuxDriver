# To run the build from the package root:
#    docker build -f ./viture-build/Dockerfile -t "viture-xr-driver" .
#    docker run --rm -t -v ./:/source "viture-xr-driver" <path-to-sdk-static-lib>

FROM debian:latest

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
ENV UA_API_SECRET_INTENTIONALLY_EMPTY=1
CMD [ "bin/package" ]