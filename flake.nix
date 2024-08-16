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

      packages.x86_64-linux =
        nixpkgs.legacyPackages.x86_64-linux.callPackage ./pkgs { };

      devShell.x86_64-linux =
        let
          pkgs = nixpkgs.legacyPackages.x86_64-linux;
        in
        pkgs.mkShell {
          inputsFrom = [ packages.x86_64-linux.fat32 ];
          packages = with pkgs; [
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
