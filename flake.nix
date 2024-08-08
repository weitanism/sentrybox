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
          ];
        };
      };

      images = {
        zero2w = nixosConfigurations.zero2w.config.system.build.sdImage;
      };
    };
}
