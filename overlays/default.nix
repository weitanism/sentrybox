{ config, pkgs, lib, ... }:

{
  nixpkgs.overlays = [
    (self: super: (import ../pkgs { pkgs = super; }))
  ];
}
