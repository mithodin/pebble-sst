{
  description = "Pebble watchapp + for SimpleTimeTracker";

  inputs = {
    pebble.url = "github:pebble-dev/pebble.nix";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { pebble, flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        devShells.default = pebble.pebbleEnv.${system} {
          packages = with pkgs; [
            gradle
            git
          ];
        };
      });
}
