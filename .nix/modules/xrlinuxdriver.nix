{ self }:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.services.xrlinuxdriver;
  package = self.packages.${pkgs.stdenv.hostPlatform.system}.xrlinuxdriver;
in
{
  options.services.xrlinuxdriver = {
    enable = lib.mkEnableOption "XRLinuxDriver service for XR glasses";
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ package ];
    systemd.packages = [ package ];
    services.udev.packages = [ package ];
    systemd.user.services.xr-driver.wantedBy = [ "default.target" ];
  };
}
