# Phase 00: reconciliation of documents that exist

**Complete.** All ten items are decided and applied; see "What this phase decided"
below and the work log at the end. Nothing here is outstanding.

This phase began as a reconciliation of twelve documents carrying an open,
deferred or not-covered section, eleven of which carried something genuinely open.
It closes those before any new document is written, because every later phase reads
them and a later phase written against an unsettled document has to be revised.

Nothing here is new design. It is finishing decisions that were started and left
half-made, and finding the places where two documents disagree.

The per-document inventory this phase worked from is the table in
[README.md](README.md). The list below is not a summary of that table; it is the
subset that needed a decision rather than a correction, in the order it blocked the
most.

## What the first pass closed

Every item here was a stale statement rather than a question, and each is
corrected in the document that carries it.

- **`include/shared/omni_layout.h` §11's fallback comment.** It documented a
  reader that "degrades `get` to a catalog walk" when it cannot validate the
  `CATALOG_INDEX` row, which is the design `configstorelayout.md` §3 reversed
  and §3's own `BLOCK_UNSUPPORTED` line replaced. The comment now states the
  refusal and records that its result code is undefined.
- **`configstorelayout.md` §5's four-records wording.** The heading called all
  four composite tags "a fixed-shape header" and the text said "Four tags carry
  a fixed-size record", four lines before the same section said two of them are
  not fixed-shape. The section is now titled for composite records and states
  the actual split: one fixed-size, one fixed header with a variable tail, two
  variable-length. `configstorage.md` §4's "Three payloads are shaped rather
  than scalar" is now four, which is what the section then describes.
- **`windows.md` §8's cluster correction.** It accused `layoutengine.md` §3.2 of
  attributing the cluster arrangement to Mango. §3.2 names no Mango behaviour
  at all; the attribution was already gone. The correction now says so and points
  at §7.1, which derives a Mango-style group from our group plus chrome rather
  than the other way round. `layoutengine.md` §9's reference table said
  `havel`, a project that does not exist, and now says halley and keeps the
  mis-spelling visible in the same cell.
- **`action_ref`, restated as open in two places.** `helpers.md` §6.2 settles it
  as a frame offset holding the action's name, `ipc.md` §8 and `helpers.md` §11
  both still reported a contradiction with it. Both are corrected, and
  `helpers.md` §11's residual "what the port still owes this document" clause
  goes with them.
- **The modmask grammar and keysym names, deferred in `ipc.md` §8.** They are not
  deferred: `generaldesign.md` §14 brings them in with the port, and
  `helpers.md` §11 already records that closure.
- **Four cross-references that pointed at the wrong section.**
  `configstorelayout.md` §5 pointed at §6 for the guard tiers, which are §12,
  and §1 called the invariant rows numbered, which they are not;
  `configstorage.md` §12 pointed at `configstorelayout.md` §11 for the invariant
  table, which is §12; `generaldesign.md` §14.1 pointed at §14.2 for the
  namespace mapping, which is §14.4, and did not say that the mapping is still
  open.

## What this phase decided

Every item below was answered by the owner and applied. The section is a record of
the decisions, not a work list; a reader looking for outstanding work should read
the *Done when* list and the work log at the end, and this list will not grow
again.

**1. The result vocabulary. Closed: one `OMNI_ERR_*` set, plus `OMNI_SOCK_ERR_*`
for wrapper-only facts.** `omni_layout.h` §12 defines the single set the read path,
the request queue and the socket share, and `configstorelayout.md` §14's bullet is
struck. Two sub-problems went with it, and both were prerequisites rather than
detail: `CATALOG_GENERATION` is gone, so there is no fifth vocabulary leaking from
`configstorage.md` §12.3, and the refusal condition now has a field to test —
`OMNI_CATALOG_INDEX_HDR_OFF_VERSION` with `OMNI_CATALOG_INDEX_VERSION` = 1, which
is what the index's own header row carries. The `get` signature is
`OMNI_ERR_* omni_get(omni_block *, const char *name, omni_value *out)`, because a
function's out-parameter cannot fail to be delivered and therefore needs no slot
and no lifecycle.

