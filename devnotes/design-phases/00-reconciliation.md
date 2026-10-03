# Phase 00: reconciliation of documents that exist

**Complete.** All ten items are decided and applied; see "What this phase decided"
below and the work log at the end. Nothing here is outstanding. Most of this file
is work log, and a work log is not maintained and is not to be reconciled against
anything: see [README.md](README.md)'s note on that. What matters is the two
sections above the log, and the log's own conclusion, which is that phase 00 is
done.

This phase began as a reconciliation of twelve documents carrying an open,
deferred or not-covered section, eleven of which carried something genuinely open.
The twelve are the eleven rows of [README.md](README.md)'s inventory plus
`audits/architecture-audit.md`, which predates the inventory, is not one of its
rows, and states on its own first page that it is not a readiness gate — it is
counted here only because it carries an "Out of scope, deferred" section.
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

- **`include/shared/omni_layout.h` §4's fallback comment.** It documented a
  reader that "degrades `get` to a catalog walk" when it cannot validate the
  `CATALOG_INDEX` row, which is the design `configstorelayout.md` §3 reversed
  and §3's own `BLOCK_UNSUPPORTED` line replaced. The comment now states the
  refusal and names its result code, `OMNI_ERR_BLOCK_UNSUPPORTED`.
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

**2. The invariant table and the guard tiers. Closed: four structural tiers plus a
fifth replay tier, 51 rows, one tier per row.** The `Semantic` tier is deleted
outright rather than given rows, because
a semantic check is not a guard — a guard answers whether bytes are readable as
what they claim to be, and "is this value sensible" belongs to whoever uses it. The
two compound rows are split, so no row joins two unrelated comparisons. Three
things had no row at all and now do: the **arena free-list** is checked for
acyclicity, for termination on `OMNI_REF_NONE`, and for every frame on it lying
inside the arena and being unreachable from a live `body_ref`; the **`ready` axis**
is checked for being one of its four values and for reaching `READY` and `DEGRADED`
only in a commit that sets the matching `state`; and the **catalog name index** is
checked for ascending sort order, for a `live_count` equal to the live slot
count, and for every row resolving to a live slot whose name hashes to the row's
own value. That last one is the row the index most needed, because an index that
is internally sorted but describes a deleted entry is the failure the read path
cannot detect on its own.

The row count is 51, not the 45 an earlier draft of this paragraph claimed, and the
tier count is five labels rather than four: `configstorage.md` §12 describes four
ordered structural tiers and then calls the replay tier a fifth, and
`configstorelayout.md` §12's table carries one `R` row per check. The distribution
is L1 10, L2 10, L3 17, L4 10, R 4. The two rows the third pass added are the
`state` and `commit_state` value sets, which is the finding recorded at the end of
this file; the earlier figure of 49 was correct when it was written.

**3. The three-key layout program. Closed, and the three keys are now named in both
documents.** `.viewport` is parsed and is not optional in the sense of being
invented later; a program with `rules` and `spaces` and no `viewport` gets the
static identity viewport rather than being refused, which is the same
missing-means-default rule as `rearrange_on_focus`. Whole-program `constraint`
framing exists, so the tree change `layoutengine.md` §11 records is visible to the
parser instead of being a documentation-only change. Nested layouts have an
authoring rule, partial-program deletion is specified for the case where only one
of the three keys is present, and key-shape guards exist. `tomlparser.md` §2's
exception to the rule in its own §6 is now written down in both places, so a
`binding`, a `constraint` program, a `client_rule` set and a window `map` all bind
to a named-field composite tag instead of to a positional `tuple`, and `.viewport`
appears in the example. And `layoutengine.md` §3.1's "space table, mapping rules,
constraint records" is reconciled with §4.4's `rules`, `spaces`, `viewport`, so the
two lists name the same three things; §3.1 and §3.3 no longer describe a 16-byte
record.

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
sentence and it is now the policy in `generaldesign.md` §14: **we use MangoWM's code
where we can, and change what we need.** The word "wholesale" is gone from every
place that asserted a completeness nobody had checked. It survives in five places,
all of which are about the word rather than the decision: this item, this
phase's acceptance criterion, the work log, and two sentences that exist to
explain the substitution — `generaldesign.md` §14, which says the old word told a
reader there was nothing to audit, and `design-phases/11-protocols.md` item 1, which
records that its own heading used the word. §14 now also states *why* the
substitution was
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

It is unglamorous and it blocks everything. Every one of the eleven inventory
documents above is written against `omni_layout.h`'s constants and
`configstorage.md` §4's definitions of what a record means, and so is every
document a later phase writes: an unsettled one is a document that has to be
revised, and `monitor.md` against an unsettled `tomlparser.md` is a rewrite, while
`testing.md` against unassigned guard tiers means inventing an assertion set that
phase 00 then contradicts.

## Done when

All six met. Each is a check that can be run rather than a judgement, and the last
three were run after the final edit.

- **Every open-items section in the twelve documents has either a closed decision
  or a named owner.** Met. The twelve are the eleven rows of
  `design-phases/README.md`'s inventory plus `audits/architecture-audit.md`. The
  two that were genuinely unowned at the end are `input.md` §7.2's device-rule
  type tag and its `isallowconflict` test. Both are named, and the second has
  since moved again: it was **phase 01 item 5**, and it is now `03-input.md`
  item 5, because the ordering rule turned out to be last-match-in-config-order
  -wins with `isallowconflict` not carried, and the test has to follow the
  resolution walk that phase 03 designs rather than a framework phase guessing at
  it. The criterion is met by a decision plus an owner either way; what changed
  is which phase holds the test. The first was named
  twice and to two different phases, which was an ownership conflict rather than a
  gap, and it is now resolved rather than averaged: `input.md` §7.2 and
  `03-input.md` item 2 agree that **the number is chosen in phase 03, when the
  input design is fleshed out**, because the device rule's record shape is only
  knowable once the rest of input is, so `configstorelayout.md` §4's vocabulary and
  the static assert follow this phase's work rather than leading it. The item that
  had been a genuine gap was the Lua deliverable,
  and the owner answered it — no interpreter and no library is a deliverable of the
  window manager at all, since they are separate example programs written after the
  project is complete to prove the surface is generic, which is why they are last on
  the roadmap. `generaldesign.md` §17 and §19, and `12-languages.md` item 6.
  The `input.md` §7.2 `spec` field that was named but unplaced is now gone rather
  than placed: `generaldesign.md` §14.1 drops it beside `line_number` and
  `file_index`, because it is a shim over a storage model that could not hand a
  binding back over its IPC. A binding here is a catalog entry that the general
  `get` already returns, so there is no field to place and no `get binds` to want.
