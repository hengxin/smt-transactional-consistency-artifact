{
  outputs = { nixpkgs, ... }:
  let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
    # mkShell = pkgs.mkShell.override {
    #   stdenv = pkgs.llvmPackages_latest.libcxxStdenv;
    # };
    mkShell = pkgs.mkShell;
  in
  {
    devShell.x86_64-linux = mkShell {
      hardeningDisable = [ "fortify" ];

      packages = with pkgs; [
        meson
        ninja
        clang-tools
        cmake
        boost.dev
        argparse
      ];
    };
  };
}
