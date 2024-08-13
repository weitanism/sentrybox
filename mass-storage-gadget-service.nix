{ sizeGb ? 4
, path ? "/mass-storage.bin"
, mountPath ? "/mnt/mass-storage"
, pkgs
, ...
}:

{
  systemd.services.mass-storage-gadget-service =
    let
      sizeStr = builtins.toString sizeGb;
      gadget = "${pkgs.mass-storage-gadget}/bin/mass-storage-gadget";
    in
    {
      wantedBy = [ "multi-user.target" ];
      after = [ "local-fs.target" ];
      description = "mass storage gadget service.";
      serviceConfig = {
        User = "root";
        Group = "root";
        ExecStart = "${gadget} -f ${path} -s ${sizeStr} -m ${mountPath} host-mode";
        ExecStop = "${gadget} -f ${path} -s ${sizeStr} -m ${mountPath} disable";
        RemainAfterExit = "yes";
      };
      path = with pkgs; [
        util-linux
        kmod
        dosfstools
      ];
    };
}
