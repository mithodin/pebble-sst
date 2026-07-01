{
  description = "Pebble watchapp + for SimpleTimeTracker";

  inputs = {
    pebble.url = "github:pebble-dev/pebble.nix";
    flake-utils.url = "github:numtide/flake-utils";
    simpletimetracker = {
      url = "github:mithodin/Android-SimpleTimeTracker/feature/pebble-integration";
      flake = false;
    };
    unity = {
      url = "github:ThrowTheSwitch/Unity";
      flake = false;
    };
  };

  outputs =
    {
      pebble,
      flake-utils,
      nixpkgs,
      unity,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        devShells.default = pebble.pebbleEnv.${system} {
          packages = with pkgs; [
            gcc
            gnumake
          ];
        };

        apps.test = {
          type = "app";
          program = "${pkgs.writeShellScript "pepple-sst-test-runner" ''
            export PATH=${pkgs.gcc}/bin:${pkgs.gnumake}/bin:$PATH
            export UNITY_PATH=${unity.outPath}
            make -C tests test
          ''}";
        };
      });
}
