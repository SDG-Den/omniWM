# Phase 07: `layoutengine.md` finalization

Stub. Not started. The document exists and is the most complete of the set: §11's
twenty-one items are all closed or deferred, so nothing is carried forward there,
and the questions that remain are in its body, in §3 and §4 and §7.

The stage is 6 and the priority is P0, and this is the phase where the design has
the most committed claims, so it is worth being explicit that the work is not
writing a solver. It is closing the eight items below and the eleven in the second
section, and it records four that phase 00 closed rather than reopening them.

## What it should contain

**1. Pass triggers in detail: when a solve is scheduled versus coalesced.**
`missing-devnotes-topics.md` names this first, and it is the one that decides
whether the solver runs on every event or once per frame. §5 has a pipeline; what
it does not have is a statement of the coalescing rule.

**2. The solved-to-arranged persistence rule.** Closed by phase 00 as **one
buffer, two stages**, and item 6 below records that; what is left to write is the
rule itself rather than the choice. The solved half is settled by
`OMNI_SECTION_SOLVED_LAYOUT`, so a script can already read a layout, and the
question this item now has to finish is what a reader is told when it opens the
section: which stage it is reading, and whether the geometry is the solver's
output alone or its output after relaxation, which is what §5's step 6 sharpens,
because an external animator has to be told which of the two it is reading.

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
decision `tags.md` §9 records, and §7.7 now points there rather than asking.

**8. The roadmap inversion.** Phase 08's first item is whether the scene graph has
to exist before decorations, and the answer changes what this phase owes
`decorate.md`. If the node model is written first, this phase has to state what a
node is, because that is the solver's output shape.

## The eleven questions in the body

`layoutengine.md` keeps its open questions in two places and the two do not
correspond. §11 is clear; the body is not. These eleven are the body's, they are
all part of the layoutengine design, and they are finalized here, in items 9 to 19.

| item | where | the question | what it is entangled with |
|---|---|---|---|
| 9 | §3.2 | whether total residual violation is reported as a diagnostic | nothing; §3.2 calls it a logging question and not a protocol one, so it cannot change the socket's error set |
| 10 | §3.3 | whether constraints may reference each other, and whether arithmetic on operands is expressible at all | the two halves are one question: if operands can be numbers rather than names, cross-references are a different problem |
| 11 | §3.3 | whether the viewport sits above the solver rather than inside it, and whether that survives contact with the solver | items 18 and 19; the separation is what they assume |
| 12 | §3.4 | which of the §8 built-in set ship at stage 6 | `README.md` stage 6 says a single demo layout, and the section argues a one-axis stack in a nested group is the cheapest genuine one |
| 13 | §3.4 | whether a layout name denotes a seeded key, an entry reference, or an `enum` constant | §3.5, which needs a name that means something per tag, and item 1's vocabulary, since `wm.cycle_layout` already passes one |
| 14 | §3.6 | of a client in two scopes, which of the two constraint sets the solver is responsible for | `tags.md` §8.2 answers the membership half, since the scratchpad withholds participation rather than membership; what is left is the solver's input set |
| 15 | §3.7 | what triggers a pass | `windows.md` §4 defers to it explicitly, and it is the trigger half of item 1 |
| 16 | §3.7 | when the first pass can run, before any client exists | the residue of §3.7's old activation-order bullet, whose band half was closed by §2.4 and §11 when the engine became a core service; distinct from item 5, which is the order layouts activate in when one solve touches several |
| 17 | §4.5 | whether a slot may carry a user-facing label beside its letter | nothing |
| 18 | §7.2 | the coordinate space behind the viewport | §7.2 says outright that this is the hard part; items 11 and 19 |
| 19 | §7.5 | whether a window's size is a fraction of the canvas or an absolute size, and whether the two can be mixed | item 18, since both are statements about the space the fraction is a fraction of |

Seven of the eleven are independent of each other. Two chains run through them:
item 13 blocks §3.5, and items 11, 18 and 19 are three statements about one
coordinate space and are cheapest decided together.

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
- All eleven questions in the body's table are decided in the document rather than
  flagged in it, which is the only way to tell a finished document from one whose
  §11 is finished.

## Reconciliation note, 2026-09-27

Phase 00 was verified against the documents rather than against its own record, and
this stub's preamble was one of the things that failed: it claimed "eleven closed
open-items and six still open" while item 6 said §11 carried no open item at all.
§11 has twenty-one items, all closed or deferred.

The substantive part of the same pass is that `layoutengine.md` carries open
questions in its body that no single section collects, so a reader checking §11
alone would conclude the document is finished. As of this note they are the eleven
in the table above, and this note is where the count was first derived: the pass
counted them section by section and the section list is §3.2, §3.3 twice, §3.4
twice, §3.6, §3.7 twice, §4.5, §7.2 and §7.5, which is eight sections and eleven
questions. The first draft of that count named §4.6 and §4.7, which are not where
the questions are.

The same pass closed two questions that were not on the list, because §4.7 pointed
at questions §3.5 and §3.6 had already answered: a tag names a layout and a monitor
derives one, and a group names one. `layoutengine.md` §4.7 now says so rather than
asking. It also closed the §3.2 determinism question, which was still headed "Still
open" over two bullets of which the first was struck through and the second said
the answer was a requirement, and it corrected the §1 status table, which still
described §3.4's mechanism as unstated after the section had stated it.

Re-entrancy is the one question deliberately outside both lists, and idempotence
is the one that is in this stub rather than in the document's body: it is item 3
above, and `layoutengine.md` §1 defers re-entrancy to phase 10.

**Ownership of the eleven was decided on 2026-09-27**, and it is the reason the
table sits in this stub rather than in the document. All eleven are part of the
layoutengine design and are finalized here. That is not a formality: the previous
state had this phase's preamble answering the ownership question by assertion,
with a count of six that was wrong, while the questions themselves were distributed
across eight sections of a document a reader is told to check at §11. An item with
no owner is not a smaller version of an unfinished phase, it is a phase that
reports itself finished, and the eleven were eleven instances of it.
