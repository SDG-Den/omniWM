# Phase 02: stage 3, the substrate

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

**B1. The dependency set.** Which libraries, at which versions, and which are
optional. `helpers.md` §7.2 already assumes PCRE2, and the container kit in §7.3
assumes more.

**B2. The wlroots version, and the scenefx coupling.** `generaldesign.md` §19 says
scenefx 0.5 requires wlroots 0.20 and that the tracked wlroots release is
unresolved. This is a hard coupling, not a preference, and it constrains
everything else in the document.

**B3. The scenefx extension set.** `generaldesign.md` §19 calls this the largest
risk to "Any Look". The specific need is user GLSL shaders and 3D transforms,
which `draw.md` and `animate.md` both depend on. The open question is whether
those extensions exist as a maintained patch or require a fork, and the answer
changes the project's relationship to its own upstream.

**B4. The solver's arithmetic width, and CPU versus GPU.** `missing-devnotes-topics.md`
notes the layout engine does not depend on the answer. It still has to be in the
document because it decides what the solver links against.

**B5. `flake.nix`, with `cache.nixos.org` set explicitly as the substituter**
rather than inheriting the ambient config, so a build is reproducible from a
clean machine. This is a stated requirement, not a preference.

**B6. The Meson structure.** `missing-devnotes-topics.md` says Mango's `meson.build`
is the model, from `architecture-audit.md` §5. What carries over and what does not
is open, and the answer depends on how many components there are by stage 3.

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

**D8. The test seam.** Phase 01 writes the store tests, which are the only tests
with a fixed assertion set today. Every later stage needs to test a component, and
nothing says what a component test looks like when the component needs a wlroots
event loop. This is the gap that makes stages 5 onward either well tested or not
tested, and it is much cheaper to close now than after four components have each
solved it differently.

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
- A component test exists that runs without a wlroots event loop, or the reason it
  cannot is written down.
