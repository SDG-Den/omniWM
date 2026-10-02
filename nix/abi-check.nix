# The ABI header check.
#
# This is the build target phase 01 exists to make runnable, and it is
# deliberately the smallest one that can be real. `include/shared/omni_layout.h`
# is the only file in the tree with content, and it is the only part of the design
# that is verified rather than asserted: every `OMNI_STATIC_ASSERT` in it holds a
# constant `configstorelayout.md` section 2 states in prose, so a clean compile is
# the header and the document agreeing with each other.
#
# The flags are the ones `devnotes/build.md` section 1 records, and the reason
# each is there is in that section. The two that are not obvious are
# `-fsyntax-only` and `-x c`: the target is a header rather than a translation
# unit, and without `-x c` a `.h` file is taken as a C++ header, which is a
# different standard with a different set of warnings, so the check would not be
# the check it claims to be.
#
# A zero exit with no output is a pass over every `OMNI_STATIC_ASSERT` in the
# file, because those are the header's only executable content. As of 2026-09-27
# there are 79 of them and `OMNI_CAP_DEFAULT` is `0x1FF`.
#
# The derivation installs a script as well as failing, so that `nix run` has
# something to run before the compositor exists. The script performs the same
# compile as the build did, which is the one property a check script can have and
# still be worth having: it cannot pass at run time and fail in CI, because it is
# the same compiler and the same flags rather than a re-implementation of them.

{
  lib,
  stdenv,
  buildPackages,
  makeWrapper,
}:

stdenv.mkDerivation {
  pname = "omniwm-abi-check";
  version = "nightly";

  src = builtins.path {
    path = ../.;
    name = "source";
  };

  nativeBuildInputs = [
    buildPackages.gcc
    makeWrapper
  ];

  dontConfigure = true;
  dontFixup = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin

    # -Iinclude is the project include root. The header needs nothing from the
    # tree today, since it includes only <stdint.h>, and the flag is here so the
    # command does not have to change when a header starts including a project
    # header.
    cat > $out/bin/omniwm-abi-check <<'EOF'
    #!/bin/sh
    set -eu
    gcc -std=c11 -Wall -Wextra -Iinclude -fsyntax-only -x c \
      include/shared/omni_layout.h
    echo "omniwm ABI header: all OMNI_STATIC_ASSERTs pass"
    EOF

    chmod +x $out/bin/omniwm-abi-check

    # The compile runs here rather than only at run time, so that a broken header
    # fails the build and not the first person to run the script.
    gcc -std=c11 -Wall -Wextra -Iinclude -fsyntax-only -x c \
      include/shared/omni_layout.h

    wrapProgram $out/bin/omniwm-abi-check \
      --prefix PATH : ${lib.makeBinPath [ buildPackages.gcc ]}

    runHook postInstall
  '';

  meta = {
    mainProgram = "omniwm-abi-check";
    description = "Verifies the static asserts in include/shared/omni_layout.h";
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
  };
}
