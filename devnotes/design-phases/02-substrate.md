# Phase 02: stage 3, the substrate

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md).*

Stub. Not started.

Stage 3 is "create shared helpers and scaffolding (core libraries, logging and
server backend)". One roadmap stage, so one phase file. It has two halves that
look like separate jobs and are not: the build, which decides whether the design
is buildable at all, and the internal codebase design, which decides what the core
libraries have to cover so that every stage after this one is writing a feature
rather than inventing a mechanism. Scaffolding is part of the core libraries, not
something delivered alongside them.

Items are labelled **B** for the build half and **D** for the design half, because
they interleave and a single numbering would hide that. Every phase after this one
appends to [scaffolding.md](scaffolding.md), which is this phase's other output.

## What stage 3 is actually for

Stage 3 is the hardest stage in the roadmap for a reason that is not visible in
the stage name. Every stage after it is a matter of writing a feature, and the
reason that is possible is that this is where the internal codebase design gets
settled. Each later stage adds a component (`helpers.md` §3), subscribes to the
journal, registers options, actions and triggers, reads the store, and dispatches
events. If the component substrate, the event path, the option and action
registration, and the store access pattern are all sound and all demonstrated
once, each later feature is mostly its own logic. If they are not, every later
stage re-discovers the same gaps in a slightly different place.

Stage 4 then consumes this rather than inventing it, which is why the input port
is a good first real feature: a binding has to be resolved from a name, read out
of shared memory, checked against a journal, cached in a derived index, and
dispatched into an action with typed arguments, with the result published in a
commit. That exercises the entire substrate in one path. A stage 3 that shipped
the core library without settling these would push the same discovery into stages
4, 5, 6, 7 and 8 separately, and each would find it slightly differently.

## The build half

`missing-devnotes-topics.md` marks `build.md` P0, and it is also the phase with the
largest external risk, because the biggest open item in the whole design depends
on a third-party project.

**Build scaffolding already in the tree, 2026-09-27.** `flake.nix`, `meson.build`,
`meson_options.txt`, `protocols/` and `nix/` exist and all four Nix entry points
work. They were created in phase 01 rather than here, because a test document with
no runner in the tree is a specification nothing checks, and the runner is a build
system. This does not close the items below; it gives them a build to be decided
against. What it did already settle is recorded against each item.

**B1. The dependency set.** Which libraries, at which versions, and which are
optional. `helpers.md` §7.2 already assumes PCRE2, and the container kit in §7.3
assumes more. **Mango's list is in the tree unchanged** and configures against
nixpkgs-unstable: libinput 1.31.3, libxcb 1.17.0, libxkbcommon 1.13.2, PCRE2
10.48, pango 1.57.1, cjson 1.7.19, pixman 0.46.4, wayland 1.26.0,
wayland-protocols 1.49, wlroots 0.20.2, scenefx 0.5.0, libGL 1.7.0, libdrm
2.4.134, with libxcb-wm 0.4.2 and xwayland 24.1.13 behind `meson_options.txt`'s
`xwayland` feature. XWayland is the only optional one. Still open is whether we
add anything on top, which is what the user described as likely.

One sub-question inside B1 that the package split raised is settled: all five
packages carry the whole Mango list while the sources are empty, and that is the
correct long-term shape. `libomniwm` is the internal development library omniWM
itself is built from — an internal library, not a hand-off surface. It links
wlroots, scenefx, libinput, pango, GL, drm, xcb and xwayland directly, because
the WM is built on them; the "part other projects link" is not it. The properly
narrow surface is a separate SHM-client library that does not exist yet, whose
only interface dependency is the block layout in `include/shared/`. `build.md`
§2.1's phrase "the part other projects link" is corrected for that reason.

**B2. The wlroots version, and the scenefx coupling.** `generaldesign.md` §19 says
scenefx 0.5 requires wlroots 0.20 and that the tracked wlroots release is
unresolved. This is a hard coupling, not a preference, and it constrains
everything else in the document. **Answered against nixpkgs-unstable**: wlroots
0.20.2 and scenefx 0.5.0 are both released and both present, so the coupling
resolves against a matched pair rather than against a fork. The flake pins
`wlroots_0_20` rather than `wlroots` to say so explicitly, since they are the same
version today and will not stay that way.

