# Scaffolding inventory

The design work has two outputs. The first is the documents. The second is this
file, and it is the one that decides what stage 3 of implementation actually is.

The inventory has two sources, and confusing them is the mistake this file exists
to prevent.

**Anticipated, ahead of time.** Most of the general scaffolding is knowable now,
before any of the features are designed, because it follows from the project's
shape rather than from any one feature. Every feature in this project is a
component that registers options, actions and triggers, reads a shared block, and
subscribes to changes, so a component substrate, a registration path, a store read
path and a subscription path are needed regardless of what the features turn out to
be. Phase 02's design half is that list. The *Conventions* section below is almost
entirely of this kind: nobody had to design a feature to notice that no document
says how to name a file-local symbol.

**Surfaced, during design.** The rest is not knowable in advance, because a helper
is only identifiable as a helper once you know what several features all have to
do. Designing input is what reveals that binding resolution needs a name index and
a journal subscription, because only then is there a hot path that needs one.
Designing `draw.md` is what reveals that decorations and animations both need a
node model. These rows arrive as the phases that surface them are worked, and they
are the more valuable half, because a row nobody anticipated is a design output
rather than a restatement.

So the file is never finished by any single phase. It starts with the general
scaffolding that can be named now, and it grows as the features are designed, with
the full scope known only when the last phase is worked. At stage 3, the job
becomes: implement the rows in the *specified*, *partial* and *anticipated*
sections, and close the rows in *undetermined*. What does not happen at stage 3 is
discovering a fourth helper because a feature needed it.

## The rule

Every phase, when worked, appends the helpers and scaffolding it implies, and
marks both whether an existing document specifies them and whether the row was
anticipated or surfaced. A phase that surfaces a row nobody expected should say so,
because that is the row that would otherwise have been missed.

Status values:

- **specified** — a document states it, in enough detail to implement.
- **partial** — a document states the intent, not the shape.
- **anticipated** — foreseen from the project's shape, not yet confirmed by a
  designed feature. True of anything phase 02 lists that no phase has needed yet.
- **undetermined** — no document covers it; the phase that will is named.

## SHM and store handling

The category the user named first. This is where all the block access lives, and
it is the category with the most rows, because a store read is not one operation
but four.

| Requirement | Specified in | Status |
|---|---|---|
| read-only mapping, name resolution, version and header check | `configstorage.md` §1-2 | specified |
| catalog name index lookup, the read path | `configstorage.md` §3.1, `generaldesign.md` §14.4 | specified |
| journal read and change detection | `configstorage.md` §5 | specified |
| writer transaction and futex-based commit | `configstorage.md` §1.1 and `configstorelayout.md` §13 | specified |
| get/put/take/claim/list request surface | `configstorage.md` §8 | specified |
| validation guard tiers, four structural plus replay | `configstorage.md` §12, `configstorelayout.md` §12 | specified; phase 00 split the flat bundle into L1-L4 and `01-testing.md` item 7 carries the per-tier cases across all five tier labels |
| subtree delete, subtree exchange, generation-correct deletion | `configstorage.md` §14.1, `tags.md` §5 and §7 | specified as an obligation; the mechanism is phase 02 item D9 |
| process-private derived cache and its invalidation | `configstorage.md` §3.1 and `helpers.md` §5, between them | partial; **the worked example this row used to cite does not exist.** `input.md` has no §14.2 and describes no cache, so the two halves of the pattern are specified and no component yet instantiates them. Phase 02 item D3 |
| `get` result codes | `omni_layout.h` §12, one `OMNI_ERR_*` set plus `OMNI_SOCK_ERR_*` | specified; phase 00, and `configstorelayout.md` §14's bullet is struck |
| fresh-block header initialization, including the arena free head | `configstorage.md` §13.1, `configstorelayout.md` §3 and §6.1 | specified; phase 00 fixed the creator's low run, the `name_ref` chain for the remainder, a nonzero `catalog_free_count`, and clearing a `DESTROYED` name |
| the rule for which component fields live in the block | `configstorage.md` §1 has the scope taxonomy | undetermined; phase 02 item D2 |
| per-component store handle, so a component does not re-derive any of the above | none | undetermined; phase 02 item D2 |

