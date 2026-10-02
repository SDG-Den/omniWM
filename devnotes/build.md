# Build (devnotes/build.md)

This document is a stub, and the stub is deliberate. It exists now because one
fact has to be citable today: **how the C in this repository is compiled.** That
fact is not inferable from the tree, and three separate verification paragraphs in
`design-phases/00-reconciliation.md` had already gone stale for want of it.

The rest of the document is phase 02's build half
(`design-phases/02-substrate.md`, items **B1** to **B6**), which owns the
dependency set, the wlroots and scenefx version coupling, the exact scenefx
extensions needed for user GLSL shaders and 3D transforms, the solver's
arithmetic width, the Meson structure, and the `flake.nix` with
**`cache.nixos.org` set explicitly as the substituter**. None of that is written
ahead of the phase that decides it. §1 is the only section that exists until
then, and other documents should cite §1 rather than restate an invocation.

## 1. Compiling the ABI header

There is no compiler on `PATH` in this environment: no `gcc`, no `cc`, no
`clang`. The compiler is reached through Nix, and the invocation that works is:

```
nix run nixpkgs#gcc -- -std=c11 -Wall -Wextra -Iinclude -fsyntax-only -x c \
    include/shared/omni_layout.h
```

Run it from the repository root. As of 2026-09-27 it exits 0 with no output
against GCC 15.3.0. The gcc that command resolves to is already realised in the
Nix store, so the run is a store lookup: it fetches nothing and writes nothing to
the environment or to the working tree. The fact is recorded because the argument
this file makes about a criterion has a second half, and a check a reader has to
weigh before running it is a check that does not get run.

The flags after the `--` describe the task rather than the project's real build
flags, and each one is there for a reason:

- `-std=c11` — the header is C11.
- `-Wall -Wextra` — a clean compile means no output at all, not merely a zero exit.
- `-Iinclude` — the project include root. The header needs nothing from the tree
  today (it includes only `<stdint.h>`), so this flag is not currently load-bearing
  and is here so the command does not have to change when a header starts
  including a project header.
- `-fsyntax-only` — parse and check, do not emit. The target is a header and there
  is nothing to link.
- `-x c` — force the language. Without it a `.h` file is taken as a C++ header,
  which is a different standard with a different set of warnings, so the check
  would not be the check it claims to be.

A zero exit with no output is a pass over **every `OMNI_STATIC_ASSERT` in the
file**, because those are the header's only executable content. It is therefore
the check that the byte-level constants `configstorelayout.md` §2 is built on and
the `OMNI_STATIC_ASSERT`s in `include/shared/omni_layout.h` agree with each other,
which is the one property in the design that is verified rather than asserted.

This section is what `design-phases/00-reconciliation.md` should cite when it
claims the header compiles. Citing it rather than repeating the invocation is not
tidiness: the three copies the file carried had each drifted into a form that
could not be run at all, and a criterion that cannot be run as written reports a
pass that was never demonstrated.

## 2. What phase 02 adds

The gap was recorded here on 2026-09-27 and is now partly closed. There **is** a
build system in the tree: `flake.nix`, `meson.build`, `meson_options.txt`,
`protocols/` and `nix/`, with `nix build`, `nix run`, `nix flake check` and
`nix develop` all working. It was written in phase 01, because a test document
with no runner in the tree is a specification nothing checks.

What the build does **not** do is build a compositor. The `omniwm` executable is
disabled in `meson.build` while all 62 source files are zero bytes, so
`nix build` installs the ABI header and `nix run` runs the header check. The
section 1 invocation above is the same compile, wrapped so that `nix flake check`
performs it on every evaluation.

### 2.1 The five packages

`meson.build` defines four targets and the flake exposes five packages. The
split is not cosmetic, so it is worth stating what each one is for:

| Package | Target | Propagates | Notes |
| --- | --- | --- | --- |
| `libomniwm` | `libomniwm.so` | — | The **internal** development library omniWM itself is built from: it links wlroots, scenefx, and the whole Mango list, because the WM is built on them. Not a hand-off surface. |
| `libomniwm-ipc` | `libomniwm-ipc.so` | `libomniwm` | The socket facade, `ipc.md` §7.1. |
| `libomniwm-debugger` | `libomniwm-debugger.so` | `libomniwm` | Reads the block. Nothing links it into the compositor. |
| `omniwm` | `libomniwm-ipc.so` | `libomniwm-ipc` | The compositor once `src/main.c` has a `main`. |
| `omniwm-debug` | `libomniwm-ipc.so` | `libomniwm-ipc`, `libomniwm-debugger` | The above, with ASan and the debugger in the closure. |

