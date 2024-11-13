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
}:
let
  pythonEnv = python3.withPackages (ps: [ ps.pyyaml ]);
  commonBuildInputs = [
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
    if pkgs.system == "aarch64-linux" then
      "aarch64"
    else if pkgs.system == "x86_64-linux" then
      "x86_64"
    else
      throw "Unsupported system ${pkgs.system}";
in
stdenv.mkDerivation rec {
  pname = "xrlinuxdriver";
  version = "0.12.0.1";

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
  sourceRoot = ".";
  postUnpack =
    let
      nrealAirLinuxDriver = (builtins.elemAt srcs 0).name;
      xrlinuxdriver = (builtins.elemAt srcs 1).name;
    in
    ''
      cp -R ${xrlinuxdriver}/ $sourceRoot/xrlinuxdriver
      cp -R ${nrealAirLinuxDriver}/* $sourceRoot/xrlinuxdriver/modules/xrealInterfaceLibrary
      chmod -R u+w $sourceRoot/xrlinuxdriver
    '';

  nativeBuildInputs = [
    cmake
    pkg-config
    pythonEnv
    autoPatchelfHook
  ];
  buildInputs = commonBuildInputs;

  cmakeFlags = [
    "-DCMAKE_SKIP_RPATH=ON"
    "-DXRLINUXDRIVER_ALLOW_SUBMODULES=OFF"
  ];
  cmakeBuildDir = "xrlinuxdriver/build";
  cmakeBuildType = "RelWithDebInfo";

  postPatch =
    let
      root = "$sourceRoot/xrlinuxdriver";
    in
    ''
      substituteInPlace ${root}/systemd/xr-driver.service \
        --replace-fail "ExecStart={bin_dir}/${meta.mainProgram}" "ExecStart=$out/bin/${meta.mainProgram}" \
        --replace-fail "{ld_library_path}" "$out/usr/lib/${arch}"
    '';

  installPhase =
    let
      root = "$sourceRoot/xrlinuxdriver";
    in
    ''
      mkdir -p $out/bin $out/lib/systemd/user $out/usr/lib/udev/rules.d $out/usr/lib/${arch}
      cp -R ${meta.mainProgram} ../bin/xr_driver_cli ../bin/xr_driver_verify $out/bin
      cp -R ../udev/* $out/usr/lib/udev/rules.d/
      cp -R ../lib/${arch}/* $out/usr/lib/${arch}/
      cp ${hidapi}/lib/libhidapi-hidraw.so.0 $out/usr/lib/
      cp -R ../systemd/* $out/lib/systemd/user/
    '';

  preBuild = ''
    addAutoPatchelfSearchPath $out/usr/lib/${arch}
  '';

  doInstallCheck = false;
  # The default release is a script which will do an impure download
  # just ensure that the application can run without network

  meta = with lib; {
    homepage = "https://github.com/wheaney/XRLinuxDriver";
    license = licenses.mit;
    description = "Linux service for interacting with XR devices.";
    mainProgram = "xrDriver";
    maintainers = with maintainers; [ shymega ];
    platforms = platforms.linux;
  };
}
