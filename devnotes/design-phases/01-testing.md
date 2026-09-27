# Phase 01: `testing.md`

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md). This stub has a known wrong citation, recorded in
`00-reconciliation.md`'s resolution log rather than repaired here.*

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
deterministic, which is what makes a golden-file test possible. The determinism
precondition it used to be blocked on is now met: `windows.md` §13 fixed the
membership walk at ascending `entry_id` and `layoutlanguage.md` §3.0 fixed the
focus trigger at a per-layout `rearrange_on_focus`, so the test is not waiting on
either. What the document still has to state is what determinism *means* here,
which is a narrower question than it looks: a golden file is reproducible only if
nothing in the program is ordered by address or by hash, so the assertion is that
the solve reads its inputs through the `entry_id` order and not merely that it
produced the same bytes once.

**5. A gesture and binding match test.** `input.md` §7.2 already names the case
that matters: two conflicting bindings in one index bucket, where config order is
load-bearing.

**6. What `configstorage.md` §14 defers.** It defers compile-time ABI tests,
partly covered by the static asserts. The document has to say which half is
covered and which half is not.

**7. The three container operations, which are a new assertion group and the reason
this phase's list grew.** `configstorage.md` §14.1 now records a subtree delete, a
subtree exchange, and generation-correct deletion as *required*, and each is a
distinct case rather than three settings of one:

- The **subtree delete** has to assert that every name in the subtree is invalidated
  and that `catalog_free_count` rises by exactly the number of frames released, since
  a name left resolvable after its frame is on the free chain is the one failure
  that silently corrupts every later write.
- The **subtree exchange** has to assert that the free count is **unchanged** before
  and after, because a tag's children are still live in the other tag. An exchange
  that frees frames is indistinguishable from two deletes and a re-add until the
  catalog runs dry, so this is the assertion that distinguishes the two operations
  and it is the reason the stub for phase 02 treats the exchange as its own item.
- **Generation-correct deletion** has to assert the `ENTRY_FREE` and
  `GENERATION_MISMATCH` paths separately, since a reader that kept a frame offset
  across the delete gets one of the two depending on whether the frame was recycled,
  and a test that only does one of them will report whichever order it happened to
  run.

The L2 section cases are also worth stating here rather than in the fuzz corpus,
because they are decided rather than adversarial: `PRESENT` clear is not a failure
at all, `PRESENT` set with invalid framing is `OMNI_ERR_BLOCK_UNSUPPORTED`, and a
short block is refused at rest but skipped while `commit_state` says a growth is in
flight. A test per tier is the assertion set phase 00's restructuring made possible,
and one test for the old flat bundle is not equivalent to four.

## What it resolves

- Stage 1's completion criterion, which is currently undefined.
- The gap between "the document is byte-exact" and "the document is correct". A
  byte-exact layout can be internally consistent and still be unimplementable, and
  the write/read test is what catches that.
- The fuzz corpus, which is the only way the free-list and generation guards get
  exercised adversarially rather than by construction.
- Whether the subtree exchange and the subtree delete are genuinely two operations,
  which is settled in `configstorage.md` §14.1 and asserted in item 7 here. That
  question was invisible while the obligation was listed as a name.

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
  recorded as a blocker on that test. The claim itself is no longer blocked.
- The three container operations of item 7 have assertions, and the exchange's
  free-count-unchanged assertion exists, since a missing one is how the two
  operations silently collapse into each other.
- Each of the four guard tiers of `configstorage.md` §12 has at least one named
  case, and the L2 cases assert the *differences* between them rather than the
  refusals alone.
