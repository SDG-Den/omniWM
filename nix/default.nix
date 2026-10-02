# The four omniWM packages.
#
# They share one source tree, one Meson project, and one dependency base
# (`common.nix`), and they differ in which target they build and what they depend
# on. Keeping them in one file rather than four is deliberate: the dependency
# edges between them are the thing most likely to be got wrong, and one file puts
# the four packages and the three edges in one place to be read together.
#
#   libomniwm            the compositor's logic. Exists to be depended on.
#   libomniwm-ipc        the socket facade. The WM links it; a client does not.
#   libomniwm-debugger   the block visualiser. Nothing links it into the WM.
#   omniwm               the compositor. Links the IPC at runtime, never the
#                        debugger.
#
# The debugger is absent from `omniwm` on purpose rather than by omission. A
# debugger linked into a shipping compositor puts a read-everything surface in the
# release binary for no runtime reason, and `ipc.md` §7.1 is explicit that a
# facade earns its existence by offering something the direct route does not,
# which is an argument about what a *user* can reach and not about what a build
# should carry. So the release package does not depend on it and the debug
# package does.
#
# `common` is a function that calls `callPackage` afresh each time rather than a
# partially-applied `callPackage`. nixpkgs validates a called function's required
# arguments at the `callPackage` site, so `callPackage ./common.nix { }` followed
# by a later call supplying `pname` fails before that later call is reached.

{ lib, callPackage, enableXWayland ? true, debug ? false }:

let
  base = {
    inherit enableXWayland debug;
  };

  common = arguments: callPackage ./common.nix (base // arguments);

  # Gives a derivation a description and the libraries it links.
  # `propagatedBuildInputs` is the right attribute rather than `buildInputs`
  # because a shared object that links another one needs that other one present
  # both at link time and in the consumer's runtime closure.
  withDescription = derivation: description:
    derivation.overrideAttrs (old: {
      meta = (old.meta or { }) // { inherit description; };
    });

  withLibraries = derivation: libraries:
    derivation.overrideAttrs (old: {
      propagatedBuildInputs = (old.propagatedBuildInputs or [ ]) ++ libraries;
    });

  # The compositor's logic, minus its entry point and minus the socket. This is
  # the package the design wants reachable: `ipc.md` §7.2 says a well-written
  # omniWM library maps the block directly rather than touching the socket, and
  # this is what such a library maps it with.
  libomniwm = withDescription
    (common {
      pname = "libomniwm";
      buildTarget = "libomniwm.so";
      installTag = "core";
    })
    "Core library of the omniWM compositor, for driving a config store directly";

  # The socket facade. `ipc.md` §1 serves the socket from the WM event loop, so
  # this is server code that runs inside the compositor process and cannot be a
  # separate program. It is a separate *library* for a different reason: it is the
  # one part of the compositor that other programs talk to, and giving it its own
  # object means a consumer can link the protocol without linking a compositor.
  libomniwm-ipc = withLibraries
    (withDescription
      (common {
        pname = "libomniwm-ipc";
        buildTarget = "libomniwm-ipc.so";
        installTag = "ipc";
      })
      "Socket facade for the omniWM config store, per ipc.md")
    [ libomniwm ];

  # The block visualiser. Links the core library to read the block, and is linked
  # by nothing.
  libomniwm-debugger = withLibraries
    (withDescription
      (common {
        pname = "libomniwm-debugger";
        buildTarget = "libomniwm-debugger.so";
        installTag = "debugger";
      })
      "Visual analyser for the omniWM shared-memory config block")
    [ libomniwm ];
in
rec {
  inherit libomniwm libomniwm-ipc libomniwm-debugger;

  # A function of `debug` rather than a single value, so that `omniwm-debug` is
  # the same derivation with a different flag rather than a second copy of the
  # build definition that can drift from the first.
  makeOmniwm = debug: withDescription
    (
      withLibraries
        (common {
          pname = "omniwm";
          inherit debug;
          # `libomniwm-ipc.so` and not `omniwm`. The `omniwm` executable is
          # commented out in `meson.build` while `src/main.c` is zero bytes, and
          # naming a target that does not exist fails the build with "unknown
          # target" rather than with anything a reader can act on.
          #
          # Building the socket facade is the closest honest stand-in: it pulls in
          # `libomniwm.so` as a link dependency, so this package contains the
          # compositor's logic and its socket and nothing else — which is exactly
          # the closure a release compositor should have. When the executable is
          # enabled this becomes `buildTarget = "omniwm"`.
          buildTarget = "libomniwm-ipc.so";
          # Only the socket. `libomniwm.so` reaches this package's closure
          # through `propagatedBuildInputs` and onto its RPATH, so installing it
          # here as well would put the same file in two store paths for no gain.
          installTag = "ipc";
        })
        ([ libomniwm-ipc ] ++ lib.optionals debug [ libomniwm-debugger ])
    )
    "API-driven Wayland compositor with a shared-memory config store";

  # The release build. Does not depend on the debugger.
  omniwm = makeOmniwm false;

  # The debug build. Carries the debugger in its runtime closure and compiles with
  # ASan. Exposed as a separate package rather than as a flag on the default, so
  # that "the release build does not carry the debugger" is a property of what
  # `packages.default` *is* rather than a claim about how it was built.
  omniwm-debug = makeOmniwm true;
}
