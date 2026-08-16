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

  version =
    let
      versionFile = pkgs.runCommand "xrlinuxdriver-version" { } ''
        grep -oP '(?<=project\(xrDriver VERSION )[0-9]+\.[0-9]+\.[0-9]+' ${self}/CMakeLists.txt > $out
      '';
    in
    lib.strings.trim (builtins.readFile versionFile);
in
stdenv.mkDerivation (finalAttrs: {
  pname = "xrlinuxdriver";
  inherit version;

  src = lib.cleanSourceWith {
    src = self;
    name = "${finalAttrs.pname}-src";
  };

  cargoRoot = "modules/xrealOneDeviceKit/modules/xreal_one_driver";

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

    # Vendor SDK shared libraries. CMake auto-detects these at build time
    # by presence under lib/${arch}, so the install step mirrors that: it
    # only installs what actually exists for this arch, since not every
    # SDK ships blobs for every arch (e.g. RayNeo/Rokid are x86_64-only
    # upstream, VITURE ships for both x86_64 and aarch64).
    mkdir -p $out/lib

    # RayNeo XR Mini SDK
    if [ -f "$src/lib/${arch}/libRayNeoXRMiniSDK.so" ]; then
      install -Dm755 "$src/lib/${arch}/libRayNeoXRMiniSDK.so" $out/lib/libRayNeoXRMiniSDK.so
    fi

    # Rokid Glass SDK
    if [ -f "$src/lib/${arch}/libGlassSDK.so" ]; then
      install -Dm755 "$src/lib/${arch}/libGlassSDK.so" $out/lib/libGlassSDK.so
    fi

    # VITURE SDK, plus its bundled OpenCV/libcarina_vio dependencies.
    # These are versioned shared objects with .so -> .so.X.Y symlink
    # chains, so copy with -d to preserve links instead of flattening
    # them into duplicate files via `install`.
    if [ -d "$src/lib/${arch}/viture" ]; then
      cp -dR "$src/lib/${arch}/viture/"*.so* $out/lib/
      find $out/lib -maxdepth 1 -type f -exec chmod 755 {} +
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
