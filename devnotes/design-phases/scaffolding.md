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
| journal read and change detection | `configstorage.md` §9 | specified |
| writer transaction and futex-based commit | `configstorage.md` §10, `ipc.md` | specified |
| get/put/take/claim/list request surface | `configstorage.md` §8, `configstorelayout.md` §13 | specified |
| validation guard tiers, six of them | `configstorage.md` §6 | partial; tier ownership is phase 00 |
| process-private derived cache and its invalidation | `input.md` §14.2 is the worked example | partial; the general form is phase 02 item D3 |
| `OMNI_GET_*` result codes | `configstorelayout.md` §14 records the gap | undetermined; phase 00 |
| fresh-block header initialization, including the arena free head | `configstorelayout.md` §2 | undetermined; phase 00 |
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
| a richer in-process payload than a journal entry | `helpers.md` §11 records that the field set is fixed | undetermined; phase 02 item D4 |
| new TOML key registration | `tomlparser.md` | specified |
| device rule matching, four matchers | `input.md` §10 | specified |
| client kind set, which the solver partitions on | `windows.md` §10 leaves it open | undetermined; phase 06 |
| error and diagnostic reporting outside the store | `configstorage.md` §12.6 covers the store only | undetermined; phase 02 item D5 |

## Dependencies and backends

| Requirement | Specified in | Status |
|---|---|---|
| wlroots server backend and which managers are instantiated | `generaldesign.md` §12 at a high level | partial; phase 11 names the managers |
| core library, log, util | `helpers.md` §7 | specified |
| PCRE2 in the util layer | `helpers.md` §7 | specified |
| scenefx, for user GLSL, blur, glow and shader animation | `generaldesign.md` §19 calls it the largest risk | undetermined; phase 02, requirement list from phase 08 |
| protocol set, and the buffer-injection protocol | `configstorelayout.md` §10 defines the region descriptor | partial; phase 11 |
| a test seam for a component that needs an event loop | none | undetermined; phase 02 item D8 |

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

The last section is a checklist, not stage 3 work. It exists so that a reader can
tell at a glance which rows stage 3 still owes.
