# Testing (devnotes/testing.md)

What a test is, how it is written, and what this build actually runs. This is the
phase 01 document, and it resolves stage 1's only missing document
(`missing-devnotes-topics.md`). It is not a test plan for the whole project: it
states the framework every phase shares, states the seam that decides which
component tests run in the plain unit runner and which run under the wlroots
test harness, and then tests only the components that
were already designed when this document was written. A component with no design
yet does not get its tests guessed here.

## 1. Ownership rule

A test for a component lives in that component's design phase, not here. This
document is the phase that owns the general framework and the test seam, and it
may test a component only if that component's design is stable enough that a test
would not have to guess a shape.

- The store (`configstorelayout.md`, `configstorage.md`) is designed. Its tests
  are here.
- The TOML parser (`tomlparser.md`) is designed. Its tests are here.
- The tags container (`tags.md` §5) is designed. Its container-op assertions are
  here, as `configstorage.md` §14.1's three required operations.
- The layout solve (`layoutengine.md` §3.2) is designed for the *solve* half. Its
  tests are here, and the pass half is explicitly absent (§7).
- The IPC surface (`ipc.md` §3, §4, §5.2) is designed for what is testable
  without a compositor. Those assertions are here, and the live-dispatch half is
  explicitly absent (§7).
- The binding index (`input.md` §7.2, `generaldesign.md` §14.4) has one decided,
  stable property: config order inside a bucket. That one test is here.
- Everything else — keymodes, the device-rule record, the stylus trigger kind,
  the render pipeline, the input pipeline, the server — is not designed, and its
  tests are owned by `03-input.md`, `05-server.md`, and the phases that follow.

The rule is enforced by the phase boundary rather than by an editor. When
`03-input.md` decides its open items, the tests it owes appear in that phase's
document, and the framework they use is the one this document defines.

## 2. The seam

`libomniwm` is an internal development library: it is the library omniWM itself
is built from, not something handed to other projects. It links wlroots, scenefx,
and the rest of the Mango dependency list directly, so a component test that uses
wlroots objects is testing real dependencies, not stubbing them. The seam is
therefore about testability, not about linkage. Three rules make it hold.

- **The solve is a pure call.** `layoutengine.md` §3.2 makes the solve a pure
  function of committed state, so the solver never reads the event loop. A test
  feeds an input snapshot and reads the output; nothing needs to be running.
- **Pure components run in the unit runner, wlroots-bound components run in a
  wlroots test harness.** The store, the parsers, and the solver have no wlroots
  object in their inputs, so they are covered by `omniwm-test`. A component that
  genuinely drives wlroots — an input router, an output, the server itself — is
  tested against the real library with a test compositor, the way wlroots and
  scenefx projects test their own code. There is no fake wlroots.
- **Time is an injected clock.** Everything time-dependent — animations, retry
  windows — reads an `omni_clock` that returns a `u64` ns timestamp. The test is
  a fake clock, so a stale-generation retry is tested by advancing the clock, not
  by sleeping.

Which of the two a component test lands in is decided by the seam's own line: a
component that reads or writes the store, or computes from committed state, is
covered by the unit runner, and a component that owns wlroots objects is covered
by the wlroots test harness. The wlroots harness is a shared facility rather than
each phase inventing one, because every phase from the server onward needs it.

## 3. How a test is written

One source file per test, named `test_<unit>.c`, next to the unit it tests — a
sibling of `src/core/<unit>.c` in `test/core/`, `src/ipc/` for `test/ipc/`. The
registering macro is `OMNI_TEST`, and the runner is `test/main.c`, which links
against `libomniwm` and, for the harness tests, wlroots.

```
OMNI_TEST(tags_subtree_delete) {
    omni_clock *clock = test_clock_start(0);
    /* driver */
    assert(success);
}
```