- **The guard table has one tier per row and no guard without a row.** Met: four
  structural tiers and one replay tier, 51 rows, one tier per row, and no `Semantic`
  tier at all. `configstorage.md` §12's L1 bullet states `magic`,
  `format_version` and the section constants as preconditions on mapping rather
  than as tier invariants, which is the framing `configstorelayout.md` §2 already
  used, and the three value words the L1 bullet enumerates each have a row.
- **A grep for "wholesale" returns no place that *asserts* a port was taken
  unchanged.** Met. The word occurs in three documents, and every occurrence is
  about the wording rather than about the port: this file, `generaldesign.md` §14,
  which says the old word told a reader there was nothing to audit, and
  `design-phases/11-protocols.md` item 1, which records that its own heading used
  the word. None of them is a claim about the code. This criterion is stated by
  document rather than by line count deliberately: every paragraph in this file
  that discusses the grep adds a line to it, so a count is a number this file can
  invalidate by mentioning the thing it counts. Naming the three documents cannot
  change.
- **A grep for `xref.py` and `lcheck.py` finds no document that names either script
  as a thing to run.** Met. **Neither name occurs in any core design file.** The
  core design files are the `devnotes` documents and the `design-phases` stubs,
  which is where a reader would go to find a tool worth running; this file is the
  reconciliation record rather than a design document, and the audits under
  `devnotes/audits` are historical records of documents that no longer exist.
  Naming the files that mention the scripts is the check, not counting the lines
  that do: a count is a number this file invalidates by mentioning the thing it
  counts, which is what the first audit's removed criterion did and what this
  criterion's own parenthetical did in the third pass.
- **The header compiles and every `OMNI_STATIC_ASSERT` passes.** Met. Verified
  under `-std=c11 -Wall -Wextra` with no output, which covers all 79
  `OMNI_STATIC_ASSERT`s and `OMNI_CAP_DEFAULT == 0x1FF`. The property is stated
  here and the invocation lives once, in `build.md` §1, because the three copies
  of the command this file carried had each drifted into a form that could not be
  run in this environment at all.
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
  written as zero. Fresh slots are chained, the count is nonzero, and a
  never-allocated slot has `DESTROYED` deliberately clear rather than set, since
  `DESTROYED` is a statement about a name that existed and was deleted and
  `entry_generation` = 0 is what distinguishes that from a slot freed after use.
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
  items actually are, and the honest answer is that no document listed in it is
  left with an open item.

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

**Verification.** The header compiles under `-std=c11 -Wall -Wextra`, per the
invocation in `build.md` §1, and every `OMNI_STATIC_ASSERT` passes. `git diff --check` is clean. A grep for `wholesale`
finds the word in this file, in `generaldesign.md` §14, and in
`design-phases/11-protocols.md` item 1, and all three are about the wording rather
than about the port. A grep for `xref.py` and `lcheck.py` finds both names only in
this file. This phase is committed as `0151ed8`.

## Post-completion audit

This phase was committed on the strength of its own acceptance criteria. A later
pass went back and checked each claim against the documents rather than against the
record of having made it. The record was wrong in more places than the design was,
which is the opposite of the usual outcome and worth writing down before the
next phase does the same thing.

**The phase's own record was inaccurate in eleven places.** The guard table has 49
rows, not the 45 stated in item 2 and in the acceptance criterion, and it carries
five tier labels rather than four: `configstorage.md` §12 describes four ordered
structural tiers and then calls replay a fifth, and the distribution is L1 8, L2 10,
L3 17, L4 10, R 4. The catalog-index refusal comment is in `omni_layout.h` §4,
not §11 as the first-pass list says, and it still said the result code was
undefined and an open item in `configstorelayout.md` §14, which item 1 had already
closed. `tomlparser.md` §2's claim that its nesting instruction "is now what §6
does" pointed at a section that does the opposite, and §6 was still missing the
exception §2 states. The `xref.py` criterion promised two lines and returns five,
all in this file, which is the same defect class as the `wholesale` one this phase
was correcting; item 9's "exactly two places" was five. `entry_count` is not a
field, the index header carries `live_count`. "Nothing is committed" was untrue,
this file is in `0151ed8`. The twelve documents are the eleven inventory rows plus
`audits/architecture-audit.md`, which says on its first page that it is not a
readiness gate. "Six of the ten remaining documents cite `omni_layout.h`
constants" was not checkable, because nine of those ten documents do not exist. And
the `scaffolding.md` work-log entry said `input.md` §14.2 had been cited twice and
both citations fixed, when one of the two was still there, contradicting a line in
the same file.

**Six contradictions survived between design documents.** The worst was the one
item 3 claimed to have fixed: `layoutengine.md` §3.1 and §3.3 still described
`OMNI_TAG_CONSTRAINT` as a 16-byte record, against `layoutlanguage.md` §9,
`configstorage.md` §4, `configstorelayout.md` §5, the header, and `layoutengine.md`
§11. `omni_layout.h` defined the arena frame header twice, §2 as `next_free` and
`free_size` and §5 as `RESERVED32` and `RESERVED64`, so the free-list
discriminator the guard design rests on lived in the block another section called
reserved. `configstorage.md` §12.3 put the free-list link in the first word of the
payload when it is the second word of the header, ahead of the payload.
`configstorelayout.md` §3 listed a ninth capability bit the header did not define
while §4 omitted the eighth section row the header did define, and the reason is
structural rather than clerical: eight capability bits cover seven sections plus
the socket facade, so the catalog index had a row and no bit.
`missing-devnotes-topics.md` said the catalog name index "degrades to a walk rather
than failing if its section row does not validate", which reverses the no-fallback
decision in the same repository. And `generaldesign.md` §6 still carried, verbatim,
the exclusivity sentence this file's work log cites as the wrong reading of the
scratchpad.

**Four things the first pass had not looked for.** The no-fallback contradiction
above, because it is in a document whose subject is what is *missing* rather than
what exists. `07-layoutengine.md`'s items 6 and 7, which restate two of this
phase's closed decisions as open in the same way `05-tags.md` did, and which
surfaced only when the stubs were grepped as a set rather than one at a time.
`tomlparser.md`'s example, which omitted `.viewport` from a program it had just
declared to have three structural keys. And the fact that the header's
`OMNI_CAP_DEFAULT` and `configstorelayout.md`'s bit list were describing different
numbers of sections without either document noticing, which is what turned a
clerical disagreement into the question below.