**B3. The scenefx extension set.** `generaldesign.md` §19 calls this the largest
risk to "Any Look". The specific need is user GLSL shaders and 3D transforms,
which `draw.md` and `animate.md` both depend on. The open question is whether
those extensions exist as a maintained patch or require a fork, and the answer
changes the project's relationship to its own upstream. **The scaffold takes
scenefx from nixpkgs, not from Mango's `github:wlrfx/scenefx` flake input.** Mango
pins that input precisely because it needs extensions nixpkgs does not carry, so
this is a deliberate deferral of B3 rather than an endorsement of the released
extension set. The comment in `meson.build` says so, because a reader finding
nixpkgs scenefx and assuming the extension question was answered would be worse
than not finding the question at all.

**Decided 2026-09-29: no fork and no patch; the extensions are built into
omniWM.** This is a deliberate decision to break ground rather than track
upstream — scenefx has no extension point (its installed public pass API is a
closed set of draw calls, and its shader/compile helpers live in an uninstalled
header), so extending it means owning the render layer that uses it, not patching
it. The list below is the agreed feature list, and the target is a rendering
**toolkit**, not a fixed effect library: each item is built from a small set of
simple, generic, flexible primitives that can produce any desired effect, and a
preset is a mapping of several of those primitives onto one config key. The core
does not implement super specific, complex effects; it ships the toolkit, so any
looks-based feature can be implemented rapidly on top of it later. The specific
items below are what the toolkit has to be able to express, not a list of
one-off code paths.

- 3d transform windows
- 2d transform windows
- above-background and above-windows full-size custom-render buffer support
- multi-layer borders
- textured borders (tile and stretch-to-fit)
- nine-patch support
- shadered border support (gradients and similar)
- custom shader effects
- custom shader animations
- animated shaders on borders
- glow and shadows (one feature underneath, exposed two ways)
- window opacity
- custom GLSL shader support
- all buffers available in SHM for direct modification

All of it in-GPU, which accepts that a different higher-level rendering library
may have to be integrated into the WM to support and extend scenefx. A proposed
integration is not yet chosen; that is a consequence of this decision, not a
second deferral. This affects the dependency set (B1): the render layer is a
core service with its own dependency set, and `libomniwm` being internal means
carrying scenefx and whatever the renderer needs is normal for it.

**B4. The solver's arithmetic width, and CPU versus GPU.** `missing-devnotes-topics.md`
notes the layout engine does not depend on the answer. It still has to be in the
document because it decides what the solver links against.

**Decided 2026-09-29: fixed-point internally, integer at commit, CPU.** Every
reference WM (Mango, Hyprland, wayfire, Halley, ShojiWM) is CPU with floating
point internally and integer at the commit boundary, so CPU was never in doubt.
Fixed-point over float is a deliberate divergence, chosen for cross-architecture
bit reproducibility: a deterministic golden-file solver cannot rest on float, and
determinism outranks aarch64 (which is only a Nix default; it is still wanted,
just not at the cost of determinism). The user-facing cost is absorbed in the
parser, not the block: `tomlparser.md`'s float handling interprets literals into
fixed-point so TOML stays easy to write, while a SHM client reads the stored
representation directly — the trade for much lower-level access, with the
documentation that goes with it.

**B5. `flake.nix`, with `cache.nixos.org` set explicitly as the substituter**
rather than inheriting the ambient config, so a build is reproducible from a
clean machine. This is a stated requirement, not a preference. **Done**:
`nixConfig.extra-substituters` names `https://cache.nixos.org`. Note that Nix
requires `--accept-flake-config` to honour it, so a clean-machine user gets it
from the prompt rather than silently; that is Nix's behaviour and not something
the flake can override.

**B6. The Meson structure.** `missing-devnotes-topics.md` says Mango's `meson.build`
is the model, from `architecture-audit.md` §5. What carries over and what does not
is open, and the answer depends on how many components there are by stage 3.
**Mango's structure is in the tree**, with `core_sources` naming all 62
intended translation units and the `executable()` call commented out. The list is
`filestructure.md`'s tree in build-system terms, so it is a second place that has
to change when the tree does. Whether the list survives stage 3 is still open,
and the package split has made the question sharper: `libomniwm` is the internal
development library, so the 62 units are the WM's own build, and the unit split
is about component boundaries rather than about shielding an external library
from dependencies. Which units belong to the core and which to the compositor is
a phase 03 decision, and B1 above records the dependency-set correction.

