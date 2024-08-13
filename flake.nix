{
  description = "Flake for building a Raspberry Pi Zero 2 W SD image";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/24.05";
  };

  outputs =
    { self
    , nixpkgs
    ,
    }:
    let
      username = "user";
      hostname = "sentrybox";
      specialArgs = {
        inherit username hostname;
      };
    in
    rec {
      nixosConfigurations = {
        zero2w = nixpkgs.lib.nixosSystem {
          inherit specialArgs;
          modules = [
            "${nixpkgs}/nixos/modules/installer/sd-card/sd-image-aarch64.nix"
            ./zero2w.nix
            (import ./overlays)
          ];
        };
      };

      images = {
        zero2w = nixosConfigurations.zero2w.config.system.build.sdImage;
      };

      devShell.x86_64-linux =
        let
          pkgs = nixpkgs.legacyPackages.x86_64-linux;
        in
        pkgs.mkShell {
          packages = with pkgs; [
            # C++
            cmake
            llvmPackages_12.clang
            clang-tools
            abseil-cpp
            argparse

            # Python
            nodePackages.pyright
            (python3.withPackages
              (p: with p; [
                # devtools
                black
                isort
                epc
              ]))
          ];
        };
    };
}
