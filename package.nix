{
  # Core inputs
  self,
  lib,
  pkgs,
  stdenv,
  autoPatchelfHook,
  # xrDriver deps
  libusb1,
  curl,
  openssl,
  libevdev,
  json_c,
  hidapi,
  wayland,
  cmake,
  pkg-config,
  python3,
  libffi,
  systemd,
  makeWrapper,
  jq,
  # nrealAirLinuxDriver deps
  rustPlatform,
  rustc,
  cargo,
  ...
}:
let
  arch =
    let
      inherit (pkgs.stdenv.hostPlatform) isx86_64 isLinux isAarch64;
    in
    if isAarch64 && isLinux then
      "aarch64"
    else if isx86_64 && isLinux then
      "x86_64"
    else
      throw "Unsupported system ${pkgs.stdenv.hostPlatform.system}";
in
stdenv.mkDerivation (finalAttrs: {
  pname = "xrlinuxdriver";
  version = "2.0.5";

  src = lib.cleanSourceWith {
    src = self;
    name = "${finalAttrs.pname}-src";
  };

  cargoRoot = "modules/xrealInterfaceLibrary/interface_lib/modules/xreal_one_driver";

  cargoDeps = rustPlatform.importCargoLock {
    lockFile = "${finalAttrs.src}/${finalAttrs.cargoRoot}/Cargo.lock";
  };

  nativeBuildInputs =
    let
      pythonEnv = python3.withPackages (ps: [ ps.pyyaml ]);
    in
    [
      cmake
      pkg-config
      pythonEnv
      autoPatchelfHook
      rustPlatform.cargoSetupHook
      rustc
      cargo
      makeWrapper
    ];
  buildInputs = [
    curl
    hidapi
    json_c
    libevdev
    libffi
    libusb1
    openssl
    systemd
    wayland
  ];

  # The vendor .so blobs and hidapi-hidraw need libudev at link time
  NIX_LDFLAGS = "-ludev";

  autoPatchelfIgnoreMissingDeps = [ "libopencv_*" ];

  installPhase = ''
    runHook preInstall

    # Main binary
    install -Dm755 xrDriver $out/bin/xrDriver

    # Vendor SDK shared libraries
    mkdir -p $out/lib
    for so in $src/lib/${arch}/*.so; do
      [ -f "$so" ] && install -Dm755 "$so" $out/lib/$(basename "$so")
    done
    if [ -d "$src/lib/${arch}/viture" ]; then
      for so in $src/lib/${arch}/viture/*.so*; do
        [ -f "$so" ] && install -Dm755 "$so" $out/lib/$(basename "$so")
      done
    fi

    # Install hidapi shared libs built during CMake
    find . -name 'libhidapi*.so*' \( -type f -o -type l \) | while read -r f; do
      cp -a "$f" $out/lib/
    done

    patchelf --set-rpath "$out/lib:${
      lib.makeLibraryPath [
        systemd
        stdenv.cc.cc.lib
        curl
        openssl
        json_c
        libusb1
        libevdev
        wayland
      ]
    }" $out/bin/xrDriver

    # CLI tool
    install -Dm755 $src/bin/xr_driver_cli $out/bin/xr_driver_cli
    wrapProgram $out/bin/xr_driver_cli \
      --prefix PATH : ${
        lib.makeBinPath [
          jq
          curl
        ]
      }

    # Udev rules
    mkdir -p $out/lib/udev/rules.d
    cp $src/udev/*.rules $out/lib/udev/rules.d

    # Systemd user service
    install -Dm644 $src/systemd/xr-driver.service $out/lib/systemd/user/xr-driver.service
    substituteInPlace $out/lib/systemd/user/xr-driver.service \
      --replace-fail '{ld_library_path}' "$out/lib" \
      --replace-fail '{bin_dir}' "$out/bin"

    runHook postInstall
  '';

  doInstallCheck = false;
  # The default release is a script which will do an impure download
  # just ensure that the application can run without network

  meta = {
    homepage = "https://github.com/wheaney/XRLinuxDriver";
    license = lib.licenses.mit;
    description = "Linux service for interacting with XR devices.";
    mainProgram = "xrDriver";
    maintainers = with lib.maintainers; [ shymega ];
    platforms = lib.platforms.linux;
  };
})