**2. The invariant table and the guard tiers. Closed: four tiers, 45 rows, one tier
per row.** The `Semantic` tier is deleted outright rather than given rows, because
a semantic check is not a guard — a guard answers whether bytes are readable as
what they claim to be, and "is this value sensible" belongs to whoever uses it. The
two compound rows are split, so no row joins two unrelated comparisons. Three
things had no row at all and now do: the **arena free-list** is checked for
acyclicity, for termination on `OMNI_REF_NONE`, and for every frame on it lying
inside the arena and being unreachable from a live `body_ref`; the **`ready` axis**
is checked for being one of its four values and for reaching `READY` and `DEGRADED`
only in a commit that sets the matching `state`; and the **catalog name index** is
checked for ascending sort order, for an `entry_count` equal to the live slot
count, and for every row resolving to a live slot whose name hashes to the row's
own value. That last one is the row the index most needed, because an index that
is internally sorted but describes a deleted entry is the failure the read path
cannot detect on its own.

**3. The three-key layout program. Closed, and the three keys are now named in both
documents.** `.viewport` is parsed and is not optional in the sense of being
invented later; a program with `rules` and `spaces` and no `viewport` gets the
static identity viewport rather than being refused, which is the same
missing-means-default rule as `rearrange_on_focus`. Whole-program `constraint`
framing exists, so the tree change `layoutengine.md` §11 records is visible to the
parser instead of being a documentation-only change. Nested layouts have an
authoring rule, partial-program deletion is specified for the case where only one
of the three keys is present, and key-shape guards exist. `tomlparser.md` §2's
instruction to nest spaces is now what §6 does. And `layoutengine.md` §3.1's
"space table, mapping rules, constraint records" is reconciled with §4.4's `rules`,
`spaces`, `viewport`, so the two lists name the same three things.

**4. Creation-time header initialization. Closed: a value and a reason for every
field.** `configstorage.md` §13's table states all of them, and two rows are
defended in prose because they are the ones a "memset it and fill in the obvious
fields" implementation gets wrong. `state` starts at `CREATING`, not `READY`,
because the window between the `ftruncate` and the seeding commit is real and a
block published `READY` with an empty catalog is one a reader will accept and
conclude from. And `catalog_free_head` does **not** start at `OMNI_REF_NONE`: a
fresh catalog is a *full* freelist rather than an empty one, so the creator seeds
a low run for the core `wm.*` keys and links the entire unseeded tail through each
slot's `name_ref`, with `catalog_free_count` carrying the honest count. The
asymmetry against `arena_free_head` and `region_head`, which do get the sentinel,
is not an inconsistency — those two lists are genuinely empty at creation and the
catalog is the one list that is not.

**5. `layoutengine.md` §11's four open items. All four closed, one by deferral with
its owner named.** Whether an *arranged* layout is written beside the solved one:
**one buffer, two stages** — a `stage` byte in `configstorelayout.md` §11's header
distinguishes `SOLVED` from `ARRANGED` rather than a second section, because the
two are sequential and a second 32KB section would cost capacity to hold an
intermediate nobody reads. What a second update does mid-animation: **deferred to
phase 10**, which already owns it and where the scratchpad is now a fourth case.
Whether the engine is a component or a borrowed service: **a core service**, which
was a conflict between two halves of one bullet rather than a real choice. And the
priority band values: **one weight per solved family**,
`OMNI_SOLVER_WEIGHT_PROGRAM` 60000 and `OMNI_SOLVER_WEIGHT_CLIENT_RULE` 6000, a
decade apart so one dominates the compromise, with the four named bands deleted
rather than renamed because three of them had no caller.

**6. `windows.md` §13's five open decisions. Four closed, one deferred with its
shape fixed.**

- *Membership walk order.* **Ascending `entry_id`, as the design and not a
  prototype choice.** Total, stable across restarts, cheap on the solve's hot path,
  and free of insertion history, which is the property that makes two clients
  reaching the same set walk identically. The scattered-walk worry is accepted: the
  walk is over one tag's membership children, so the scattered part is bounded by
  how many clients a tag holds.
- *Whether a focus change causes a pass.* **Per layout**, as a program-level
  `rearrange_on_focus` defaulting to `true`. Mango's conditional re-arrange is the
  evidence for making the trigger per layout rather than global, and the default
  follows from which case fails silently: `stack` with `raise` breaks without it,
  while the layouts that do not need it merely waste a pass.
- *Whether a fake client can take the keyboard focus.* **Participation is yes;
  focusability is a separate per-client property defaulting to non-focusable.** The
  default is deferred to phase 06, which is the phase with the input and focus
  design needed to say whether any kind should invert it.
- *Whether the client kind set is closed.* **Closed at seven.** Mango's six protocol
  kinds are kept because they describe what a client talks to the compositor about,
  `GroupBar` is dropped because ours is a placed participant rather than chrome, and
  `FAKE_CLIENT` is ours.
