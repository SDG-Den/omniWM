# Shared pieces of the four omniWM derivations.
#
# The build is four packages, and this is the part they agree on: the same
# source, the same Meson flags, the same Mango dependency base, and the ABI
# header installed by each. Anything a consumer of one of them might need to
# compile against the library belongs here rather than in one derivation, because
# a header that only the library package ships is a header a dependent cannot
# find.
#
# The dependency list is Mango's, unchanged, and the reason it is written once
# here rather than four times is that four copies of a pin list is four things to
# keep in agreement. `02-substrate.md` B1 owns changing it, and changing it here
# changes all four at once.
{
  lib,
  libdrm,
  libGL,
  libinput,
  libxcb,
  libxcb-wm,
  libxkbcommon,
  pango,
  cjson,
  pcre2,
  pixman,
  pkg-config,
  scenefx,
  stdenv,
  wayland,
  wayland-protocols,
  wayland-scanner,
  wlroots_0_20,
  xwayland,
  meson,
  ninja,
  enableXWayland ? true,
  debug ? false,
  # Named `pname` rather than `name` because nixpkgs' `callPackage` reserves the
  # latter and refuses to fill it from the caller's attrset. A derivation argument
  # called `name` is a confusing failure to debug for a one-word fix.
  pname,
  # The ninja target this package builds, and the Meson install tag it installs.
  # One Meson project defines all four targets; each derivation names one of each,
  # so the packages stay separate without the work being done four times.
  buildTarget ? null,
  installTag,
}:

let
  source = builtins.path {
    path = ../.;
    name = "source";
  };
in
stdenv.mkDerivation {
  inherit pname;
  version = "nightly";

  inherit source;

  src = source;

  mesonFlags = [
    (lib.mesonEnable "xwayland" enableXWayland)
    (lib.mesonBool "asan" debug)
  ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    wayland-scanner
  ];

  # stdenv's Meson hook `cd`s into the build directory before `buildPhase` runs, so
  # `build.ninja` is in the current directory and takes no `-C`. nixpkgs'
  # `buildFlags` is not the answer to the same problem: it is consumed for
  # parallel-build flags, and the log shows the build running all 105 steps
  # regardless of it. So the phase is written out rather than configured through an
  # attribute whose name suggests it selects targets.
  #
  # This is also what makes the four packages cost one build between them rather
  # than four.
  buildPhase = ''
    runHook preBuild

    ninja ${lib.optionalString (buildTarget != null) buildTarget}

    runHook postBuild
  '';

  # `--tags` is what keeps the packages separate, and both flags are load-bearing:
  # without `--tags` Meson installs every target in the plan, so a package built
  # for one library ends up containing all three; without `--no-rebuild` Meson
  # re-runs the default target first and undoes the `buildTarget` above. Meson has
  # no per-target install selection other than tags, which is why `meson.build`
  # sets `install_tag` on each target.
  #
  # No `--destdir`: the configure phase already set `--prefix` to `$out`, so
  # `meson install` writes there directly.
  installPhase = ''
    runHook preInstall

    meson install --no-rebuild --tags ${installTag}

    # Every one of the four installs the ABI header, not just the library. The
    # header is the project's stable interface — `ipc.md` §7.1 and
    # `configstorage.md` §0 both rest on the block being the interface — so a
    # package that ships a library without shipping the header would be a package
    # whose consumer cannot use what it just linked.
    #
    # Copied by absolute store path, because the phase runs in the build
    # directory rather than in the source root and the header is not installed by
    # any Meson target.
    mkdir -p $out/include/shared
    cp ${source}/include/shared/omni_layout.h $out/include/shared/

    runHook postInstall
  '';

  buildInputs = [
    libinput
    libxcb
    libxkbcommon
    pcre2
    pango
    cjson
    pixman
    wayland
    wayland-protocols
    wlroots_0_20
    scenefx
    libGL
    libdrm
  ] ++ lib.optionals enableXWayland [
    libxcb-wm
    xwayland
  ];

  meta = {
    homepage = "https://github.com/omniwm/omniwm";
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.unix;
  };
}
