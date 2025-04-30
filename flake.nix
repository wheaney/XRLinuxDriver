{
  description = "Nix Flake for XRLinuxDriver";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-unstable";
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
    self.submodules = true;
  };
  outputs = inputs: let
    inherit (inputs) self nixpkgs;

    forEachSystem = let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      genPkgs = system: nixpkgs.legacyPackages.${system};
      inherit (nixpkgs.lib) genAttrs;
    in
      f: genAttrs systems (system: f (genPkgs system));
  in {
    packages = forEachSystem (pkgs: {
      xrlinuxdriver = pkgs.callPackage ./package.nix {inherit self;};
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.xrlinuxdriver;
    });

    devShells = forEachSystem (pkgs: {
      default = pkgs.mkShell {
        inputsFrom = pkgs.lib.singleton self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      };
    });

    overlays.default = _: prev: self.packages.${prev.stdenv.hostPlatform.system};
  };
}