- *What a cluster contributes beyond moving as one.* **Nothing.** A cluster
  constrains geometry and grouping only: members stay individually focusable and
  individually interactive, they do not raise as a unit, and they are not one focus
  target. A group is the other thing, and the difference is now stated rather than
  inferred from Mango, which has no cluster to compare against.

One arrangement decision went with it because it follows from the focus answer: a
`group` raises its members as a unit on focus, a `cluster` does not, and a client
in both raises as the group.

**7. `helpers.md` §11's `omni_event` payload. Closed: the field set is final and is
not extended.** The struct mirrors one journal slot and stops there. A trigger *is* a
view of the journal, so there is nothing richer to carry; a component needing more
reads the block. A synthetic event is deliberately not expressible, because it would
let a dispatch deliver something no commit produced, and that is the property worth
keeping. The two questions the bullet used to carry are answered without a new field:
`COMMIT_END` is enforced by the dispatcher before delivery, and a custom event
arrives as an ordinary match.

**8. `tags.md` §9's open items. All closed, and the model changed underneath them.**

- *Stored or derived membership.* **Stored on the tag**, as one `WINDOW_DEPENDENT`
  child per member, against derivation. `layoutengine.md` §3.2's determinism
  requirement is the reason: a derived membership is a `HashSet`-shaped iteration
  order and therefore not reproducible, which would make a golden-file solver test
  impossible.
- *Whether a tag's entry is a container or a value.* **A container.** §5 makes it
  one, §7 makes a swap an exchange of two containers, and the three store operations
  that follow are now *required* in `configstorage.md` §14.1 rather than deferred:
  subtree delete, subtree exchange, generation-correct deletion.
- *The surface form of the swap.* **Two full keypaths**, `a` and `b`, in
  `ipc.md` §4's `swap_tags`, reusing the one addressing rule the rest of the protocol
  already has.

The larger change was not on the list and was decided with it: a per-monitor tag's
monitor is now **part of its identity**, stored at `wm.monitor.<id>.tag.<n>` with
the monitor's ordered list at `wm.monitor.<id>.tags`. Mirroring was reframed from a
given-up feature into the protocol violation this makes unrepresentable, since a
client expects one surface on one output, and Mango's per-monitor tag-numbered
state is what the decision keeps. The display invariant is **at most** one monitor,
not exactly one, which is what allows the scratchpad as an exclusive tag of its own
with its own overlay layout, `wm.scratchpad.shared` (`bool`, default `true`),
per-monitor or shared, with a shared one open on at most one monitor. A singleton
tag has no monitor in its identity and sits at `wm.tag.<name>`; the shape carries
the distinction rather than a flag, and §5 spells out how the setting and the tag
avoid sharing a prefix. Hotplug is therefore not the reverse of a tag move, which
`04-monitor.md` had assumed, and migrating a disappearing monitor's clients is a
sequence of swaps rather than a move.

**9. The wholesale-port wording. Closed.** The owner supplied the replacement
sentence and it is now the policy in `generaldesign.md` §14: **we use Mango's code
where we can, and change what we need.** The word "wholesale" is gone from every
place that asserted a completeness nobody had checked. It is retained in exactly two
places, both of which are about the word rather than the decision: item 9 above,
which is the record of the correction, and this acceptance criterion, which can only
be evaluated by searching for it. §14 now also states *why* the substitution was
worth making rather than treating it as a synonym: "wholesale" told a reader there
was nothing to audit, and the three documented seams are what a reader should audit
against instead.

**10. The `xref.py` and `lcheck.py` gate. Closed by removal.** Neither script
exists anywhere in the repository, so the criterion could never be run and its
presence was worse than its absence: it named a tool that a future contributor
would go looking for. The criterion is replaced with the two checks that can
actually be run, and the one `windows.md` line that cited `lcheck.py` as a reason
not to validate a convention is rewritten to give the reason on its own terms.

## What it resolves

- The catalog is described one way in one document and another way in another,
  which is the class of defect that costs a week later.
- The guard tiers become assignable, so `testing.md` (phase 01) has an assertion
  set to write against rather than an argument about.
- `get` gains a failure vocabulary, so `configstorage.md` §3.1 becomes
  implementable.
- Every later phase reads settled documents.

## Why it is first

It is unglamorous and it blocks everything. Six of the ten remaining documents
cite `omni_layout.h` constants, and all of them cite `configstorage.md` §4 for
what a record means. Writing `monitor.md` against an unsettled `tomlparser.md`
means revising it, and writing `testing.md` against unassigned guard tiers means
inventing an assertion set that phase 00 then contradicts.