- A test registers with `OMNI_TEST(name)` and returns `void`; a failure is `assert`
  or `abort()`, so a passing test is one that reaches the end of its body. There
  is no assertion-count bookkeeping and no per-test config.
- The seed for any fuzz case is fixed per case and recorded in a comment, so a
  failure reproduces in `ci` and locally; a run with `OMNI_FUZZ_SEED` unset repeats
  the case once at its recorded seed and does not explore.
- The runner is one binary, `omniwm-test`, built only when `tests` is enabled. It
  does a registration-count check first, so a test that failed to compile is a
  build failure rather than a silent green. Harness tests are the same binary
  with wlroots in the link and a test compositor driving the registration; there
  is no second framework to learn.
- Each phase that adds tests extends the runner with that phase's source list in
  `meson.build`. The runner is deliberately not a dynamic discovery mechanism:
  the meson test list is the authoritative inventory, and it can be inspected.

The store fuzz target is a separate binary, `omniwm-fuzz`, because it has its own
definition of pass: no crash and the correct skip-and-continue. Its corpus and
its use are §6.

## 4. Coverage matrix

Every invariant row in `configstorelayout.md` §12 maps to at least one named
assertion. The rows are 10 L1, 10 L2, 17 L3, 10 L4, and 4 R; each is covered by
one or more of the store tests in §5.1 or by a fuzz case in §6. The matrix is the
authoritative mapping, and this section lists where each tier's rows are asserted
rather than repeating all 51 rows here:

- **L1** (10 rows, header-level): the store header test, §5.1 (a).
- **L2** (10 rows, section-level): the section test, §5.1 (b), except the three
  `solved` rows, which live in §5.3 where the geometry they protect is produced.
- **L3** (17 rows, slot-level): the allocation tests, §5.1 (c), with the
  journal-identity row asserted in §5.1 (d) where a writer's metadata update is
  the context that makes `publish_seq` meaningful.
- **L4** (10 rows, value-level): the value tests, §5.1 (e).
- **R** (4 rows, replay): the replay test, §5.1 (d).

`solved` rows (L2) are covered by the solve test §5.3, because `node_count`,
generation sampling, and the `SOLVED`/`ARRANGED` stage byte are asserted where
the geometry they protect is produced.

The four values that are **not** tier invariants — `magic`, `format_version`,
`header_size`, and the section constants — are named in §5.2 as preconditions on
mapping and are covered once rather than once per tier. The fuzz target does not
assert them 51 times.

## 5. Unit tests by component

Each subsection names the assertions, not the code. The byte-exact details live
in the cited sections; the test is the place that checks the document and the
implementation agree.

### 5.1 The store (write and read)

The largest single test group, and the assertion set is the §12 invariant table.

- **(a) header.** `magic`, `format_version`, `header_size`, and the section
  constants (preconditions, §5.2 below); `block_size` multiple of 4096 and within
  bounds; the three value words each refuse a value outside their set; the futex
  protocol with `writer_pid` and `writer_token` identifying the current holder
  while `futex == 1`; `commit_state` IDLE whenever no writer holds the futex;
  a reader refusing a snapshot taken while `commit_state` is not IDLE; the
  coherency between the value words (`ready` reaches READY only in a commit that
  sets `state` to READY, DEGRADED only in a commit that sets it to DEGRADED);
  BROKEN blocks never mutated, with no recovery path reusing their epoch. The 10 L1
  rows.
- **(b) section.** `id == row`, 16-aligned offsets, non-overlapping ranges, and
  the fixed catalog/journal/request/descriptor/solved ranges fitting before
  `OMNI_POOL_OFF`; `OMNI_POOL_OFF <= arena_end <= region_head <= pool_base +
  pool_size`; every offset/length dereferenced lying in `[0, block_size)`;
  `journal count <= OMNI_JOURNAL_CAPACITY`. The decided cases assert the
  *differences* between the tiers: `PRESENT` clear is not a failure at all;
  `PRESENT` set with invalid framing (or an unimplemented version word) is
  `OMNI_ERR_BLOCK_UNSUPPORTED` and refuses the block; a short block is refused at
  rest but skipped while `commit_state` says a growth is in flight
  (`configstorage.md` §12.6). Four L2 rows leave this subsection to the contexts
  that make them assertable: the three `solved` rows to §5.3 and the `publish_seq`
  row to §5.1 (d).
