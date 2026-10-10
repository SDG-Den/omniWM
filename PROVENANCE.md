# Provenance record

This file records third-party code and definitions that are copied or ported
into this repository, so that the GPL-3.0 obligations of `LICENSE` are traceable
to a source. `devnotes/licence.md` is the document that requires it, and this
file is phase 02's B7.

The rule: an entry is added at the moment of the copy, not later. A port counts
as a copy even when the code is translated, renamed, or restructured, because a
port still derives from the source. The applicable copyright notice is preserved
at the point of the copy, in the file or in the license text it carries.

| area | source project | source path | licence | what was taken | date |
|---|---|---|---|---|---|
| `protocols/wlr-layer-shell-unstable-v1.xml` | wlroots / wlr-protocols | `wlr-layer-shell-unstable-v1.xml` | X11-style permissive (notice in file) | vendored protocol definition, unchanged | 2026-10-02 |
| `protocols/wlr-foreign-toplevel-management-unstable-v1.xml` | wlroots / wlr-protocols | `wlr-foreign-toplevel-management-unstable-v1.xml` | X11-style permissive (notice in file) | vendored protocol definition, unchanged | 2026-10-02 |
| `protocols/wlr-output-power-management-unstable-v1.xml` | wlroots / wlr-protocols | `wlr-output-power-management-unstable-v1.xml` | MIT (notice in file) | vendored protocol definition, unchanged | 2026-10-02 |
| `protocols/meson.build` | MangoWM | `protocols/meson.build` | GPL-3.0 (same as this project) | adopted unchanged | 2026-10-02 |

The three protocol XML files are vendored rather than generated from the
`wayland-protocols` package because they are the wlroots protocols that wlroots
exposes but does not generate for us. Each carries its own permissive copyright
notice inside the file (X11-style for two, MIT for one), which is what the
licence requires and what is preserved.

The expected next entry is the input port. `input.md` §3 is about to be the
largest copy in the project, because `generaldesign.md` §14 decides that the
input layer reuses Mango's implementation where it can; that copy is not made
yet, so it has no row here.

## What an entry has to carry

- The destination path, so the record is findable from the tree.
- The source project and the source path, so the origin is checkable.
- The licence, with the notice preserved at the copy when the licence is not the
  project's own.
- What was taken, so a later reader can tell a whole-file copy from a table of
  constants.
- The date, so the entry matches the commit that made the copy.
