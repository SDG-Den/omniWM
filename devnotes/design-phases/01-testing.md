# Phase 01: `testing.md`

Stub. Not started.

**Stage 1's one missing document.** `missing-devnotes-topics.md` names it as the
reason stage 1 cannot be called done, and it is the only P0 row that is a
document rather than a decision.

## What it should contain

**1. The store write and read test.** The largest single item, and the one with
the most already written for it. It needs `configstorelayout.md` §3, §5, §6, §6.1,
§7, §8, §9, §10 and §13, all byte-exact, plus the invariant table in §12 as the
assertion set. Phase 00 assigns the guard tiers, which is what makes this an
assertion list rather than a list of opinions.

**2. The ABI static-assert header.** This exists already, in
`include/shared/omni_layout.h` as `OMNI_STATIC_ASSERT`. What the document has to
do is state which invariants are asserted where, so a future change that moves an
offset has to touch the assert rather than discover the break.

**3. The store fuzz target and its corruption corpora.** `configstorage.md` §14
names a fuzz target that has no implementation. The document has to specify the
entry point, the mutation strategy, and the corpus: a valid block, a block with a
truncated section, a block with a stale generation, a block with a recycled frame,
a block whose journal is inconsistent, a block past `block_size`.

**4. A layout and constraint solver test.** `layoutengine.md` §3.2 makes the solve
deterministic, which is what makes a golden-file test possible. The document has
to state what determinism means here, because §11 notes the membership iteration
order question is still open and a `HashSet`-shaped derivation is not
reproducible.

**5. A gesture and binding match test.** `input.md` §7.2 already names the case
that matters: two conflicting bindings in one index bucket, where config order is
load-bearing.

**6. What `configstorage.md` §14 defers.** It defers compile-time ABI tests,
partly covered by the static asserts. The document has to say which half is
covered and which half is not.

## What it resolves

- Stage 1's completion criterion, which is currently undefined.
- The gap between "the document is byte-exact" and "the document is correct". A
  byte-exact layout can be internally consistent and still be unimplementable, and
  the write/read test is what catches that.
- The fuzz corpus, which is the only way the free-list and generation guards get
  exercised adversarially rather than by construction.

## Why it is second

It depends on phase 00 for the guard tiers, and it is the only way to check phase
02's build actually produces something. It does not depend on any of the missing
documents, so it can be written while the rest of the plan is still forming.

## Done when

- Every invariant row in `configstorelayout.md` §12 maps to at least one named
  assertion, and every guard in `configstorage.md` §12 maps to at least one
  invariant row.
- The fuzz corpus list is written down as cases rather than as "malformed
  blocks".
- The determinism claim the solver test depends on is stated, or its absence is
  recorded as a blocker on that test.