**Why the acceptance criteria did not catch any of it.** Four of the six are checks
on this file rather than on the design: the ownership criterion, both greps, and
`git diff --check`. The fifth is a compile, and the header compiled throughout,
including with the frame header defined two contradictory ways. The sixth, the guard
table, was a check on the design but stated its own number, and a criterion that
supplies the number it is checking cannot fail. Not one of the six read two
documents and compared them, which is the only thing that would have found any of
the six contradictions. The lesson for the next phase is that "Done when" has to
name the comparison, not the conclusion: a criterion like "`configstorelayout.md`
§3's bit count equals the `OMNI_CAP_*` defines in the header" is checkable, and
"the guard table has one tier per row" is not, because the row count was the thing
in doubt.

**Three questions the audit raised that were not the audit's to answer.** The audit
found the capability-bit gap, the clock disagreement on `boot_time_ns`, and
`input.md` §7.2's `spec` field assigned to an `ipc.md` surface that does not exist.
All three went to the owner rather than being resolved by picking the answer that
looked more consistent.

*Capability bit 8.* **The bit exists**, so `omni_layout.h` gained
`OMNI_CAP_HAS_CATALOG_INDEX (UINT64_C(1) << 8)` and `OMNI_CAP_DEFAULT` moved to
`0x1FF`. It is now built as `OMNI_CAP_ALL_BITS`, the union of every bit the header
defines, and a static assert holds it at `0x1FF`. That assert is the part worth
keeping: defining a bit without widening the default was the original defect, and
it is now a build failure rather than a disagreement between a header and a
document. The header also records that the bits are not offset from the section ids
by a fixed amount, since the socket's bit has no row, so nobody tries to derive one
table from the other.

*`boot_time_ns`.* **`CLOCK_MONOTONIC`.** The field is a witness of when the creator
started, so a reader can tell a block written by this instance from one written by
a previous one; both sides read the same clock, so the comparison is a subtraction
rather than a conversion. The stated reason in `configstorage.md` had not
discriminated either way, since a recreate writes a fresh value and moves forward.
`CLOCK_MONOTONIC` is already the design's stored-timestamp clock,
`configstorelayout.md` §9's `terminal_at_ms` and `ipc.md` §4's entry `time` both,
so this field is not the one exception that has to remember which clock it meant.

*The `spec` field.* **Dropped, and the question went with it.** It was assigned to
an absent `get binds` because the reachability of a binding looked like a separate
question from the binding's storage, and it is not: a binding is a catalog entry,
it has a key, and `ipc.md` §4's `get` returns it verbatim. `generaldesign.md` §14.1
now drops `spec` beside `line_number` and `file_index`, on the grounds that all
three describe where a binding was written rather than what it does. `spec` needs
its own reasoning, because it is not a parse diagnostic and looks load-bearing: it
is a shim over Mango's original storage, where a binding lived in process memory
and could not be fetched over the IPC, so the source text was the only human handle
on it. A stored copy would also be a second copy of the truth that only some
bindings would have, since one written by `set` over the socket never had a config
line at all, and reconstructing a config line from a record by running a parser
backwards is lossy in a way the record is not.

**Also corrected, in the same pass.** `tags.md` §9 listed a covered decision under
"not covered" and announced an exception that was itself closed. `helpers.md` §6.2
said it contradicted "what §6.2 said", a document disagreeing with itself. The
`design-phases/README.md` inventory was stale in precisely the two rows this phase
had closed, and its closing paragraph announced two documents still holding an open
question between them. `scaffolding.md` cited a nonexistent worked example in one
line while another line in the same file said it did not exist. `layoutlanguage.md`
attributed the reversal of the 16-byte record to a section that had itself been
rewritten, so its pointer was to a claim no longer in the document.

**Left alone deliberately.** `devnotes/audits/` holds `architecture-audit.md` and
`audit-2-findings.md`, moved there during this pass to keep the audit record out of
the set of documents the design treats as authoritative. Both are point-in-time
records and their line-number citations were accurate when written, so correcting
them would falsify what they are for. The bare-filename cross-references to them
across the design documents are unchanged, which is a question about the convention
and not a defect. `countlines.sh` had an unrelated uncommitted edit.

**Re-verification after the corrections.** The header compiles under `-std=c11
-Wall -Wextra`, per `build.md` §1, and all 79 `OMNI_STATIC_ASSERT`s pass, up from
77 because the capability coverage check is one of the two added. `git diff --check` is clean.
The guard table counts 49 rows, L1 8, L2 10, L3 17, L4 10, R 4. A grep for
`xref.py` and `lcheck.py` finds both names only in this file. A grep for the twelve
wrong claims listed above returns nothing in the design documents. It returns two
things that are not instances of them: `omni_layout.h` §2's "A frame is 16 bytes
of header", which is a true statement about the frame and not the retracted
constraint record, and the paragraphs of this section that quote the claims in
order to record them.

**One of those twelve was committed while writing this section.** The `wholesale`
criterion and the work log's verification paragraph both said the grep returned
"seven lines in four files". It returns nine lines in three files, and it returned
seven in three before this section was added, because this section mentions the
word. The error is the exact one the previous paragraph is about: a count asserted
without being derived, in a file whose own discussion of the count changes it. Both
are now stated as the three documents that contain the word, which no paragraph
here can invalidate.

## Second post-completion audit, 2026-09-27

This pass was asked one question: within phase 00's twelve-document scope, is
anything still open and does anything contradict anything else. The answer is that
the design is in much better shape than the record of it, and the ratio is the
finding worth keeping, because it is the same ratio the first audit found. The
documents record a decision accurately once the decision is written down, and they
record their own status inaccurately in a way no single reader is positioned to
see, because the stale status is always in a summary, an inventory or a heading,
and never in the section it summarises.

**Six defects in the main design documents.** The first is a genuine
self-contradiction in a numeric contract. The capability bit pairing was stated in
`omni_layout.h` and in `configstorelayout.md` §3, and both said the same thing:
"bit *n* is the section whose id is *n*+1 for *n* in 0..5, 7 and 8". For *n* = 7
that clause yields id 8, which is wrong, and for *n* = 8 it yields 9, which does
not exist. The actual pairing is bits 0 to 5 against ids 1 to 6, bit 6 as the
socket with no section at all, and bits 7 and 8 against ids 7 and 8 unchanged, and
the break is the socket's. §4 of the same document carried the same claim in a
second phrasing, "the bits are the ids shifted, except that the socket's bit has no
row here", which is the same error with the exception left out. All three passages
now state the pairing in the order the bits actually come in, and the header
comment says why the assert cannot catch a mispairing: it holds `OMNI_CAP_DEFAULT`
to every defined bit, so it catches a bit added without widening the default and
nothing else.