A general principle governs that decision, stated 2026-09-29: the core lib is a
simple, generic feature set that cannot do much by itself, but serves as a
foundation to build new features extremely quickly, so omniWM can realistically
support the massive feature set it wants. A unit that is a super specific,
complex behaviour belongs in the compositor layer or in a component on top of the
core, not inside it. This is a rule about what a *unit* implements, not about
what `libomniwm` links — wlroots and scenefx are dependencies of the internal
library, not features of it, and B1's correction stands independently.

**B7. `licence.md`**, which is small: the licence file itself, and a provenance
record for anything actually copied. GPL-3.0 is already decided by
`architecture-audit.md` §5 from Mango's `LICENSE`. DuckWM's and MangoWM's licences
are therefore not an obstacle, and neither are most other window managers'. The
provenance record is a chore at the moment of the first copy, but the input port
(`input.md` §3) is about to be the largest copy in the project, so this is
earlier than it looks.

## The design half

This half is the one most likely to be skipped, because none of it is visible in
any single document's open-items list. It is also the only part of the plan whose
output is conventions rather than decisions, which is exactly why it has to be
written down: a convention that is never written is a convention every later file
reinvents.

**D1. The component lifecycle, demonstrated rather than specified.**
`helpers.md` §3 defines the descriptor, the instance, the toggle engine and
suspension. What is not written is what a component *looks like* in practice:
which of its fields live in the descriptor, which live in process memory, and
which live in the block. Every later component answers that question, and the
answer has to be the same every time or `WINDOW_DEPENDENT` scope and the `save`
exclusion test in `configstorage.md` §12 start disagreeing with each other.

**D2. The rule for what goes in the block versus process memory.** The store is the
configuration, and a component's transient state is not, but the line between them
is drawn per component and a wrong draw is invisible until a `save` writes
something it should not. `configstorage.md` §1's scope taxonomy is the vocabulary;
this phase says how a component uses it.

**D3. The store access pattern, as a pattern.** `configstorage.md` §3.1 optimizes
the read path and `generaldesign.md` §14.4 has the index design for bindings. What
is not written anywhere is the general form every component copies: read through
the catalog name index, cache privately, invalidate on a journal event, never
cache a frame offset.

This item used to name `input.md` §14.2 as the worked example, and **that example
does not exist**: `input.md` has eight sections and no §14, and it describes no
private cache. Both halves of the pattern are specified and neither has an
implementation-shaped instance — `configstorage.md` §3.1 is the lookup,
`helpers.md` §5 is the invalidation trigger — so what is missing is a component
that does both. `input.md` is the right candidate and the right size, since it is
the component with the most reads, but writing that instance is a change to
`input.md` rather than a consequence of it, which is why the pattern has to be
stated here before it can be cited anywhere.

**D4. The event and subscription pattern.** `helpers.md` §5 and `ipc.md`'s watch
mechanism both exist, and this item used to say that a follow-up design was owed
because `omni_event`'s field set is fixed by the journal slots. That is closed:
`helpers.md` §7 now records that the field set is final and is **not** extended, so
there is no follow-up to do and no component should be written expecting one. The
part of the item that survives is the pattern itself, and the consequence is
sharper for being settled: a component that wants more than a journal entry reads
the block, and a synthetic in-process event is deliberately not expressible
because it would let a dispatch deliver something no commit produced. So D4 is now
about how a component subscribes and invalidates, not about a payload schema.

**D5. Error and diagnostic conventions.** `configstorage.md` §12.6 has a diagnostic
policy for the store. A component outside the store has no stated policy: what a
failed option validation does, what a failed action reports, what goes to the log
versus to a client. `helpers.md` §6.1's `ACTION_FAILED` is one answer in one place
and there is no general rule.

**D6. Header and include conventions.** `filestructure.md` says component files
include `include/commonheaders.h` and nothing else, and that `src/<dir>` mirrors
`include/<dir>`. What is missing is the rule for what goes in the public header
versus the private one, and the rule for a symbol that two components need.

**D7. Naming.** `helpers.md` §7.2 establishes the `omni_` prefix rule and that the
dwms macros are renamed as a whole and nothing more. What is not settled is
whether a file-local symbol is prefixed at all, which is the difference between a
codebase where `grep` works and one where it half works.

