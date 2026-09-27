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

**6. The two §11 questions phase 00 closed, recorded here so this phase does not
reopen them.** *The priority bands* are closed as **one weight per solved family**,
`OMNI_SOLVER_WEIGHT_PROGRAM` 60000 and `OMNI_SOLVER_WEIGHT_CLIENT_RULE` 6000, with
the four named bands deleted rather than renamed. *The arranged-layout question* is
item 2 above and is closed as **one buffer, two stages**, a `stage` byte in
`configstorelayout.md` §11's header rather than a second section. `layoutengine.md`
§11 now carries no open item, so the part of this that was going to need
enumerating against §11 no longer exists.

**7. The two §7 questions phase 00 closed.** *§3.2's Mango attribution for cluster
behaviour* is corrected: §3.2 named no Mango behaviour, and the derivation runs the
other way, from our group plus chrome to a Mango-style group (`layoutengine.md`
§7.1). *§7.7's membership determinism question* is closed as **stored**, by the same
decision `tags.md` §9 records, and §7.7 points there rather than asking.

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
