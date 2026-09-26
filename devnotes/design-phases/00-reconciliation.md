# Phase 00: reconciliation of documents that exist

Stub. Not started.

Thirteen documents already exist and nine of them carry open items. This phase
closes those before any new document is written, because every later phase reads
them and a later phase written against an unsettled document has to be revised.

Nothing here is new design. It is finishing decisions that were started and left
half-made, and finding the places where two documents disagree.

## What it should contain

**1. The three-key layout program, in `tomlparser.md`.** The program is
`omniwm.layouts.<name>.rules`, `.spaces`, and optionally `.viewport`
(`generaldesign.md` §14's three keys, `layoutlanguage.md` §3.6.1). What is
unreconciled: the whole-program `constraint` framing, flattening, nested layouts,
partial-program deletion when only `.rules` is deleted, and the guards on key
shape.

**2. `configstorelayout.md` §4's "four fixed-shape records".** Only `binding` and
`client_rule` have byte tables. `constraint` and `map` are variable-length
(`configstorelayout.md` §4 says so in its own text) and the surrounding wording
still calls four records fixed-shape.

**3. The 19 semantic validation guards.** `configstorelayout.md` §12 defines four
tiers and its invariant table has roughly 50 rows, one per tier. Which tier owns
which guard is not settled, and the free-frame guard added for the arena free
list is the newest one and has no agreed owner yet.

**4. The `OMNI_GET_*` result vocabulary.** `configstorelayout.md` §14: a direct
`get` has nothing to return, and the catalog-index refusal needs a code. This is
a prerequisite for implementing `configstorage.md` §3.1, not only for this
design.

**5. Missing initialization rules in `configstorage.md`.** There is no
creation-time header initialization list, so `OMNI_HDR_OFF_ARENA_FREE_HEAD` has
nowhere documented to be set to `OMNI_REF_NONE` on a fresh block.

**6. `layoutengine.md` §3.2's false Mango attribution** for cluster behaviour,
and `layoutengine.md` §11's six open items: the priority band values, whether an
arranged layout is written beside the solved one, and the rest.

**7. `windows.md` §13's five open decisions**: membership walk order, whether a
focus change causes a pass, whether a fake client can take keyboard focus,
whether the client kind set is closed, and what a cluster contributes beyond
moving as one.

**8. `helpers.md` §11's remaining open items**: richer in-process `omni_event`
payloads, and the `owner` field's lack of enforcement. The `binding` key-to-action
path is closed and needs no work.

**9. `tags.md` §9's four open items**: stored versus derived membership, the tag
membership move operation, and the two that are explicitly other documents'.

**10. The wholesale-port wording.** `generaldesign.md` §14 still says "port
wholesale", which the current policy supersedes: Mango is a reference, and
optimization is allowed. This is a wording pass with a real effect, because a
reader who takes "wholesale" literally will not look for a place where the
design deliberately departs.

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

- Every open-items section in the nine existing documents has either a closed
  decision or a pointer to the phase that owns it.
- `xref.py` and `lcheck.py` are clean.
- A grep for "wholesale" returns only places where it is accurate.