**D8. The test seam.** *Owned by phase 01, decided 2026-09-29.* Phase 01 writes
the store tests, which are the only tests with a fixed assertion set today. Every
later stage needs to test a component, and nothing says what a component test
looks like when the component drives wlroots objects. `libomniwm` is an internal
library and links wlroots directly, so the answer is a wlroots test harness for
the components that own wlroots objects and the plain unit runner for the pure
ones — the seam decides which. This is the gap that makes stages 5 onward either
well tested or not tested, and it is much cheaper to
close now than after four components have each solved it differently. The decision
is that `devnotes/testing.md` states the seam, and this item stays here as the
record of what it was blocking.

**D9. The three store operations a tag is a container for.** `configstorage.md`
§14.1 now records these as *required* rather than deferred, because `tags.md` §5
makes a tag a container and §7 makes a swap an exchange of two containers: a
subtree delete, a subtree exchange, and generation-correct deletion. D3 is the read
pattern and this is the write pattern, and the reason it belongs to the substrate
rather than to phase 05 is that phase 05 states the obligation and cannot choose the
mechanism — an exchange is a different operation from a delete, not a variation on
it. A delete may free each frame as it recurses; an exchange frees nothing at all,
because every child of one tag is still live in the other, and an implementation
that assumes the second is built from the first runs out of slots halfway through
the second tag. Generation-correct deletion is the third because the same recursion
has to invalidate the names of everything it removed, and `catalog_free_count` is
what tells a reader the chain is consistent.

**D10. The server.** *Owned by phase 02, decided 2026-10-03.* `server.md` §9 defers
three items to "the compositor subsystem work that follows", and until now no row
of `README.md`'s inventory claimed the server, so that deferral had no landing
place and `testing.md` §7 had to record the server's live tests as unassigned.
Phase 02 is where stage 3's server backend lives — `scaffolding.md` marks the
backend and the header split as this phase's deliverable — so the server is
designed here, with the substrate it sits on, and the half of the IPC surface
that needs a live compositor to answer comes with it.

The reason this phase rather than one of its own is that the server has no
decision of its own to make. Its open items are the wlroots surface it wraps
(`wlr_output`, `wlr_seat`, the pointer and touch protocols) and the contents of
the registration table, and both are answers to "what does the substrate expose",
which is this phase's question. A phase of its own would have nothing to decide
before this one runs. Its tests follow D8's line rather than a new one:
`testing.md` §2 puts a component that drives wlroots objects under the wlroots
harness, so the server's components and the live IPC dispatch are harness cases,
and both land with the substrate that makes them linkable.

## What it resolves

- Whether the design is buildable at all, which is the precondition for phase 01's
  tests being runnable rather than merely specified.
- The scenefx fork-or-patch question, which is the single largest external risk
  and which `draw.md` and `animate.md` both inherit.
- Reproducibility, which is currently a requirement with no mechanism.
- Why stage 4 through 18 are feature work. Not because the features are easy, but
  because the substrate underneath them was settled once.
- The failure mode where four components each reinvent the store cache, the
  subscription, and the error path, in three slightly different ways.
- The test seam, which is the difference between a project where the later stages
  are verifiable and one where they are not.
- The write pattern of D9, which phase 05's `configstorage.md` §14.1 rows are
  specifications for rather than only names, and which phase 01's store tests need
  in order to assert something more interesting than that a write took effect.
- Where the server is designed, which `server.md` §9 deferred and no phase
  claimed, and therefore where its live IPC tests are written.

## Why it is second

The build half needs phase 00 in the sense that a build cannot be validated
against a half-reconciled ABI, but it does not block any design decision. The
design half is a consequence of the plan's second output existing: the scaffolding
rows in `scaffolding.md` marked stage 3 are this phase's deliverable, and they can
only be written once the features that need them have been designed. That is why
this phase sits after the phases that produce those rows and before everything
that consumes them.

## Done when

- The wlroots and scenefx versions are named, and the fork-or-patch question has
  an answer.
- `flake build` succeeds on a machine with no ambient Nix configuration.
- The provenance record exists as a file, even if it is nearly empty.
- One worked component exists in the tree, following the conventions, and later
  phases cite it instead of restating the patterns.
- The three operations of D9 have a stated mechanism each, and the exchange is
  documented as a distinct operation from the delete rather than as a reuse of
  it.
- The block-versus-process-memory rule is written as a rule, and `configstorage.md`
  §1's scope taxonomy is referenced rather than paraphrased.
- The seam is stated in `devnotes/testing.md` §2: which components run in the
  plain unit runner and which under the wlroots test harness, with the line
  between them defined.
- `server.md` §9's three deferred items have answers here, and the server's
  components appear in `testing.md` §2's harness list rather than in its
  unassigned row.