The second is the only one that was a design contradiction rather than a
bookkeeping one. `layoutengine.md` §7.7 claimed "**Focus is an ejection**" and
built an argument on it, while `tags.md` §8.4 and `windows.md` §5 both say focus
does not touch the membership child and that the mutation is the hide *into* the
scratchpad. The section now states the opposite, with the reason that the ejection
is Wayfire's behaviour rather than this design's, and points at the two documents
that own the decision. Three other places in `layoutengine.md` repeated the claim
in weaker form and were corrected with it: the §8 mechanism table's client-set
membership row, the §3.2 bullet that said §7.7's question was waiting on the
determinism requirement, and §3.2's determinism bullet itself.

The third is a heading that lied about its own list. §3.2 was headed "Still open:"
over two bullets, of which the first was struck through and closed and the second
opened by saying the answer "is now a requirement rather than a question". Both
are now struck and closed, under a heading that says so. §3.3 opened on the surface
syntax of a program, which `layoutlanguage.md` owns, is normative for, and settles,
and §3.4 carried the same heading over four entries of which two were decisions and
two were open; the two open ones are now marked `**Open:**` in place. §3.6's list
was headed "Still open in this section:" over four entries of which three state
decisions, and the fourth carried the one genuine question, so the heading now says
that and the question is marked where it appears.

The fourth is the one that would have cost a reader the most. §4.7 asked two
questions that §3.5 and §3.6 had already answered several sections earlier: whether
a tag or a monitor names a layout independently of the current one, and whether a
group carries its own layout reference. §3.5 says a tag holds a layout name and a
monitor derives one from the head of its list; §3.6 opens by saying a group names a
layout. Both bullets now say so and cite the section that settled them.

The fifth is a count that three documents disagreed about. `layoutlanguage.md` §1
says a program is four keys under one prefix, `tomlparser.md` §11 already reasoned
in terms of "all four", and `layoutengine.md` said "three keys under one prefix" in
§4.4 and "the three keys it is stored under" in §2.10. The engine's two sentences
were not simply wrong, and saying so would have lost the thing that distinguishes
them: three of the four keys are structural and carry the program, and the fourth,
`rearrange_on_focus`, is a boolean about when a pass runs with no representation in
the block at all. Both sections now say three parts, four keys, and why, and
`layoutlanguage.md` §1's two sentences that said "all three absent" now say
"all three of the structural keys absent", which is the same claim with the fourth
excluded and one sentence of reasoning.

The sixth is a dangling citation. `input.md` §4 cited "§14.2" for the rule that a
binding's action name resolves on use rather than at set time, and `input.md` has
eight sections. The rule is `generaldesign.md` §14.1, with `helpers.md` §6.2 for the
`action_ref` resolution. This is the second `input.md` reference to a section that
does not exist to be fixed, the first having been the private-cache worked example
the earlier pass corrected.

**Four in the bookkeeping, which is where the errors concentrated.** The
`design-phases/README.md` inventory said `configstorage.md` §14 has 14 bullets; it
has 11, being 8 in §14 and 3 in §14.1. It said `tomlparser.md` §11 has 3 bullets; it
has 5, none of them open, and the row now names what each one is. The preamble of
`07-layoutengine.md` said the document has "eleven closed open-items kept as a
record and six still open" while item 6 of the same stub said §11 carried no open
item at all. §11 has 21 items, all closed or deferred, one of the 21 deferred to
phase 10; the stub's preamble is corrected, its item 2 is rewritten because the
question it asked was closed here as one buffer with a stage byte, and its "Done
when" clause that asked the same question a second time is aligned.

This file's own record was wrong twice. The unowned-items paragraph had become
garbled, promising "the five that were genuinely unowned" and then referring to
"the first two" and "the last two" over a list of two items, with a fifth item
appearing two sentences later; it now names the two, and records the real finding
that one of them is claimed by two different phases. And the catalog-freelist entry
said a `DESTROYED` slot that was never allocated "has its name cleared rather than
left stale", which is not the rule: `configstorage.md` §13 says a never-allocated
slot has `DESTROYED` deliberately clear rather than set, because `DESTROYED` is a
statement about a name that existed and was deleted, and `entry_generation` = 0 is
what distinguishes the case from a slot freed after use.

**The structural finding, which is the reason phase 07's stub gained a table.**
`layoutengine.md` keeps its open questions in two places, and the two do not
correspond: §11 is clear, and the body is not. A reader who checks §11, as the
inventory tells them to, concludes the document is finished. Twelve questions are
open in §3.2, §3.3, §3.4, §3.6, §3.7, §4.5, §7.2 and §7.5. One of the twelve is
already phase 07's, as its item 3, and eleven are assigned to nobody. Re-entrancy
is a thirteenth question in the same family and is not in the list, because
`layoutengine.md` §1 defers it to phase 10 on purpose. The stub now carries the
twelve with their locations and their owners, `layoutengine.md` §1's status table
names the residuals on the §3.3, §3.4 and §3.6 rows rather than calling them
settled, and the stub's preamble points at its own table. This is the one place
where the phase's "every open item has a closed decision or a named owner" criterion
is not yet met, and it is met for the wrong reason: the criterion was checked
against §11 and §11 was the easy half.

**Checked and found already correct,** so that the next reader does not spend the
same pass on it. The frame header at 16 bytes with `node_count` delimiting the
occupied range; the freelist link in `name_ref` on a `FREE` slot with
`catalog_free_count` seeded nonzero; `boot_time_ns` stamped from `CLOCK_MONOTONIC`
at creation and stored, with both sides reading the same clock so the comparison is
a subtraction; the dropped fields; the catalog freelist's seeded low run; the
payload split between `REGION_DESC` and `REGION_PAYLOAD`; the three
cross-references the earlier pass fixed; `layoutlanguage.md`'s authority over the
surface and `omni_layout.h`'s over the numbers; and the claim that no `wholesale`
occurrence asserts a port was taken unchanged, which is still three documents, this
one and `generaldesign.md` §14 and `11-protocols.md`, and every one of the three is
about the wording rather than about the port.
`xref.py` and `lcheck.py` do not exist in the repository, as the earlier pass
recorded, so the cross-reference check in this pass was done by reading every
citation it touched rather than by running a tool: `tags.md` §4, §5, §8.2 and §8.4,
`windows.md` §4 and §5, and this document's §3.3, §3.4, §3.5, §3.6, §4.5, §7.2,
§7.3, §7.5 and §7.7 all exist and are the sections they are cited as.

