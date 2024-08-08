{ lib
, pkgs
, username
, hostname
, ...
}:

{
  imports = [
    ./sd-image.nix
  ];

  nixpkgs.hostPlatform.system = "aarch64-linux";
  # Uncomment this line to cross-compile or Nix would use QEMU.
  # nixpkgs.buildPlatform.system = "x86_64-linux";
  nix.settings.trusted-users = [ "@wheel" ];
  system.stateVersion = "24.05";

  sdImage = {
    # bzip2 compression takes loads of time with emulation, skip
    # it. Enable this if you're low on space.
    compressImage = false;
    imageName = "zero2w.img";

    extraFirmwareConfig = {
      # Give up VRAM for more free system memory
      # - Disable camera which automatically reserves 128MB VRAM
      start_x = 0;
      # - Reduce allocation of VRAM to 16MB minimum for non-rotated(32MB
      # for rotated)
      gpu_mem = 16;

      # Configure display to 800x600 so it fits on most screens
      # * See: https://elinux.org/RPi_Configuration
      hdmi_group = 2;
      hdmi_mode = 8;
    };
  };

  # Keep this to make sure wifi works
  hardware.enableRedistributableFirmware = lib.mkForce false;
  hardware.firmware = [ pkgs.raspberrypiWirelessFirmware ];

  boot = {
    # Fix zfs-kernel
    # issue. https://github.com/NixOS/nixpkgs/issues/216886
    supportedFilesystems.zfs = lib.mkForce false;

    kernelPackages = pkgs.linuxPackages_latest;

    initrd.availableKernelModules = [ "xhci_pci" "usbhid" "usb_storage" ];
  };

  fileSystems = {
    "/" = {
      options = [ "noatime" "nodiratime" ];
    };
  };

  networking = {
    hostName = hostname;
    interfaces."wlan0".useDHCP = true;
    wireless = {
      enable = true;
      interfaces = [ "wlan0" ];
      networks = import ./config/wifi.nix;
    };
  };

  services.journald.extraConfig = "Storage=volatile";
  services.openssh.enable = true;

  services.zram-generator = {
    enable = true;
    settings.zram0 = {
      compression-algorithm = "zstd";
      zram-size = "ram * 2";
    };
  };

  users.users.${username} = {
    isNormalUser = true;
    home = "/home/${username}";
    extraGroups = [ "wheel" "networkmanager" ];
    openssh.authorizedKeys.keys = import ./config/ssh-keys.nix;
  };

  security.sudo = {
    enable = true;
    wheelNeedsPassword = false;
  };
}
