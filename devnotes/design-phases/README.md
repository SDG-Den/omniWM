# Design phases

The plan for finishing the design. One file per phase, each a stub: what the
phase should contain, what it resolves, and why it sits where it does. These are
not detailed plans yet. The point of writing them down now is that the decision
state is tracked, so a later session picks up a phase rather than re-deriving
which decisions are open.

## What this is derived from

`missing-devnotes-topics.md` is the inventory: thirteen rows, each a document
that does not exist or a gap in one that does. Three of the rows are now closed
(`input.md` was written, `windows.md` and `tags.md` exist as prototypes), which
leaves ten documents to write and a set of open decisions inside the documents
that already exist.

The open decisions are not only in the table. Each existing document carries its
own open-items section, and those are folded into the phases below rather than
being tracked separately, so there is one place to look:

| source | what is actually there |
|---|---|
| `generaldesign.md` §19 | 5 bullets, none open: 2 unresolved external facts now owned by phase 02's build half (the scenefx extension set, and the wlroots release it constrains), 1 closed this pass (no interpreter or library is a deliverable of the window manager), 1 owned by `tags.md` §7, and 1 explicitly deferred to `draw.md` |
| `helpers.md` §11 | 5 bullets, **none open**: 2 are struck and closed, and the other 3 restate decisions `§6.1` or `§14` already fixed. The `omni_event` payload question that was the one genuine item is closed — the field set is final and is not extended |
| `configstorelayout.md` §14 | 8 bullets, **none open**: the `get` result vocabulary is struck and closed, and the remaining 7 are v1 capacity choices with stated rationales rather than questions |
| `configstorage.md` §14 | 11 bullets in two groups, 8 in §14 and 3 in §14.1. §14 records 3 things fixed and 5 deliberate deferrals, none of them a question. **§14.1 is a second group and is not a deferral**: 3 store operations (`tags.md` §5 and §7 oblige) recorded as *required* rather than deferred, with the subtree exchange called out as not being a special case of the subtree delete |
| `layoutengine.md` §11 | 21 bullets: 20 closed or struck, 1 deferred to phase 10 (mid-animation retarget, which phase 10 now owns), and **none open**. The stored-or-derived membership bullet that used to be the open one is closed as **stored**, by the same decision `tags.md` §9 records |
| `tomlparser.md` §11 | 5 bullets, none open: 1 delegated to the upstream TOML specification, 1 out of scope (datetime arithmetic), 1 decided in favour of `duration`, 1 that states the per-key fallback rule as closed, and 1 that separates a malformed key from a blank one |
| `windows.md` §13 | 2 groups, **none open**: 5 numbered items all closed or deferred-with-shape-fixed, and a closing paragraph that settled the override-representation blockers. The one thing recorded as inferred rather than decided is where a deferred tag-seed write is kept, and `§9.3` names it |
| `tags.md` §9 | 3 "not covered" bullets, one placement decision recorded separately, then 4 struck and closed, **none open** |
| `input.md` §7, §7.2 | 4 not-covered bullets in §7 and 3 more in §7.2; one of the seven is the substantive gap and the rest are owned elsewhere or not needed at stage 4 |
| `ipc.md` §8 | 8 bullets; 2 of them were stale restatements of decisions already closed and have been corrected. `swap_tags` was added to §4 rather than here, because it is specified rather than open |
| `server.md` §9 | 2 bullets, both delegating to the compositor subsystem work that follows it |

The counts are as of this reconciliation pass, and **no document listed above
carries an open item**. The two that used to, `layoutengine.md` §11 and `tags.md`
§9, pointed at each other on a single question, and both now close it the same
way: client-set membership is stored, not derived, because `layoutengine.md` §3.2
makes the solve a pure function of committed state and a derived membership is a
`HashSet`-shaped iteration order that no golden-file test could pin. Neither blocks
another phase, and the choice costs one write per membership change.

## What the design work is actually for

The plan is sequenced for **implementation**, not for finishing the prose, and the
reason is that the design work has a second output besides the documents:
[scaffolding.md](scaffolding.md), the inventory of every helper, registration
path, backend and convention that the finished design implies.

That inventory cannot be produced on demand at stage 3. A helper is only
identifiable as a helper once you know what several features all have to do, and
that is a conclusion reached by designing the features, not a guess made
beforehand. Designing input is what reveals that binding resolution needs a name
index and a journal subscription, because only then is there a hot path that
needs one. So the inventory fills in as the phases are worked, and it arrives at
stage 3 already written down.

Stage 3 is then the substrate stage: it implements the finished inventory, and
stage 4 onward is writing features against it. [02](02-substrate.md) states this
in roadmap terms, in a section titled "What stage 3 is actually for". The
consequence for this plan is that phases 00 to 03 are the ones whose output is
code later stages build on, and phases 04 onward can absorb a gap because the
scaffolding already exists. A phase that is skipped or thin is recoverable; the
substrate done wrong is paid five times over, in stages 4, 5, 6, 7 and 8
separately.

