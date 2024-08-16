{ config, ... }:

let
  ip = "<headscale-server-ip>";
in
{
  services = {
    headscale = rec {
      enable = true;
      address = "0.0.0.0";
      port = 54321;
      settings = {
        server_url = "http://${ip}:${builtins.toString port}";
        logtail.enabled = false;
        randomize_client_port = true;
        ip_prefixes = [ "100.64.0.0/16" ];
        dns_config.magic_dns = false;
        dns_config.override_local_dns = false;
      };
    };
  };

  networking.firewall.allowedTCPPorts = [ config.services.headscale.port ];

  environment.systemPackages = [ config.services.headscale.package ];
}
