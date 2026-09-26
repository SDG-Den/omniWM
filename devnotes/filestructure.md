# File structure

This document owns directory boundaries and nothing else. It says what each
directory is responsible for and which design document is authoritative for its
contents. It does not assign work stages, map individual source files, or
specify module contents; those belong to the design document for the subsystem
in question.

## The rule

There are three kinds of directory here, and they have different rules.

- **ABI surface.** Header-only, no logic, no dependency on anything else. A
  program in any language maps the block using these definitions alone, so they
  must compile without the compositor, without wlroots, and without a C
  toolchain that knows anything about this project.
- **Compositor internals.** C sources and their headers. Nothing outside the
  compositor links against these.
- **Non-code.** Notes, documentation, and assets, which are not compiled and are
  not part of any build.

A directory that cannot be placed in exactly one of those three categories does
not belong in the tree yet.

## assets

Config files, test harnesses, example libraries, and interpreters. Not compiled
and not part of the build. Anything here may be written in any language, which
is the point of the store being the stable interface (`helpers.md` §8.1).

## devnotes

Design and development notes. Each file is authoritative for one concern, and a
concern is documented in exactly one of them; other documents link rather than
restate. The authority map is `server.md` §8.

## docs

User-facing documentation. Not compiled.

## include

Public headers of the compositor internals, and nothing else. `main.h` and
`commonheaders.h` sit at the top level; `commonheaders.h` is the umbrella that
includes every public `core` header, and a component includes it and nothing
else (`helpers.md` §1).

### include/shared

The ABI surface, and the only header in the tree that is not compositor-internal.
`omni_layout.h` is the single definition point for every byte-level constant in
`configstorelayout.md` §2. No constant is duplicated anywhere else, and changing
a value must be a one-line edit here.

This header is deliberately dependency-free. It uses only compiler built-ins and
static assertions, because a client mapping the block in Python, Rust, or any
other language reads the numbers from this one file, and because a consumer that
had to link the compositor to learn an offset would defeat the purpose of a
shared block.

## src

Compositor internals. Mirrors `include` one-for-one: every `src/<dir>` has a
corresponding `include/<dir>`, and a file added to one is added to the other.

### src/core

The component substrate, per `helpers.md` §1: `registry`, `component`, `events`,
`actions`, `log`, `util`, and `server`. This is the ground layer. It owns the
lifecycle that every other directory's code is activated within, and it is the
only directory permitted to include `include/shared/omni_layout.h` for its own
internal use rather than for re-export.

`server.md` is authoritative for what `src/core/server.c` does. `helpers.md` is
authoritative for the rest of the substrate.

### src/config

The store and the built-in TOML parser. `configstorage.md` is authoritative for
the store's semantics and `tomlparser.md` for the parser's type binding. The
parser is a facade over the store, not a privileged writer (`tomlparser.md` §10),
so this directory holds no capability that `ipc` does not also have.

### The remaining src directories

One directory per compositor subsystem, each a component in the sense of
`helpers.md` §3 and each paired with its `include` counterpart: `input`, `windows`,
`tags`, `monitor`, `decorate`, `draw`, `animate`, and `ext-protocol`. Their
designs are not written, so no directory here has a documented contract yet, and
this document does not invent one.

`src/main.c` is the entry point and contains no logic beyond handing off to
`omni_boot()`.

## ipc

The socket facade, pairing `ipc.c` and `ipc.h`. `ipc.md` is authoritative.

This is a client of the store, at the same privilege level as the in-process
helpers and the built-in parser, which is why it sits outside `src`: it is the
surface a non-C program can reach. Its absence degrades the project to the block
alone rather than making it unusable (`ipc.md` §1.5).

## Where a new file goes

- A byte-level constant, or a struct that mirrors block layout: `include/shared`.
- A public header of a compositor subsystem: `include/<dir>`.
- A compositor subsystem's implementation: `src/<dir>`.
- A design decision: the owning document in `devnotes`, per `server.md` §8, never
  this file.
- An example program, config, or test harness: `assets`.