**One thing this pass did not decide,** and one the developer did. The developer
settled the device-rule tag: it is chosen in phase 03, when the input design is
fleshed out, on the ground that fleshing it out is what requires knowing the input
design, so a number picked before then would be a number picked for a record shape
that is still moving. `input.md` §7.2 and `03-input.md` item 2 now say that
instead of claiming phase 02, and the criterion above is met by a decision rather
than by two owners who happen to overlap. The developer also settled the second:
**the eleven unassigned `layoutengine.md` questions are all part of the
layoutengine design and are finalized in `07-layoutengine.md`, as items 9 to 19.**
The stub's preamble used to answer that by assertion, with a count of six that was
wrong, and the questions were distributed across eight sections of a document whose
§11 is complete, which is how a phase reports itself finished while eleven
questions are open in its body.

**Re-verification after this pass.** The header compiles under `-std=c11 -Wall
-Wextra`, per `build.md` §1, with no warnings, all 79 `OMNI_STATIC_ASSERT`s pass,
and
`OMNI_CAP_DEFAULT` is still `0x1FF`, now checked by a `_Static_assert` of its own
rather than by reading the macro. `git diff --check` is clean. The guard table
counts 49 rows, L1 8, L2 10, L3 17, L4 10, R 4, and `configstorelayout.md` §12
still has no `Semantic` tier. A grep for the retracted phrasings returns nothing in
the design documents: "the three keys it is stored under", "focus is an ejection",
"§3.2's open question", and "`layoutengine.md` §5.4" each come back with hits in
this file and nowhere else, and every hit is a paragraph quoting the phrase in
order to record that it was wrong, which is the same self-reference the `wholesale`
paragraph above had to be corrected for.

## Third post-completion audit, 2026-09-27

This pass was asked whether phase 00 is complete and whether phase 01 can start,
and the answer to the first is no. Three of the six acceptance criteria fail, one
of them on the check the second audit said nobody had ever performed, which is the
finding worth writing down: **the criterion that reads two documents and compares
them is the one that is false, and both earlier passes reported the design as sound
because every criterion they could run was a check on this file, a compile, or a
grep.** The first audit said the lesson for the next phase is that "Done when" has
to name the comparison rather than the conclusion. Phase 00 then wrote six criteria
and one of them is a comparison, and it is the one that does not hold.

**Two of the six criteria are re-run here and pass**, so that the next reader does
not spend the pass on them: the guard table is 49 rows distributed L1 8, L2 10,
L3 17, L4 10, R 4 with no `Semantic` tier, and the capability-bit pairing is stated
identically in `omni_layout.h` §4, `configstorelayout.md` §3 and its §4, which is
the defect the second audit opened with. The header compiles and all 79
`OMNI_STATIC_ASSERT`s pass. `git diff --check` is clean. The `wholesale` grep still
returns three documents, all about the wording. The `design-phases/README.md`
inventory's eleven bullet counts were re-derived from the documents and all eleven
are right, which is the second audit's fix holding. And the eleven
`layoutengine.md` body questions the second audit found are all present at the
locations its stub names, and the count of eleven is right.

### Three criteria that fail

**The guard table has guards without rows.** This is the criterion stated as "The
guard table has one tier per row and no guard without a row", and the second half
of it is false. `configstorage.md` §12 enumerates the L1 tier as six named checks:
`magic`, `format_version`, `header_size`, section constants, `state` in
{CREATING, READY}, and `commit_state` readable. The table's eight L1 rows state a
check for exactly one of the six. `header_size` is inside the first row. `magic`
and `format_version` appear nowhere in the table; a grep of `configstorelayout.md`
§12's forty-nine rows for either name returns nothing, although both are fields of
the header the table's own preamble claims to cover and both are L1 refusals by
`configstorage.md`'s own definition of the tier. The header's section constants
are named at L1 in `configstorage.md` and appear at L2 instead, as the row about
fixed ranges fitting before `OMNI_POOL_OFF`, which is a different check at a
different scale. And neither value-set field has a row: `configstorelayout.md`
§12 has a row for `ready` that is a value set and a second row that pairs it with
`state`, and it has nothing at all for the value set of `state` or of
`commit_state`.

The `commit_state` half is the one that bites. The table's L1 rows say
`commit_state` is `IDLE` whenever no writer holds the futex, and that readers never
accept a snapshot taken while it is not `IDLE`. Both of those are conditional
statements about a value. Neither of them says what happens to a `commit_state`
byte holding a value outside the four the header defines, so a corrupted
`commit_state` is refused by no row at all: it is neither `IDLE` nor not-`IDLE` in
any sense the table can test, and a reader implementing the table has no case for
it. This is the same defect class as the known-tag hole the second audit found in
`configstorage.md` §12.1, where `0x34` fell in no branch of the three-way test: a
defined value with no branch that handles it.

**`state`'s value set is stated two ways, and the enumeration is the stale half.**
`configstorelayout.md` §3's header table says `state` is `0` = CREATING, `1` =
READY, `2` = BROKEN, and `omni_layout.h` defines `OMNI_STATE_CREATING` 0,
`OMNI_STATE_READY` 1, `OMNI_STATE_BROKEN` 2 "terminal for this epoch".
`configstorage.md` §12 says the L1 check is "`state` in {CREATING, READY}". Two
documents, one of them the byte-layout authority and one of them backed by the
header, against a two-value set in a third.

The direction of authority is not in doubt here, which is what makes this cheap:
this is the reverse of the usual pattern in this repository, where the prose is
stale and the table is the authority. `configstorelayout.md` §3 says so of itself,
recording that an earlier draft claimed the defined fields ended at `0x087` when
the table had always been right. Here the table and the header agree and the
enumeration is the copy that is behind, so the repair is one word in one line.
The reason it still matters is the sentence immediately after it, which requires a
BROKEN block to be `OMNI_ERR_STORE_BROKEN`. An L1 guard written from the
enumeration has no case for value 2 and reaches a BROKEN block with no arm to
take, so it falls through to the general "these bytes are not what they claim to
be" path and reports a malformed header, which is a different code for a client
that is supposed to be told the block is intact and the instance is dead.

**The compile criterion names an invocation that does not exist.** The criterion
reads "Met, under `-std=c11 -Wall -Wextra -Iinclude`", and there is no `gcc`, no
`cc` and no `clang` on `PATH` in this environment; the compiler is reached through
`nix run`. The runnable form is:

```
nix run nixpkgs#gcc -- -std=c11 -Wall -Wextra -Iinclude -fsyntax-only -x c \
    include/shared/omni_layout.h
```