- **(c) allocation.** catalog freelist acyclic index chain; arena freelist acyclic
  frame chain from `arena_free_head`, terminating on `OMNI_REF_NONE`; every frame
  on the arena freelist lies in `[OMNI_POOL_OFF, arena_end)` and is unreachable
  from any live `body_ref`; name index sorted ascending by hash with `live_count`
  equal to the number of live catalog slots; every name-index row resolving to a
  live catalog slot whose name hashes to the row's value; 16-aligned arena
  allocation and region payload bases; no live reader result depending on stale
  bytes of a recycled slot; simulation of a consumer upload racing a
  generation/revision change; generation matching on every live reference;
  descriptor reuse incrementing `region_generation` and clearing prior active
  bits; trailing reclamation requiring DESTROYED and both active bits clear; a
  FREE or DESTROYED slot carrying zero length, `body_ref`, and `value_inline`;
  a request slot FREE only when its requester released it or reclaim proved it
  reclaimable; a terminal request slot carrying a non-zero `terminal_at_ms` from
  the commit that set its status; a PENDING request slot carrying `terminal_at_ms == 0` and never
  deadline-reclaimed; a request refused for readiness never consuming a slot or a
  ticket. 16 of the 17 L3 rows; the journal-entry L3 row sinks to §5.1 (d),
  where the identity it checks is the replay that reads it.
- **(d) replay.** every journal entry has the current epoch, its `commit_id`, and
  a unique `journal_seq`; `publish_seq` is even outside a writer's metadata
  update; a region lifecycle event carrying its catalog and descriptor
  identities; a published `commit_id` never visible without its `COMMIT_END`; no
  consumer applies an entry whose `commit_id` lacks a retained `COMMIT_END`; no
  consumer applies an entry whose `entry_generation` no longer matches the live
  entry. A corpus for the R tier has a case that reaches it: an entry whose
  generation changed mid-replay is skipped while the rest of the journal replays. R
  rows, plus the `publish_seq` L2 row and the journal-identity L3 row, which are
  asserted where the writer's metadata update is the context that makes them
  meaningful.
- **(e) value.** framed values and region payloads within declared section
  windows; a `body_ref` resolving to a frame whose `next_free` is non-zero is
  refused as free; inline values have `body_ref == 0` and a type-valid length;
  extension tags accepted only with structurally valid framed payloads; unassigned
  low tags (0x00, 0x35..0x7FFF) refused, not read as extensions; request-name NUL
  and fixed-limit handling with target generations matching; `CREATE_REGION`
  width/height/slot_count bounds; `(epoch, ticket)` validation by a requester
  before trusting a terminal slot; a region reference carrying a live descriptor
  index and matching generation; no bounds check adding before comparing, every
  limit test subtracting first. The 10 L4 rows.

### 5.2 The four preconditions, named

`magic`, `format_version`, `header_size`, and the section constants are
preconditions on mapping rather than tier invariants — a wrong value in any of
them means the bytes are not this format at all, so there is no header left to
check a field of (`configstorage.md` §12, L1 paragraph). The store test asserts
each once, and the fuzz corpus never bothers with them (a case that clobbers
`magic` is a refusal that proves nothing about tiers). The ABI header
`include/shared/omni_layout.h` holds a `OMNI_STATIC_ASSERT` for each `omni_layout.h`
constant per `build.md` §1; the mapping precondition and the compile-time assert
are the same fact checked in two places, and §12.6's check covers them once.

### 5.3 The layout solve

