# Maintainer: hodasemi <michaelh.95 at t-online dot de>
_pkgbase=XRLinuxDriver
pkgname="${_pkgbase}"
pkgver=0.1
pkgrel=1
pkgdesc="XR Linux Driver"
arch=('x86_64')
url="https://github.com/wheaney/XRLinuxDriver"
license=('GPL-3.0')
makedepends=('cmake' 'make')
depends=('openssl' 'libevdev' 'libusb' 'json-c' 'curl')
conflicts=("${_pkgbase}")
source=("git+${url}")
md5sums=(SKIP)

build() {
    cd ${_pkgbase}

    # init submpdules
    git submodule update --init --recursive

    # build xr driver
    mkdir build/
    cd build
    cmake ..
    make
}

package() {
    # copy xr driver
    install -Dm755 ${_pkgbase}/build/xrealAirLinuxDriver "${pkgdir}"/usr/bin/xrealAirLinuxDriver
    sed -i '/ExecStart/c\ExecStart=xrealAirLinuxDriver' ${_pkgbase}/systemd/xreal-air-driver.service
    sed -i '/Environment/d' ${_pkgbase}/systemd/xreal-air-driver.service
    install -Dm644 ${_pkgbase}/systemd/xreal-air-driver.service "${pkgdir}"/usr/lib/systemd/user/xreal-air-driver.service
    install -Dm755 ${_pkgbase}/bin/xreal_driver_config "${pkgdir}"/usr/bin/xreal_driver_config
    install -Dm644 ${_pkgbase}/udev/80-viture-xr.rules "${pkgdir}"/usr/lib/udev/rules.d/80-viture-xr.rules
}