The two flags after `nix run` are needed because the target is a header rather
than a translation unit, and without them the command does not do what the
criterion says it does. This is the identical defect to the `xref.py` and
`lcheck.py` criterion this phase removed for naming a tool a future contributor
would go looking for and not find: a criterion that cannot be run as written is
worse than no criterion, because it reports a pass that was never demonstrated.
The two verification paragraphs at the end of the first and second audit sections
carry the same unrunnable form. The finding is not that the compile does not
succeed; it does, and the count of 79 is right. The finding is that the sentence
describing how to reproduce it is wrong, and no document in the repository records
that this project's C is compiled through `nix run` rather than through a compiler
on `PATH`, which is the fact the next reader needs and the one thing here a
reader cannot infer.

### Four defects in the design documents

**`layoutengine.md` §1's status row 3.7 is the row the second audit did not
correct.** Rows 3.3, 3.4 and 3.6 were rewritten to name their residuals rather
than call themselves settled, and row 3.7 reads "**resolved except re-entrancy**"
while §3.7's body carries four open bullets: what triggers a pass, whether the
pass is idempotent for an unchanged input set, when the first pass can run, and
re-entrancy. The row mentions none of the first two, and on the third it
over-claims in a way that is nearly right rather than simply wrong: it says "the
trigger is per layout as `rearrange_on_focus`", which settles whether a focus
change causes a pass, while the open question in the body is what the whole
trigger set is. A row that says "resolved except X" when there are three more is
worse than the rows that were wrong before, because it was written by a pass that
had already learned the lesson and applied it three times out of four.

**`07-layoutengine.md` says idempotence is not in the document's body. It is.**
The stub's reconciliation note ends with "Re-entrancy is the one question
deliberately outside both lists, and idempotence is the one that is in this stub
rather than in the document's body". `layoutengine.md` §3.7 carries it as a
bolded open bullet: "**Whether the pass is idempotent for an unchanged input
set**, which decides whether it can be called defensively on every relevant commit
without causing churn." The claim is load-bearing rather than decorative, because
it is the sentence that explains why idempotence is item 3 of the stub instead of
a row in the stub's table of eleven. Being wrong about it means the table is
missing a row that should be in it, and the two places that now hold "the same
list" hold different lists.

**Two documents count the same list differently.** `layoutengine.md` §1's row 3.3
says "three questions in §3.3 itself are still open, about cross-references,
arithmetic on operands, and the viewport's position relative to the solver", and
`07-layoutengine.md`'s table has two rows for §3.3, items 10 and 11, having
recorded in the same table that "the two halves are one question" and folded
cross-references with arithmetic into a single item. Both positions are
defensible and they cannot both be the count. This is the third instance of the
three-of-four-keys finding the second audit recorded, and it survives because the
two documents were each fixed in the same pass without either being compared
against the other, which is the comparison the first audit said was missing.

**One §11 bullet is neither closed, struck nor deferred.** `layoutengine.md` §11's
twenty-one items are twenty that carry a `*Closed*`, a strike, or a
`**Deferred, not open**`, and one, at the pan and zoom space, that carries
nothing. Its content asserts that the space "is a transform above the solver
rather than a constraint inside it", which is a decision, but it is asserted in
the same list whose other twenty entries each say what happened to them, and its
subject is held open in two other sections of the same document: §3.3 says
"whether that separation survives contact with the solver is still open" and §7.2
says "the coordinate space behind the viewport is still unsolved and is still the
hard part". The `README.md` inventory's row for this document says "20 closed or
struck, 1 deferred to phase 10, and none open", and the twenty is only twenty if
this unmarked statement is counted as closed, which is the same inference the
`wholesale` and `xref.py` criteria were both corrected for making.

### Three in the plan and the bookkeeping

**`01-testing.md` attributes the fuzz target to the wrong section.**
`configstorage.md` §14 does not name a fuzz target; §14 is the deferral list and
holds multi-painter arbitration, CAS, mixed-endianness, extension semantic
validation and compile-time ABI tests. The fuzz target is named at
`configstorage.md` §12.6 and in `configstorelayout.md` §12's own heading, and
`missing-devnotes-topics.md` attributes it to the document without a section, which
is the correct level of precision for it. The wrong citation is also worse than a
missing one, because §14 is the list of things deliberately *not* done, so a reader
who follows it concludes the fuzz target is deferred, which nothing says and
which the stub's own item 3 contradicts by asking for its entry point, its
mutation strategy and six named corpus cases.

**`ipc.md` §8 has an open item whose owner names nothing that exists.** The
`wm.reset` bullet records that the category name and field set are fixed and that
"the temp-file naming and lifetime for a soft-reset snapshot are not; they belong
with the transaction and readiness pass". There is no transaction pass and no
readiness pass: the transaction boundary is `ipc.md` §reload and
`configstorelayout.md` §13, and the readiness axis is `configstorage.md` §13, and
all three are written and complete, so what the bullet defers to is not a document
and not a phase. A grep of `design-phases/` for `reset` and `snapshot` returns one
hit, in `10-animate.md`, about animation progress. The bullet directly above it
treats the same class of thing correctly: the socket-path fallback's `/tmp` name
derivation is "implementation choices left to the store pass", which is a
statement about who writes the code rather than a question about the design, and
that is the right shape for naming a path. One of the two adjacent bullets is
wrong about what kind of thing it is describing.

**The `xref.py` criterion's own parenthetical is a stale count.** The criterion
states that both names "occur only in this file — item 10, this criterion, and two
work-log paragraphs — and nowhere else in the repository", which is four places,
and this file now holds eight lines naming either script. The first audit removed
a criterion whose count could not be derived, and the second audit restated the
`wholesale` criterion by document rather than by count for exactly this reason,
and this parenthetical was missed. It is the same defect a third time, in the one
place the file was supposed to have been made immune to it.

### What this pass did not decide

Four questions are the owner's rather than the auditor's, and all four are the
kind where picking the answer that looks more consistent would be the wrong move.
They are recorded here undecided rather than guessed.

*What the L1 rows should say.* That `magic`, `format_version` and the section
constants need rows is not in doubt; what is in doubt is whether they are
invariants in the sense the other forty-eight rows are, or preconditions on the
table rather than rows in it. A reader that has already mapped the file and
checked the magic does not need the second check, and putting it in the table
means the fuzz target asserts it fifty-one times rather than once. The header's
`OMNI_MAGIC` and `OMNI_FORMAT_VERSION` are compile-time constants, so a wrong
value is a build failure rather than a block, which is an argument that these two
are preconditions. The value sets of `state` and `commit_state` are a different
question and the table already answers it for `ready`, so those two look like rows
whatever the answer is for the other two.