`layoutengine.md` §3.2 makes the solve deterministic, which is what makes a
golden-file test meaningful. Determinism here means the solve reads its inputs
through the `entry_id` order — `windows.md` §13 fixed the membership walk at
ascending `entry_id`, and `layoutlanguage.md` §3.0 fixed the focus trigger at a
per-layout `rearrange_on_focus` — and not merely that it produced the same bytes
once. The assertions:

- solved output is reproducible: the same input snapshot and the same epoch yield
  the same `OMNI_SECTION_SOLVED_LAYOUT` bytes, including across a shuffled input
  order, because nothing is ordered by address or by hash.
- `node_count <= OMNI_SOLVED_SLOT_COUNT_MAX` and `16 + count * 24 <= section
  size`;
- the stage byte is `SOLVED` or `ARRANGED`, never another value;
- a solved layout read samples generation before and after and retries on change
  (`configstorelayout.md` §12, the three `solved` rows).

The fixed-point representation decided for B4 (`02-substrate.md`) is exercised
here at the commit surface: the input is parsed from TOML float literals
(`tomlparser.md` §3), the stored value is fixed-point, and the solve consumes the
fixed-point representation with no float scope in the block.

The pass half — what triggers a pass, when the first pass may run, whether it is
idempotent — is owned by `07-layoutengine.md` (items 15-17) and is absent here
(§7). The determinism claim therefore covers the solve and not the pass, and this
document says so.

### 5.4 The three container operations

`configstorage.md` §14.1 records three *required* operations, each a distinct
case rather than three settings of one:

- **Subtree delete**: every name beneath the prefix is invalidated, and
  `catalog_free_count` rises by exactly the number of frames released. The free
  count is the assertion that matters — a name left resolvable after its frame is
  on the free chain is the failure that silently corrupts every later write.
- **Subtree exchange**: the free count is **unchanged** before and after. This is
  the assertion that tells the exchange from two deletes and a re-add, and a
  missing one is how the two operations silently collapse into each other.
- **Generation-correct deletion**: the `ENTRY_FREE` and `GENERATION_MISMATCH`
  paths are asserted separately, because a reader that kept a frame offset across
  a delete gets one or the other depending on whether the frame was recycled. A
  stale `entry_ref` in the exchange must not carry the dead half across: the
  exchange validates both sides or refuses.

### 5.5 The binding index (one test)

`input.md` §7.2 names the one case that is stable today: two conflicting bindings
in one index bucket, where `generaldesign.md` §14.4's `mode_id` is part of the
index key and Mango's `isallowconflict` makes config order load-bearing inside a
bucket. The test asserts that the later binding in the same bucket wins, and that
a binding in a different bucket does not participate. Everything else about input
— keymodes, device-rule records, trigger kinds — is owned by `03-input.md`.

### 5.6 IPC, the testable boundary

The IPC assertions that run without a live compositor, per `ipc.md`:

- **readiness behaviour** (§5.2): `get`, `watch`, `unwatch`, `save`, and `reset`
  hard are served while `NOT_READY`; `set`, `exec`, `delete`, `reload`, and
  `reset` soft are refused with `NOT_READY`. The table's split is the test: reads
  are never refused, writes are.
- **wire limits** (§3.3-§3.4): each of the value limits in §3.3's table is
  asserted to be refused with `PARAM_INVALID` during decoding, before the store
  is touched: `OMNI_VALUE_MAX_NAME` (4095), `_MAX_STRING` (1 MiB),
  `_MAX_BLOB` (768 KiB), `_MAX_FRAMED` (1 MiB), `_MAX_ARRAY_ELEMS` (65536),
  `_MAX_TUPLE_FIELDS` (64), `_MAX_GROUPED_KEYS` (4096), `_MAX_NESTING` (16).
  `OMNI_SOCK_MAX_LINE` bounds bytes between newlines and bounds nothing else, so
  the limits are asserted per quantity, not as a single line-length check. No
  response-size limit is asserted, because §3.4 deliberately has none; a large
  read succeeding is itself the assertion.
