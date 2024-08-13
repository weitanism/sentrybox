{ lib
, llvmPackages_12
, cmake
, clang
, clang-tools
, argparse
, abseil-cpp
}:

llvmPackages_12.stdenv.mkDerivation rec {
  pname = "fat32";
  version = "0.0.1";

  src = ./.;

  nativeBuildInputs = [
    cmake
    clang
    clang-tools
  ];
  buildInputs = [
    abseil-cpp
    argparse
  ];

  cmakeFlags = [
    "-DENABLE_TESTING=OFF"
    "-DENABLE_INSTALL=ON"
    "-DCMAKE_BUILD_TYPE=Release"
  ];

  meta = with lib; {
    homepage = "https://github.com/nixvital/nix-based-cpp-starterkit";
    description = ''
      A template for Nix based C++ project setup.";
    '';
    licencse = licenses.mit;
    platforms = with platforms; linux;
  };
}
