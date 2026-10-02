{
  description = "omniWM: an API-driven Wayland compositor with a shared-memory config store";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  # `02-substrate.md` B5 makes this a stated requirement rather than a
  # preference: a build must be reproducible from a clean machine, and a machine
  # with no `~/.config/nix/nix.conf` gets the default substituter anyway, but one
  # with a local or unavailable substituter configured gets a build that fails or
  # silently avoids the cache. Naming it here means a fresh checkout builds the
  # same way on any machine.
  nixConfig = {
    extra-substituters = [ "https://cache.nixos.org" ];
  };

  outputs =
    {
      self,
      flake-parts,
      ...
    }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
      ];

      perSystem =
        {
          pkgs,
          ...
        }:
        let
          inherit (pkgs) callPackage;

          # The four packages. `nix/default.nix` returns all of them together
          # because the dependency edges between them are the part that is easy
          # to get wrong and hard to see when they are in four files.
          omniPackages = callPackage ./nix { };

          omniwm = omniPackages.omniwm;

          # The ABI header check. This is the one build target that can succeed
          # today, because `include/shared/omni_layout.h` is the only file in the
          # tree with content. It is also the check worth having on every commit,
          # since a wrong byte-level constant is a build failure rather than a
          # disagreement between a header and a document.
          abi-check = callPackage ./nix/abi-check.nix { };

          # Extensions to the derivation. Phase 02's B1 decides the final set;
          # these are the switches the dev shell needs today and have no effect on
          # `packages.default` unless a derivation reads them.
          devShellOverride = old: {
            nativeBuildInputs = old.nativeBuildInputs ++ (with pkgs; [
              gdb
              valgrind
              clang-tools
            ]);
          };
        in
        {
          # The compositor. Its `omniwm` executable is disabled while the source
          # tree is empty scaffold, so what it installs today is the ABI header.
          # See `nix/default.nix` and the build-status comment in `meson.build`.
          #
          # The three libraries are built and named individually because they are
          # the packages a consumer depends on. `libomniwm` is the one the design
          # wants reachable: `ipc.md` §7.1 says a well-written library maps the
          # block directly, and this is what it maps it with.
          packages = {
            inherit omniwm;
            inherit (omniPackages) libomniwm libomniwm-ipc libomniwm-debugger omniwm-debug;
            abi-check = abi-check;
          };

          packages.default = omniwm;

          # The overlay, so `pkgs.omniwm` and `pkgs.libomniwm` resolve for anything
          # using this flake as an input. It is here rather than as a flake input
          # because the point is that a consumer gets the same derivation we build.
          overlayAttrs = final: prev: {
            inherit (omniPackages) libomniwm libomniwm-ipc libomniwm-debugger;
            inherit omniwm;
          };

          # `nix flake check` runs this. Mango has no `checks` block; this is new,
          # and it exists because the ABI header is the one part of the design
          # that can be verified mechanically rather than argued about.
          checks.default = abi-check;

          # `nix run` prefers `apps` over `packages`, so this is what a bare
          # `nix run` does. It is the ABI check rather than the compositor because
          # there is no compositor executable yet, and a `nix run` that fails with
          # a missing file teaches nothing. This line is deleted when
          # `02-substrate.md` B3 installs the real binary.
          apps.default = {
            type = "app";
            program = "${abi-check}/bin/omniwm-abi-check";
            meta.description = "Compile omniWM's ABI header and check every static assert";
          };

          devShells.default = omniwm.overrideAttrs devShellOverride;
          formatter = pkgs.nixfmt;
        };
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
    };
}