- **save/reload round-trip** (§4): `save` with an optional pattern serialises
  what exists, and the reload path (`configstorage.md` §13) is the same sequence
  of `set` operations. The round-trip test asserts the *catalog contents* — every
  key the save wrote is present after reload with the same tag and value — and
  the assertion is scoped to the block, not to effects, because `ipc.md` §4 says
  specifically that reload's guarantee covers the block and never the side
  effects of an `exec` line. The test also asserts the transaction boundary: a
  file that fails to parse begins no commit, so the live block is untouched.

The command dispatch that requires a live compositor sits outside this document;
§7 records the boundary.

## 6. The store fuzz target

`configstorage.md` §12.6 names a fuzz target with no implementation, and
`configstorelayout.md` §12's table is the thing it asserts. This document
specifies it.

- **Entry point**: `omniwm-fuzz` takes a block image and a seed, maps it, runs
  the reader, and asserts two things only: no crash, and the correct
  skip-and-continue for the tier the mutation was aimed at.
- **Mutation strategy**: garbage the block — mutate random bytes, truncate,
  clobber lengths and refs — because every mapping process can write every byte
  of an RW mapping and page-level protection cannot work inside one.
- **Corpus, organised per tier.** Because each §12 invariant names exactly one
  tier, a case is asserted against exactly one expectation: a mutation that
  should trip **L1** refuses the block rather than degrade any single entry; one
  that should trip **L2** drops that section and leaves the rest readable (or
  refuses when the section is PRESENT and unframing, per §5.1 (b)); one that
  should trip **L3** skips that one slot and leaves every other slot readable;
  one that should trip **L4** skips that one entry and continues; one that
  should trip **R** skips the unretained-`COMMIT_END` or stale-generation entry
  but replays the rest of the journal.
- Seed list (recorded, so each case reproduces): a valid block; a block with a
  truncated section; a block with a stale generation; a block with a recycled
  frame; a block whose journal is inconsistent; a block past `block_size`; a
  broken (BROKEN) block with a terminated writer. The fuzz binary is a different
  definition of pass from the runner, so it is a separate target.

## 7. Deliberate absences

Each item that is *not* tested here, and the phase that owns it.

| absent | owned by |
|---|---|
| layout **pass**, not the solve (items 15-17 of `07-layoutengine.md`) | `07-layoutengine.md` |
| keymodes, device-rule record, stylus trigger kind, and any binding-match test that depends on them | `03-input.md` |
| wlroots-bound components, tested under the wlroots harness (§2) | `05-server.md` first, then every phase that owns one |
| the render pipeline, animation, shaders | `06-looks.md` |
| the input pipeline, seats, keyboard handling | `03-input.md` |

The framework they will use is the one defined here; the seam they will run
against is the same boundary. Nothing in the list is deferred because it is hard
to test. Each is absent because `03-input.md` item 5's rule applies to this phase
too: a test that has to guess at a shape no document has decided is a test that
should not exist yet.

## 8. Done when

- `nix flake check` and `nix build` both pass, and the `omniwm-test` binary is
  part of the build with tests enabled and fails loudly if it has no cases.
- Every §12 row maps to an assertion via §4's matrix, and §4 says which test each
  tier's rows live in. The five tiers each have at least one named case, the
  fifth being replay.
- The four preconditions are named in §5.2 as preconditions and asserted once,
  not 51 times.
- The three container operations have assertions, and the exchange's
  free-count-unchanged assertion exists.
- The fuzz corpus is written as cases, not as "malformed blocks", and each case
  has a recorded seed.
- The solve test asserts the determinism *mechanism* (entry_id ordering), not
  merely identical bytes once.
- The IPC boundary is recorded: what runs here and that live dispatch is owned by
  `05-server.md`.