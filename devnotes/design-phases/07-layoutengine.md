# Phase 07: `layoutengine.md` finalization

Stub. Not started. The document exists and is the most complete of the set, with
eleven closed open-items kept as a record and six still open.

The stage is 6 and the priority is P0, and this is the phase where the design has
the most committed claims, so it is worth being explicit that the work is not
writing a solver. It is closing six questions and reconciling three.

## What it should contain

**1. Pass triggers in detail: when a solve is scheduled versus coalesced.**
`missing-devnotes-topics.md` names this first, and it is the one that decides
whether the solver runs on every event or once per frame. §5 has a pipeline; what
it does not have is a statement of the coalescing rule.

**2. The solved-to-arranged persistence rule.** §11's second open item, and the
one with the sharpest phrasing: the solved half is settled by
`OMNI_SECTION_SOLVED_LAYOUT`, so a script can already read a layout, and what is
left is whether the block also carries the *execution* of one, which decides
whether an external animator has to be told where windows are going or can work it
out. §5's step 6 sharpens it, because the value written is the solver's output
after relaxation rather than the solver's output alone, so an external animator
has to be told which of the two it is reading.

**3. Idempotence and re-entrancy.** Named in `missing-devnotes-topics.md` and not
open in §11, which means it is simply unwritten rather than undecided.

**4. Animation-driven updates.** The boundary between what a solve decides and
what an animation is still moving. Phase 10 depends on this, and so does
`animate.md`'s "interaction with the solver's geometry".

**5. Activation ordering.** What order layouts are activated in when a solve
touches several, and whether that order is observable.

**6. The six open items of §11.** The priority bands are ordinal and the values
inside a band are not, which §3.2 and `omni_layout.h` already handle by naming
the constant `DOMINANT` rather than `REQUIRED`; the arranged-layout question is
item 2 above; the rest need enumerating against §11 at the time of the phase.

**7. `layoutengine.md` §3.2's false Mango attribution** for cluster behaviour,
carried from phase 00 if it was not done there, and `layoutengine.md` §7.7's
membership determinism question, which `tags.md` §9 also carries.

**8. The roadmap inversion.** Phase 08's first item is whether the scene graph has
to exist before decorations, and the answer changes what this phase owes
`decorate.md`. If the node model is written first, this phase has to state what a
node is, because that is the solver's output shape.

## What it resolves

- Whether an external animator can be written against the block, which is the
  question the project's API-driven premise turns on.
- The determinism claim, without which `testing.md`'s solver test (phase 01)
  cannot be a golden-file test.
- The client kind partition, once phase 06 closes the kind list.

## Why it is here

It consumes clients, tags and the store, so it is last of the compositor core. It
gates the whole look, because every visual phase animates a result this phase
produces.

## Done when

- A solve is scheduled or coalesced by a stated rule, and the rule is testable.
- Whether the block carries an arranged layout has an answer, and if it does,
  `configstorelayout.md` §11 says which of the two a reader is looking at.
- The determinism claim is stated, and `tags.md` §9 and §7.7 agree on it.
