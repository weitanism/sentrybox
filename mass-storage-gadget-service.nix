{ sizeGb ? 4
, path ? "/mass-storage.bin"
, mountPath ? "/mnt/mass-storage"
, webUiPort ? 8000
, staticFileServerPort ? 8001
, pkgs
, ...
}:

{
  systemd.services.mass-storage-gadget =
    let
      size = builtins.toString sizeGb;
      gadget-command = "${pkgs.mass-storage-gadget}/bin/mass-storage-gadget -f ${path} -s ${size} -m ${mountPath}";
    in
    {
      wantedBy = [ "multi-user.target" ];
      after = [ "local-fs.target" ];
      description = "Mass storage gadget service.";
      serviceConfig = {
        Type = "exec";
        User = "root";
        Group = "root";
        ExecStart = "${gadget-command} host-mode";
        ExecStopPost = "${gadget-command} disable";
        Restart = "on-failure";
        RestartSec = 2;
      };
      path = with pkgs; [
        util-linux
        kmod
        dosfstools
        fat32
        simple-http-server
      ];
    };

  systemd.services.mass-storage-gadget-http =
    {
      wantedBy = [ "multi-user.target" ];
      after = [ "mass-storage-gadget.service" ];
      requires = [ "mass-storage-gadget.service" ];
      description = "Mass storage gadget http service.";
      serviceConfig = {
        Type = "exec";
        User = "root";
        Group = "root";
        ExecStart = "${pkgs.simple-http-server}/bin/simple-http-server --port ${builtins.toString staticFileServerPort} --cors ${mountPath}";
        Restart = "on-failure";
        RestartSec = 2;
      };
    };

  systemd.services.sentrybox-ui =
    {
      wantedBy = [ "multi-user.target" ];
      after = [ "mass-storage-gadget.service" ];
      requires = [ "mass-storage-gadget.service" ];
      description = "Sentrybox web ui service.";
      serviceConfig = {
        Type = "exec";
        User = "root";
        Group = "root";
        Environment = "PORT=${builtins.toString webUiPort}";
        ExecStart = "${pkgs.sentrybox-ui}/bin/sentrybox-ui";
        Restart = "on-failure";
        RestartSec = 2;
      };
      path = with pkgs; [
        ffmpeg-headless
      ];
    };
}
