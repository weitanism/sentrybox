{ buildNpmPackage, ... }:

buildNpmPackage {
  pname = "sentrybox-ui";
  version = "0.0.1";

  src = ./.;

  npmDepsHash = "sha256-ZuMuAkW2bKMIl2bsyvgXqU9PBAxtQV9XPRDZ/XjkLQQ=";
}
