{
  description = "biosensor-embedded";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
  };

  outputs =
    { self, nixpkgs }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in
    {
      devShells.x86_64-linux.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          gcc
          glibc
          arduino-cli
        ];
      };

      packages.x86_64-linux.default = pkgs.stdenv.mkDerivation {
        name = "biosensor-embedded";
        src = self;
        buildPhase = "";
      };
    };
}