This also resolves the two apparent departures from the roadmap below. They are
not departures at all once stage 3 is read as the substrate stage, because the
substrate is the store, the core and input, which is phases 00 to 03.

## The ordering, and why it is not the roadmap's

The `README.md` roadmap numbers are build order. These are design order, and they
differ in two places, both of which are findings rather than preferences.

**The store and the core come first (phases 0 to 2), not input.** Every other
phase reads `omni_layout.h` for constants and `configstorelayout.md` for what a
record means, so the open items in those two documents block the rest. Phase 0
is a reconciliation pass over documents that already exist, which is unglamorous
and is the highest-leverage work available.

**The look phases are ordered draw, decorate, animate, not decorate, animate,
draw.** `decorate.md` owes "the scene graph node types and their animatable
properties" indirectly: it overrides border, shadow, blur, glow, opacity and
rounding on nodes, and `animate.md` owes "animatable properties on nodes". Both
need the node model, and the node model is `draw.md`. The roadmap puts basic
decorations at stage 8 and the scene graph at stages 12 to 14, so either the
roadmap's stage numbers are wrong about the dependency or `draw.md` has to be
partly written before stage 8. **That is unresolved and is phase 8's first
item.**

The rest of the order is a one-way dependency chain, each arrow being "reads the
document on the right":

```
00 reconciliation
  -> 01 testing      (its assertion set is configstorelayout §12)
  -> 02 build        (nothing can be run until it exists)
  -> 03 input
  -> 04 monitor  -> 05 tags  -> 06 windows  -> 07 layoutengine
                                                     |
                                       08 draw <-----+----> 09 decorate
                                            |                  10 animate
                                            v
                                     11 protocols
                                            v
                                     12 languages
```

Phases 00 to 03 are the substrate: the store, the build and libraries, and input.
Nothing after 03 blocks on another phase's prose, which is the property that
makes a thin phase recoverable.

`monitor` before `tags` matches the roadmap (stage 5 before stage 7), and the
dependency runs the other way than it used to: `tags.md` §9 previously left the
storage of a monitor's tag list to `monitor.md` as a deliberate split, and it no
longer does, because a promise to hand storage to a document that has not been
written is how a tag list ends up owned by nobody. `tags.md` now holds both the
ordering semantics and the storage, and `monitor.md` reads them. So phase 04 reads
phase 05 rather than the reverse, which is a stronger ordering constraint than the
one this plan was built on and does not change the phase order. `languages` is last
because it is
the only phase whose two inputs (`ipc.md`, `helpers.md` §6) are already finished,
so it is the lowest-risk phase and there is no reason to spend early attention on
it.

## The phases

| phase | subject | stages | priority |
|---|---|---|---|
| [00](00-reconciliation.md) | reconciliation of documents that exist | 1-4 | P0, substrate |
| [01](01-testing.md) | `testing.md` | 1 | P0, substrate |
| [02](02-substrate.md) | `build.md`, `licence.md`, the internal codebase design | 3 | P0, substrate |
| [03](03-input.md) | `input.md` finalization | 4, 10 | P0, substrate |
| [04](04-monitor.md) | `monitor.md` | 5 | P1 |
| [05](05-tags.md) | `tags.md` finalization | 7 | P1 |
| [06](06-windows.md) | `windows.md` finalization | 5, 8 | P0 |
| [07](07-layoutengine.md) | `layoutengine.md` finalization | 6 | P0 |
| [08](08-draw.md) | `draw.md` | 12, 13, 14 | P1 |
| [09](09-decorate.md) | `decorate.md` | 8, 11 | P1 |
| [10](10-animate.md) | `animate.md` | 9 | P1 |
| [11](11-protocols.md) | `protocols.md` | 16, 18 | P2 |
| [12](12-languages.md) | `languages.md` | 15, 16, 17, 18 | P2 |
| | [scaffolding.md](scaffolding.md): the accumulating inventory, appended to by every phase | 3 | P0 output |

## Two things this plan does not do

**It does not schedule implementation.** Every phase is design work. The
`README.md` checkboxes are build stages and this plan does not move them.

**It does not claim the phase order is the only one that works.** It is the order
that keeps a phase from being written against a document that has not been
decided yet. Phases 8 through 12 are the ones most likely to be reordered once
phase 8's roadmap question is answered, because that answer decides whether the
scene graph is written before decorations or after.

## Design Phase Tracking
- [x] Phase 00
- [ ] Phase 01
- [ ] Phase 02
- [ ] Phase 03
- [ ] Phase 04
- [ ] Phase 05
- [ ] Phase 06
- [ ] Phase 07
- [ ] Phase 08
- [ ] Phase 09
- [ ] Phase 10
- [ ] Phase 11
- [ ] Phase 12
- [ ] post-phases: finalize scaffolding requirements.
