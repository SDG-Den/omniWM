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

Stated here only so the gap is on the record, not as a plan. Phase 02's build
half decides the dependency set and pins the versions; `missing-devnotes-topics.md`
marks the row P0 and wants a `flake.nix` with **`cache.nixos.org` named
explicitly as the substituter** rather than inherited from the ambient config, so
that a build is reproducible from a clean machine. The claim in §1 above is that
the ABI header compiles; it is not a claim that the project builds, and the
project does not build yet, because there is no build system in the tree.