## Registration

The second category the user named. Registration is what makes a feature additive:
a new feature registers rather than edits, and the helpers are what make that one
call instead of a sequence.

| Requirement | Specified in | Status |
|---|---|---|
| component descriptor and instance, toggle engine, suspension | `helpers.md` §3 | specified |
| component activation order | `helpers.md` §9 records the problem, not the answer | undetermined; phase 02 item D1 |
| action registry and typed positional arguments | `helpers.md` §6 | specified |
| option registration and typed read/write | `helpers.md` §7 | specified |
| trigger registration and dispatch | `helpers.md` §5 | specified |
| in-process event dispatch | `helpers.md` §5 | specified |
| a richer in-process payload than a journal entry | none, deliberately | **closed as not required**; phase 00: the field set is final and is not extended, a component reads the block, and a synthetic event is not expressible. Phase 02 item D4 is the subscription pattern, not a payload schema |
| new TOML key registration | `tomlparser.md` | specified |
| device rule matching | `input.md` §4 and §5, Mango's `find_device_rule` kept as-is | specified; the matchers are Mango's name or `vendor:product:name`, not four, and the four-matcher vocabulary belongs to `04-monitor.md` item 2 |
| client kind set, which the solver partitions on | `windows.md` §10 | specified; phase 00 closed it at seven, so phase 07 has a fixed partition input |
| error and diagnostic reporting outside the store | `configstorage.md` §12.6 covers the store only | undetermined; phase 02 item D5 |

## Dependencies and backends

| Requirement | Specified in | Status |
|---|---|---|
| wlroots server backend and which managers are instantiated | `generaldesign.md` §12 at a high level | partial; phase 11 names the managers |
| core library, log, util | `helpers.md` §7 | specified |
| PCRE2 in the util layer | `helpers.md` §7 | specified |
| scenefx, for user GLSL, blur, glow and shader animation | `generaldesign.md` §19 calls it the largest risk | undetermined; phase 02, requirement list from phase 08 |
| protocol set, and the buffer-injection protocol | `configstorelayout.md` §10 defines the region descriptor | partial; phase 11 |
| a test seam for a component that needs an event loop | `devnotes/testing.md` §2 | decided 2026-09-29; item D8. Note this row sits in a section the stage-3 paragraph does not name |

## Conventions

The category with no open-items list anywhere, and almost entirely *anticipated*
rather than surfaced: no feature had to be designed to notice that no document
says how to name a file-local symbol. Each row is a decision nobody has made, that
every file inherits whether or not it was decided on purpose.

| Requirement | Specified in | Status |
|---|---|---|
| `omni_` prefix on exported symbols | `helpers.md` §7.2 | specified |
| naming for file-local symbols | `helpers.md` §7.2 does not say | undetermined; phase 02 item D7 |
| what belongs in a public header versus a private one | `filestructure.md` covers the layout, not this | undetermined; phase 02 item D6 |
| a symbol two components need | none | undetermined; phase 02 item D6 |
| how a failed option validation is reported | no general policy | undetermined; phase 02 item D5 |

## Nodes and visuals

Not part of the substrate, and listed separately so it is not mistaken for it.
These are phase 08 onward and they consume the substrate rather than requiring it.

| Requirement | Specified in | Status |
|---|---|---|
| node model, the shape everything visual decorates and animates | `draw.md` does not exist | undetermined; phase 08 |
| decoration property setters | `decorate.md` does not exist | undetermined; phase 09 |
| animation timeline scheduling and curves | `animate.md` does not exist | undetermined; phase 10 |
| solve scheduling and coalescing | `layoutengine.md` §5 has a pipeline, not the rule | undetermined; phase 07 |

## How stage 3 uses this