*Whether the compile criterion should name `nix run` or a build file.* The
criterion is runnable today with the `nix run` prefix and no build system exists,
and the runnable form is recorded above. Whether a design document should be
quoting a package-manager invocation at all, rather than saying "compiles clean
under C11 with warnings enabled and the static asserts pass", is a question about
what these documents are for.

*Whether idempotence joins the eleven.* It is in the body and it is not in the
table, and the stub's note claims it is in neither. Either the table grows a twelfth
row or the note is corrected, and the two answers are not equivalent: a table row
says the question is phase 07's, while a corrected note says the question is
already item 3 of the stub and needs no row.

*What the soft-reset snapshot's naming is.* Whether it is an implementation
choice the way the socket path is, or a design decision with an owner, is a
judgement about how much of a path name belongs in a protocol document, and
`ipc.md` §4 already puts a snapshot path on the wire, which is the half that does
have to be designed.

### Re-verification after this pass

The header compiles under `nix run nixpkgs#gcc -- -std=c11 -Wall -Wextra
-Iinclude -fsyntax-only -x c include/shared/omni_layout.h` with no warnings, so
all 79 `OMNI_STATIC_ASSERT`s pass and `OMNI_CAP_DEFAULT` is `0x1FF`. `git diff
--check` is clean. The guard table counts 49 rows, L1 8, L2 10, L3 17, L4 10, R 4,
and `configstorelayout.md` §12 still has no `Semantic` tier; what this pass adds is
that L1 is short of rows for guards `configstorage.md` §12 names, which is a
different statement and does not change either number. A grep of that table for
`magic` and `format_version` returns nothing. A grep of `xref.py` and `lcheck.py`
returns only this file, which is now eight lines rather than the four the
criterion's parenthetical claims. A grep of `wholesale` returns three documents,
unchanged, and is unaffected by this section because that criterion is stated by
document rather than by line. The eleven bullet counts in the
`design-phases/README.md` inventory were each re-derived from the document they
describe and all eleven hold.

## Resolution of the third audit, 2026-09-27

The ten findings above were put to the owner as ten questions, each with its
options. All ten are now decided, the four questions the section above recorded as
undecided are among them, and the resolutions are applied. This section is the log
of what was decided and what changed; the audit above is left as written, because
a record of what a pass found is only useful if it is not edited to match what
happened next.

Two of the ten produced a decision that is larger than the defect they were raised
against, and those are marked, because a reader checking whether the finding was
addressed will otherwise find a smaller edit than the one that was made.

### The owner's decisions, and what was applied

**1. The L1 rows.** Preconditions for the constants, rows for the value sets.
`configstorage.md` §12's L1 bullet now states `magic == OMNI_MAGIC`,
`format_version == OMNI_FORMAT_VERSION` and `header_size` and the section
constants as a precondition on mapping, and gives the reason in the same place: a
wrong value in any of them means the bytes are not this format, so there is no
header left to check a field of. That is the framing `configstorelayout.md` §2
already used and §12 did not agree with, so the fix is one document moving to the
other rather than a new distinction. `configstorelayout.md` §12 gains two rows,
`state` and `commit_state`, each refusing a value outside its own set, and the
table is **51 rows, L1 10, L2 10, L3 17, L4 10, R 4**. The two value sets are
rows rather than preconditions because the table already knows how to state one,
for `ready`, and because a corrupted `state` or `commit_state` byte is a real
corruption that nothing refused before.

**2. `state`'s value set.** `configstorage.md` §12 reads {CREATING, READY,
BROKEN}. The header and `configstorelayout.md` §3 were already right and are
unchanged. The L1 row added above is what makes the enumeration load-bearing: a
guard written from the old two-value set had no case for 2 and fell through to the
malformed-header path, reporting `OMNI_ERR_BLOCK_MALFORMED` where
`ipc.md` §5.2 and `server.md` §4 branch on `OMNI_ERR_STORE_BROKEN`.

**3. The compile criterion.** The invocation lives in `build.md` §1 and nowhere
else. `build.md` is new, and it is a stub: §1 is the one fact that had to be
citable today, and §2 states what phase 02's build half adds to it. The four
places in this file that carried a bare flag list now state the property and cite
the section, which is the single-recipe form, and the criterion no longer claims a
command a reader cannot run. `missing-devnotes-topics.md` records that the document
now exists as a stub and that phase 02 owns the rest of it.

**4. `layoutengine.md` §1 row 3.7.** Rewritten. The row said "resolved except
re-entrancy" and named three settled things, while §3.7 carried four open
questions. It now names all four and says which is deferred. The trigger half was
the over-claim worth noting: the row claimed the trigger was settled as
`rearrange_on_focus`, which settles whether a focus change causes a pass, while the
body asks what the whole trigger set is.

**5. Idempotence.** It is a twelfth row, at item 17, in section order rather than
appended, and the three items after it were renumbered. The stub's note claiming
idempotence was "in this stub rather than in the document's body" was false:
`layoutengine.md` §3.7 has carried it since it was written, so by the table's own
scope statement it was always a missing row. The note is corrected rather than
deleted, and says why the row and the phase item are not a duplication: the item
is the work, the row is the obligation.

**6. The two counts.** The count is now derived and the disagreement is structurally
impossible. Every open question in `layoutengine.md`'s body carries an `**Open:**`
marker at the point it is stated, and every deliberately-deferred one carries
`**Deferred:**`. Twelve and two respectively, counted by a `sed` range over the
document's own section headings rather than by a number anyone wrote down.
`§1` gives the command and says why the range is the body. §3.3's disagreement
resolves to two markers against the stub's two rows, because the cross-reference
and arithmetic halves are one sentence and are marked once; that is now the
stated reason rather than a judgement about whether "one question" is true.

**7. The pan and zoom bullet.** Not decided in phase 00, and now says so. It is
marked `**Deferred:**` and the claim it used to assert as settled is withdrawn
rather than marked closed, because §3.3 and §7.2 of the same document hold the
classification open and asserting it in §11 while §3.3 asks it is the failure
`§1` warns about three sections later. The owner is phase 07, items 11, 19 and 20.
`design-phases/README.md`'s inventory row for `§11` moves from "20 closed or
struck" to 19, and records the body separately, because a single number cannot
cover a finished §11 and an unfinished body.

**8. The fuzz-target citation.** Left as it is, deliberately. `01-testing.md` is
wrong about which section names the fuzz target, and phase 01's first act is now
to read its stub against the documents as they are then. `design-phases/README.md`
states that rule for **every** phase in the directory, with the reason: a stub is
written against the documents as they stand, a reconciled document moves underneath
it, and nothing depends on the stub until the phase runs, so a piecemeal fix costs
a second pass over the same file and produces a record of edits the
re-evaluation would have made anyway.