## Done when

All six met. Each is a check that can be run rather than a judgement, and the last
three were run after the final edit.

- **Every open-items section in the twelve documents has either a closed decision
  or a named owner.** Met. The five that were genuinely unowned at the end are
  `input.md` §7.2's device-rule type tag and its `isallowconflict` test. The first
  two name phase 02 and phase 01 respectively, and the last two are external facts
  rather than design questions and belong to phase 02's build half. Nothing is
  left unowned: the fifth, which had been a genuine gap, was the Lua deliverable,
  and the owner answered it — no interpreter and no library is a deliverable of the
  window manager at all, since they are separate example programs written after the
  project is complete to prove the surface is generic, which is why they are last on
  the roadmap. `generaldesign.md` §17 and §19, and `12-languages.md` item 6.
- **The guard table has one tier per row and no guard without a row.** Met: four
  tiers, 45 rows, one tier per row, and no `Semantic` tier at all.
- **A grep for "wholesale" returns no place that *asserts* a port was taken
  unchanged.** Met. It returns this file's item 9, this criterion, the work log,
  and two sentences that exist to explain the substitution: `generaldesign.md` §14,
  which says the old word told a reader there was nothing to audit, and
  `design-phases/11-protocols.md` item 1, which records that its own heading used
  the word. Those are about the wording; none of them is a claim about the code.
- **A grep for `xref.py` and `lcheck.py` returns only this item and this
  criterion.** Met. Neither script is referenced as a thing to run anywhere in the
  repository.
- **The header compiles and every `OMNI_STATIC_ASSERT` passes.** Met, under
  `-std=c11 -Wall -Wextra -Iinclude`.
- **`git diff --check` is clean.** Met, after fixing one line of trailing
  whitespace that a previous pass in this session had introduced in
  `filestructure.md`.

**What is deliberately not in this list.** Whether phase 00's ten decisions are
*right* is not a check this phase can run, and neither is whether the documents
read well. Both are the reader's judgement, and a phase that claimed to have
verified them would be claiming something it cannot know.

## Work log

What actually changed, in the order it was done, and what each step revealed. The
items are the plan's; the notes under them are what the documents turned out to
need, which is not always the same thing.

**1-5, the substrate items.** The error vocabulary went into
`include/shared/omni_layout.h` as one `OMNI_ERR_*` set with
`OMNI_SOCK_ERR_*` beside it for wrapper-only facts, and the direct `get` signature
is `OMNI_ERR_* omni_get(omni_block *, const char *name, omni_value *out)`: no slot,
no ticket, no lifecycle, because a function's out-parameter cannot fail to be
delivered. The guard bundle was restructured into four ordered tiers, L1 header
refuse, L2 section, L3 slot skip, L4 value skip, and the value of the split only
appeared once L2 was written: whether a section is *absent by design* or *broken* is
read off `PRESENT` and `commit_state`, so the tier needs no required/optional list
of its own. A block is only dropped at L2 when a growth commit is in flight, which
is why a short block at rest refuses and the same short block mid-growth does not.

**Semantic validation moved to its consumers.** This was not an item and it turned
out to be the largest single change. `configstorage.md` §12 had accumulated
semantic rules that the structural guard cannot evaluate, and a guard that is
sometimes evaluable and sometimes not is not a guard. `layoutlanguage.md` and
`layoutengine.md` now each state the validation their own records need, and §12 is
down to questions about bytes.

**6, windows.** All five §13 decisions are closed except the per-client focusability
default, which is deferred to phase 06 with the shape fixed, because the default is
what nearly every client gets and therefore worth deciding deliberately. A `group`
raises as a unit and a `cluster` does not, and both documents now say so instead of
leaving it to be read off Mango.

**7, `omni_event`.** Closed with no new field, which is the right outcome for a
question whose honest answer was that a trigger is already a view of the journal.
`COMMIT_END` is a dispatcher concern and a custom event is an ordinary match, so
the two sub-questions dissolved rather than being answered.

**8, tags.** The largest item, and the one that stopped being a reconciliation. The
per-monitor identity decision changed the model rather than the wording, and it
invalidated four documents that had been written against the old one — including
this phase's own `04-monitor.md` and `05-tags.md` stubs, which described a handoff
in the opposite direction. Two contradictions were found and fixed while applying
it: `tags.md` §9 still listed the stored-or-derived question as open after §5 had
answered it, and still said the swap's keypath arguments were undecided after
`ipc.md` §4 had written them. §5 also promised in §3.1 that a singleton's storage
would be stated and never said it, so `wm.tag.<name>` is now specified, with the
reason the `wm.scratchpad.shared` setting is deliberately not under `wm.tag.`.