Rows marked specified are the core library and are phase 02's, with `helpers.md`
as their contract. Rows marked partial or undetermined in the *SHM and store
handling*, *Registration* and *Conventions* sections are stage 3's, and they are
the reason stage 3 is the hard stage. A stage 3 that implements only the
specified rows has shipped a core library and none of the substrate, and every
stage after it re-discovers the same gaps.

The checklist below is not stage 3 work. It exists so that a reader can tell at a
glance which rows stage 3 still owes.

## What stage 3 still owes

Phase 02's design items, in the order `02-substrate.md` presents them, against the
row each one closes. A row with no item here and no specification is a genuine gap
in the plan rather than an unstarted phase.

| row | closed by |
|---|---|
| component activation order (`Registration`) | D1 |
| which component fields live in the block (`SHM and store handling`) | D2 |
| per-component store handle (`SHM and store handling`) | D2 |
| process-private derived cache and its invalidation (`SHM and store handling`) | D3; no worked example exists yet, so this phase has to write the first one |
| the event and subscription pattern (`Registration`, the in-process row) | D4 |
| error and diagnostic reporting outside the store (`Registration`) | D5 |
| what belongs in a public header versus a private one (`Conventions`) | D6 |
| a symbol two components need (`Conventions`) | D6 |
| naming for file-local symbols (`Conventions`) | D7 |
| a test seam for a component that needs an event loop (`Dependencies and backends`) | D8 |
| subtree delete, exchange, generation-correct deletion (`SHM and store handling`) | D9 |
| how a failed option validation is reported (`Conventions`) | D5 |

**Three rows in that list are not where the text above says they are**, and saying
so is the point of writing the list down. D8's row is in *Dependencies and backends*,
which the paragraph above does not name among the three stage-3 sections, and D9's
row is new and is in *SHM and store handling* for the first time. And **D5 owns two
rows that are not the same question**: *how a failed option validation is reported*
is about the return path a caller sees, while *error and diagnostic reporting
outside the store* is about where a log line goes and what it names. They share an
owner by convenience of ordering, not by subject, and D5 should say which it is
addressing first.

The two rows above that are **not** stage 3's are the `partial` ones naming phase 11,
which is correct: a wlroots server backend and a protocol set are substrate
consumed by a later phase, and calling them stage-3 work would make stage 3 wait on
phase 11. The scenefx row is genuinely stage 3's, and is the one row in this list
that cannot be closed by writing a document, since the answer is a version.

## The build's view of the tree, 2026-09-27

`meson.build` now exists and names every file in the sections above, in
`core_sources`. It is the same list in build-system terms, which makes it a
second place that has to change when the tree does, and that is worth recording
here rather than leaving as a comment in a build file: the two lists drifting
apart produces a build that silently omits a file rather than a build that fails.

`ipc_sources` and `debugger_sources` are the two smaller groups, one file each,
and `src/main.c` is deliberately absent from all three because it belongs to the
disabled executable.

Three files named in `filestructure.md` and in the design documents did not exist
in the tree: `src/core/layoutengine.c`, `src/input/switch.c` and
`src/input/touch.c`. All three were created empty. They are scaffolding in the
literal sense — a placeholder for a file the design already commits to — and they
are not implemented. The `debugger/` and `ipc/` rows are the two paths in the list
that have no Mango counterpart, and both are marked as such in the build file so
that the difference from Mango is findable by someone reading the build and not
only by someone who remembers the design.

The build compiles all 62 sources, and they are all zero bytes, so it produces 62
empty object files. The earlier version of this paragraph said it compiled none
of them, which was true when only the header was installed and stopped being true
when the three shared libraries were enabled. What the empty objects do establish
is that every filename in the list resolves and every dependency resolves against
it; what they cannot establish is anything about the code. A build log full of
`.c.o` lines is not a compositor.

`include/shared/omni_layout.h` is the one file in the tree with content, and
`meson.build` checks it at configure time with `cc.compiles`, so a broken ABI
fails `meson setup` rather than the first test.