**9. Snapshotting.** The feature is not being built, and this is the second of the
two decisions that turned out larger than the defect. There is no soft-reset
snapshot. `ipc.md` §reset is now two steps, clear and re-apply, and says plainly
that a soft reset is not reversible by itself: a caller that wants a way back
calls `save` and then `reload`, both of which are already specified, so the
capability the snapshot existed to provide was already in the design. The
`wm.reset` event no longer carries a path, the mode table's reversibility row
changed, and the §8 bullet that had no owner is deleted rather than rewritten,
because the thing it deferred to — a transaction pass and a readiness pass, neither
of which exists — was deferring a question about a feature that no longer exists.

What replaced it is `save` itself, and `ipc.md` §7 is new for it. The section
states the principle the owner's reasoning turned on: the socket and the TOML
parser are layers of indirection over the shared memory, direct block access is a
capability floor rather than a competitor, and a facade earns its existence by
offering something the direct route does not. Extracting part of the block to a
file is the worked example, because the block's scope-by-exclusion is what makes
"everything not window-dependent and not ephemeral under this prefix" a query
rather than a list somebody maintained, and a client at the block level has to
re-derive the classification, take a coherent view itself, and serialise the type
tags. An external parser, interpreter, library or application is a *user* of the
block in the same position, which is what the project's API-driven premise needs.

That decision had a consequence the finding did not predict. `configstorage.md`
§13 already said `save` takes an optional key-path pattern, and `ipc.md` §4's wire
form did not have one, so the two documents disagreed about a field the owner
described as central. `ipc.md` §save now takes `pattern` and `path`, which is the
"a keymatch and an output path" the decision describes, and the file it produces is
a TOML file the built-in parser reads back, so `save` narrowed to a namespace
followed by `reload` is a round trip with no snapshot facility anywhere in it. The
temp-file naming question moved to `save` and is now stated as an implementation
choice of that path, in the same sense as the socket-path bullet beside it.

**10. The stale count.** The criterion is now "neither name occurs in any core
design file", with the term defined in the criterion rather than left to the
reader: the `devnotes` documents and the `design-phases` stubs, excluding this
reconciliation record and the audits, which are records of documents that no
longer exist. The reasoning is generalised one clause further than the finding made
it, because the owner was right that the real problem is a class rather than this
instance: **a count is a number the document discussing the count can invalidate by
mentioning the thing it counts.** A file that is partly about a grep cannot assert a
grep's line count, however carefully it is qualified, and this one has now produced
three of them. Where a property is what matters, the criterion states the property.

### Where phase 00 and phase 01 now stand

The three failing criteria are repaired: the guard table has a row for every guard
that is an invariant and a precondition framing for the ones that are not, `state`'s
value set is stated one way in all three documents, and the compile criterion names
a command that runs. Of the seven further defects, six are closed and the seventh
is deferred to the phase that owns it by the standing rule in item 8.

**Phase 01 can start**, with the caveat the audit named and this pass confirms:
item 1's assertion set is `configstorelayout.md` §12, which is now complete at L1
and is a table a test can be written against. Item 5 remains the one that should
wait, because `generaldesign.md` §14.1 puts `mode_id` in the binding index key and
its meaning is undecided until `03-input.md` item 1, the device-rule record shape
until item 2, and the stylus trigger kind until item 3. `01-testing.md:90` claims
the phase does not depend on the missing documents, which is false for item 5, and
no "Done when" clause covers item 5. **Neither is fixed here**, for the reason in
item 8: both are the stub's to resolve when phase 01 begins.

### Re-verification after these edits

`git diff --check` is clean. The header compiles under the `build.md` §1
invocation with no output, so all 79 `OMNI_STATIC_ASSERT`s pass and
`OMNI_CAP_DEFAULT` is `0x1FF`. The guard table is 51 rows distributed L1 10, L2 10,
L3 17, L4 10, R 4, with no `Semantic` tier, and a grep of it for `magic` and
`format_version` still returns nothing, which is now the intended result rather
than a gap. The body marker count is 12 `**Open:**` and 2 `**Deferred:**` in §3 to
§8, matching the twelve rows of `07-layoutengine.md`'s table. A grep for `xref.py`
and `lcheck.py` returns this file and no other, which is what the criterion now
asserts and no longer counts. The `wholesale` grep is unchanged, and the eleven
bullet counts in `design-phases/README.md`'s inventory still hold, with the
`layoutengine.md` row amended for the deferred bullet and the body recorded
separately.

### Addendum: the stub-accuracy rule, and the exception to it

Item 8 above settled that `01-testing.md`'s wrong citation is phase 01's to fix, and
put the general rule in `design-phases/README.md`: a stub is re-evaluated whole
before its phase, and drift is left until then. That rule was then found to be
stated in the wrong place and to be quietly contradicted by this very pass, so
both were fixed.

**The place.** The rule lived only in the README, and a stub's own preamble said
`Stub. Not started.`, which is a different claim: it says the phase's *work* has
not been done, not that the stub's claims about other documents are not expected to
hold yet. A reader picks a phase up by opening that phase's file, so that is where
the disclaimer has to be. **All twelve stubs now carry it** as a line under the
title, pointing at the README rule. `01-testing.md` additionally names its own
known-wrong citation rather than leaving a reader to find it, and `07` names
itself as the exception.

**The contradiction.** The rule was written as a tidy distinction: a stub's *scope*
is an owner's decision and does not go stale, its *accuracy about other documents*
is derived and does, and only the second kind waits. The record does not support
the tidiness. Answers 5 and 6 in this pass corrected `07-layoutengine.md`'s table,
its counts and its idempotence note, and the first two were accuracy repairs about
`layoutengine.md` rather than scope — while answer 8 deferred a comparable accuracy
repair in `01-testing.md` in the same sitting. Both were findings from one audit and
the difference was the owner's call, not a category.

So the rule as written now says the narrow true thing instead: **do not repair
drift in a stub you were not asked about, and when you are asked, repair what was
asked and record which kind of edit it was.** The scope/accuracy distinction is
kept as the reason the default exists rather than as the rule itself, because the
reason holds even though the rule it was drafted to justify did not.

`00-reconciliation.md` and `scaffolding.md` do not carry the note, and correctly
so: this phase is closed rather than pending, `README.md` is where the rule lives,
and the inventory is a second output that fills in as the phases run rather than a
stub with a turn.