**9-10, wording and tools.** `generaldesign.md` §14 carries the replacement
sentence. The `xref.py` and `lcheck.py` criterion is gone rather than restated,
since it named tools that do not exist in the repository.

**Consistency defects found while applying the above, none of which were in the
plan.** Four, each a real disagreement between two documents:

- The catalog freelist was described as chained through `name_ref` while the
  creator seeds a low run and chains the remainder, and `catalog_free_count` was
  written as zero. Fresh slots are chained, the count is nonzero, and a `DESTROYED`
  slot that was never allocated has its name cleared rather than left stale.
- The solver weight comment and its values disagreed: 60000 is `PROGRAM` and
  6000 is `CLIENT_RULE`, so the header's mapping was inverted relative to the text
  above it, and the text did not say what a decade ratio actually buys. It buys
  domination of the compromise: two equal client rules compromise, and one program
  rule beats them all.
- `solver_stage` was given `PRESENT` and `commit_state` in the vocabulary but the
  text used neither, so L2's two `PRESENT` cases were asserted with nothing
  asserting them.
- `README.md`'s inventory table counted open items per document and was wrong for
  five of the eleven rows after the edits above. It now records where the open
  items actually are, and the honest answer is that there are two documents left
  with one question between them, which is now closed as well.

**A fifth, found last and the only one that changed a decision rather than a
sentence.** Three documents disagreed about what "exclusive" means for the
scratchpad. `generaldesign.md` §6 said the special tags "hold clients that are also
on ordinary tags"; `tags.md` §8.2 said a client in the scratchpad "is not a member
of any arrangement input"; and `windows.md` §5 said focusing a scratchpad client
deletes its membership child and clears a visibility flag, which `tags.md` §8 had
already corrected. The strong reading of *exclusive* is also the unimplementable
one: if a client's ordinary membership is deleted when it is parked, dismissal has
to restore it from somewhere, and Mango keeps that somewhere as `oldtags` — a
second copy of the truth, which is the hidden state this design exists to avoid.
So exclusivity is now stated as being about **arrangement rather than membership**:
a client holding scratchpad membership keeps its ordinary tags and is skipped by
every layout but the scratchpad's, which makes hiding an *addition* of a tag rather
than a move, and dismissal a single membership deletion with nothing to restore.
This also removes the two-write focus path, so `windows.md` §5's "ejection" is gone
and the operation that actually mutates membership is the one `swap_tags` serves in
bulk. `tags.md` §8.2, §8.4 and `windows.md` §5 now say the same thing, and §8.4 is
a new subsection because the three operations — focus, hide, minimise — were sharing
one paragraph with no heading to point at.

Three dangling cross-references were also fixed, all of them pre-existing and none
of them load-bearing: `layoutengine.md` §5.4 does not exist and the weight constants
are in §3.2, `input.md` has no §10 and its device rules are in §4 and §5, and
`input.md` §14.2 was cited twice as a worked example of the private-cache pattern
that does not exist — `input.md` has eight sections and describes no cache. The
second one mattered more than the other two, because both `scaffolding.md` and
`02-substrate.md` were pointing at a worked example as the reason a pattern was
worth writing down, and there was no example. The pattern is now stated as two
specified halves with no instance between them, and `input.md` is named as the
candidate rather than cited as if it were already one.

**Phase stubs updated for the decisions that changed their premises.** `05-tags.md`
gains the tag-model re-read and the three store operations. `06-windows.md` learns
that four of its five decisions are already closed and keeps the one that is not.
`04-monitor.md` has its handoff reversed and its hotplug item re-framed, since a tag
can no longer move between monitors. `10-animate.md` gains the scratchpad as a
fourth case, because a client appearing has no previous geometry to interpolate from
and none of retarget, queue, or replace has anything to say about it.
`01-testing.md` and `02-substrate.md` gain the three container operations, with the
subtree exchange treated as its own operation rather than a reuse of the delete.

**Verification.** The header compiles under `-std=c11 -Wall -Wextra` and every
`OMNI_STATIC_ASSERT` passes. `git diff --check` is clean. A grep for `wholesale`
returns only this file's item 9, its acceptance criterion, and the two sentences
that exist to explain the substitution. A grep for `xref.py` and `lcheck.py` returns
only item 10 and its criterion. Nothing is committed.