A future external **SHM-client library** for other software is a separate
package, not one of these. Its only interface dependency is the block layout in
`include/shared/`, which is exactly why the ABI header is dependency-free
(`filestructure.md`); it will sit on top of the block and the socket, and it is
not `libomniwm` under a friendlier name.

The release/debug distinction is a closure property, which is the part that is
easy to get wrong and worth checking rather than asserting:

```
$ nix path-info -r .#omniwm       | rg -o 'omniwm[a-z-]*-nightly' | sort -u
omniwm-ipc-nightly
omniwm-nightly

$ nix path-info -r .#omniwm-debug | rg -o 'omniwm[a-z-]*-nightly' | sort -u
omniwm-debugger-nightly
omniwm-ipc-nightly
omniwm-nightly
```

A debugger that the release WM can reach is not a separate package, it is a
flag. Keeping the dependency edge in `propagatedBuildInputs` rather than in
`buildInputs` is what puts it in the closure of the debug build only.

Two things make this work and both are non-obvious enough to record:

- **`install_tag`, not `meson install` target selection.** `meson install`
  installs every target carrying `install : true`; there is no per-target
  selection other than tags. So `nix/common.nix` passes
  `--tags <tag> --no-rebuild` and `meson.build` sets `install_tag` on each
  target. Without the tags a package built for one library ends up containing
  all three, and the split is undone at the last step. `--no-rebuild` is
  load-bearing separately: without it Meson re-runs the default target and
  discards the `buildPhase`'s target selection.
- **Dependencies travel by propagation, not by copying.** `libomniwm-ipc`'s
  output contains only `libomniwm-ipc.so`; `libomniwm.so` reaches the consumer
  through `propagatedBuildInputs` and onto its RPATH. Copying it into two store
  paths would produce two files that are equal in every byte that matters and
  diverging in every byte that does not.

`libomniwm` builds and links cleanly today with every source in it empty. It has
no symbols. The link is real and the contents are not, which is the whole
reason the executable is disabled: an empty `main` does not link, and a build
that fails for a reason unrelated to the design teaches a reader to ignore
build failures.

`omniwm` currently builds `libomniwm-ipc.so` rather than an executable, so its
output is not yet a compositor and cannot be run. The `omniwm` target in
`meson.build` is commented out with the reason inline.

### 2.2 A cost of the current source expression

`nix/common.nix` sets `source = builtins.path { path = ../.; }`, which hashes the
whole repository — `devnotes/` included. The consequence is measurable: editing
this file invalidates all five packages, and `nix path-info -r .#omniwm` then
reports that three derivations will be built. It was observed while writing this
section, which is why it is written down.

This is not a bug, and nothing in the design depends on it. It is a cost worth
naming before someone rediscovers it as "why did editing a document start a
three-minute build". The fix is a filtered source that excludes `devnotes/`,
which is a small change to `nix/common.nix` and not a design decision; it has not
been made because it was not asked for.

The claims that were recorded here and still hold:

- The invocation in section 1 is the authority on the ABI header compiling. The
  derivation in `nix/abi-check.nix` duplicates those flags on purpose, and the
  duplication is the risk this paragraph exists to flag: a second copy of a
  command is a second thing to drift, so if the flags in section 1 change, both
  copies have to change.
- A clean build is not yet evidence of anything beyond the header. It is not a
  claim that the project builds, and it is not a claim that the test suite passes,
  because the test suite does not exist yet. `01-testing.md` owns the assertion
  set that will make it mean something.

`02-substrate.md` B5 is done: `nixConfig.extra-substituters` names
`https://cache.nixos.org`. B1, B2 and B6 have a Mango-shaped build to be decided
against, and B3 is explicitly still open.

B5 is worth a second sentence. The flake sets `nixConfig.extra-substituters`, and
Nix does not honour flake config from an untrusted tree, so every command prints
`ignoring untrusted flake configuration setting 'extra-substituters'` until it
is passed `--accept-flake-config`. The setting is the right one to have in the
flake and the wrong one to rely on; the flag is what makes it take effect, and
both are in use.
