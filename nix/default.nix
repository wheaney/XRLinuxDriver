{
  self,
  lib,
  pkgs,
  stdenv,
  fetchFromGitLab,
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
  autoPatchelfHook,
  ...
}: let
  pythonEnv = python3.withPackages (ps: [ps.pyyaml]);
  buildInputs = [
    curl
    hidapi
    json_c
    libevdev
    libffi
    libusb1
    openssl
    stdenv.cc.cc.lib
    wayland
  ];
  arch =
    if pkgs.system == "aarch64-linux"
    then "aarch64"
    else if pkgs.system == "x86_64-linux"
    then "x86_64"
    else throw "Unsupported system ${pkgs.system}";
in
  stdenv.mkDerivation rec {
    pname = "xrlinuxdriver";
    version = "unstable";

    srcs = [
      (fetchFromGitLab rec {
        domain = "gitlab.com";
        owner = "TheJackiMonster";
        repo = "nrealAirLinuxDriver";
        rev = "3225fcc575e19a8407d5019903567cff1c3ed1a8";
        hash = "sha256-NRbcANt/CqREQZoYIYtTGVbvkZ7uo2Tm90s6prlsrQE=";
        fetchSubmodules = true;
        name = "${repo}-src";
      })
      (lib.cleanSourceWith {
        src = self;
        name = "${pname}-src";
      })
    ];
    sourceRoot = "${(builtins.elemAt srcs 1).name}";

    postUnpack = let
      nrealAirLinuxDriver = (builtins.elemAt srcs 0).name;
    in ''
      mkdir -p $sourceRoot/modules/xrealInterfaceLibrary
      cp -R ${nrealAirLinuxDriver}/* $sourceRoot/modules/xrealInterfaceLibrary
      chmod -R u+w $sourceRoot
    '';

    nativeBuildInputs = [
      cmake
      pkg-config
      pythonEnv
      autoPatchelfHook
    ];
    inherit buildInputs;

    cmakeFlags = [
      "-DCMAKE_SKIP_RPATH=ON"
    ];
    cmakeBuildDir = "build";
    cmakeBuildType = "RelWithDebInfo";

    installPhase = ''
      mkdir -p $out/bin $out/lib/systemd/user $out/lib/udev/rules.d $out/lib/${arch}
      cp xrDriver ../bin/xr_driver_cli ../bin/xr_driver_verify $out/bin
      cp ../udev/* $out/lib/udev/rules.d/
      cp ../lib/${arch}/* $out/lib/${arch}/
      cp ../systemd/xr-driver.service $out/lib/systemd/user/xr-driver.service
      cp ${hidapi}/lib/libhidapi-hidraw.so.0 $out/lib/
      substituteInPlace \
        $out/lib/systemd/user/xr-driver.service \
        --replace-fail "ExecStart={bin_dir}/xrDriver" "ExecStart=$out/bin/xrDriver" \
        --replace-fail "{ld_library_path}" "$out/lib/${arch}"
    '';

    preBuild = ''
      addAutoPatchelfSearchPath $out/usr/lib/${arch}
    '';

    doInstallCheck = false;
    # The default release is a script which will do an impure download
    # just ensure that the application can run without network

    meta = {
      homepage = "https://github.com/wheaney/XRLinuxDriver";
      license = lib.licenses.mit;
      description = "Linux service for interacting with XR devices.";
      mainProgram = "xrDriver";
      maintainers = with lib.maintainers; [shymega];
      platforms = lib.platforms.linux;
    };
  }
