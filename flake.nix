{
  description = "Nix Flake for XRLinuxDriver";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };
  outputs = {
    self,
    nixpkgs,
    flake-utils,
    ...
  }: let
    systems = [
      "x86_64-linux"
      "aarch64-linux"
    ];
  in
    flake-utils.lib.eachSystem systems
    (
      system: let
        pkgs = import nixpkgs {
          inherit system;
        };
      in {
        packages = {
          xrlinuxdriver = pkgs.callPackage ./nix {inherit self;};
          default = self.packages.${pkgs.system}.xrlinuxdriver;
        };

        devShells.default = pkgs.mkShell {inputsFrom = with self.packages.${pkgs.system}; [default];};
      }
    )
    // {
      overlays.default = final: prev: {inherit (self.packages.${final.system}) xrlinuxdriver;};
    };
}
