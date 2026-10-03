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
- The TOML parser (`tomlparser.md`) is designed. Its tests are here, §5.5.
- The tags container (`tags.md` §5) is designed. Its container-op assertions are
  here, as `configstorage.md` §14.1's three required operations.
- The layout solve (`layoutengine.md` §3.2) is designed for the *solve* half. Its
  tests are here, and the pass half is explicitly absent (§7).
- The IPC surface (`ipc.md` §3, §4, §5.2) is designed for what is testable
  without a compositor. Those assertions are here, and the live-dispatch half is
  explicitly absent (§7).
- Everything else has no test here. The binding index is the clearest case: its
  ordering *rule* is decided (§14.4 — last match in config order wins), but the
  walk it sits in is not designed, so the match test belongs to `03-input.md`
  item 5 rather than here. Keymodes, the device-rule record and the stylus trigger
  kind are decided in `decisions-2026-09-29.md` (§2 and §4) and fleshed out by
  `03-input.md`, which owns their tests. The render pipeline belongs to phases 08
  to 10. The server is designed in `02-substrate.md` (D10), which also owns the
  wlroots harness its components run under.

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
- **L2** (10 rows, section-level): the section test, §5.1 (b), except four rows.
  Three `solved` rows live in §5.3 where the geometry they protect is produced, and
  the `publish_seq` row lives in §5.1 (d), where the writer's metadata update is
  the context that makes it meaningful.
- **L3** (17 rows, slot-level): the allocation tests, §5.1 (c), with the
  journal-identity row asserted in §5.1 (d) on the same reasoning.
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

**Which half of `configstorage.md` §14's deferral this covers, and which half it
does not.** §14 defers "compile-time ABI tests asserting the header's values
against this document", and the deferral splits in two:

- **Covered.** Every constant in `configstorelayout.md` §2 that the header defines
  is held by an `OMNI_STATIC_ASSERT`, and the section offsets are held by
  arithmetic asserts that check the derived chain (`OMNI_JOURNAL_OFF`,
  `OMNI_REQUESTS_OFF`, `OMNI_REGION_DESC_OFF`, `OMNI_SOLVED_OFF`, `OMNI_POOL_OFF`).
  So the header cannot disagree with §2's numbers without becoming a build failure,
  and the section table cannot be laid out inconsistently without one either. §2 is
  the section `build.md` §1's compile actually checks.
- **Not covered.** §14 defers the asserts against `configstorage.md`, and that is
  the half nothing checks. §2's numbers, the header, and the store test agree by
  transcription; none of them reads `configstorage.md`'s prose, so a rule stated
  only there — a capacity rationale, a lifecycle sentence, a bound argued rather
  than tabulated — has no mechanical counterpart and can drift from the constants
  it justifies. Closing that half means asserts written against the document that
  carries the reasoning, which is a different exercise from the one §1 performs and
  is **not** done here. The fuzz target does not close it either, because a
  mutation asserts what the code does rather than what a document says.

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

### 5.5 The TOML parser

`tomlparser.md` is closed: its §11's five bullets are decided, delegated or out of
scope, so every assertion below is read off a decision rather than off a guess.
The parser's most important property is not a parse at all — it is that it is a
facade — so the first group is the one that catches a second value format
appearing.

- **facade equality (§1, §10).** A value written from a config file and the same
  value written over IPC are byte-identical in the block, in both directions,
  because the parser reuses `ipc.md` §3's encoder and decoder verbatim rather than
  having its own. A grouped commit from a file is indistinguishable from one from
  the socket: same `commit_id`, same `COMMIT_END`, same journal entries, so a
  watcher cannot tell which surface produced them. An unknown key is stored and
  served verbatim rather than rejected.
- **type binding (§2).** `boolean` → `bool` direct; a registered key decodes to its
  declared tag; an unregistered key's integer literal → `i64` and its float
  literal → `f64`. All four TOML date and time forms land on the single `datetime`
  tag rather than four.
- **no invented tags (§2).** A table bound to a declared composite key — `binding`,
  `constraint`, `client_rule`, `map` — takes that composite tag with its field
  names preserved, never a positional `tuple`. There is no `layout`,
  `layout_rule` or `space` tag: a space is an `option` and a rule is a named key
  inside it, so the assertion is that a whole program round-trips as the four keys
  of `omniwm.layouts.<name>`, with `.rules` and `.spaces` as `[[...]]` arrays and
  `.viewport` as an inline table.
- **numeric typing comes from the key (§3).** `gaps = 8` and `gaps = 300` are the
  same declared type, not the narrowest width that fits, so a consumer never has
  to handle both. A literal that does not fit the declared tag is a line failure
  carrying the line number, not a truncation. The explicit table form
  `wm.gaps = { type = "u32", value = 8 }` binds regardless of registration and is
  checked against the declared type for a registered key.
- **the two suffixed string forms (§4).** `500ms`, `2s`, `100us` and `500ns` are
  `duration`; a bare `1500` is a `duration` defaulting to milliseconds. A string
  that is not a valid suffixed duration is an ordinary `string` and is **not** a
  failure — `wm.title = "1500"` stays a `string`, which is the case that makes
  guessing wrong unacceptable. A `duration` is nanoseconds in the block and
  travels as a wide integer, so the config and socket forms agree exactly.
- **arrays (§5).** A heterogeneous array is a line failure with the line number,
  not a coercion that silently discards a value. An empty array has no elements to
  infer from, so it is representable only because `elem_type` is explicit in the
  wire form: `wm.key_order = { type = "array", elem_type = "string", value = [] }`
  parses and round-trips.
- **tables (§6).** A table is a `tuple` whose `field_types` come from field order
  once each field is bound to its registration, so `{ width = 2, color = "#3d6bff" }`
  yields `["u32", "rgba8"]`. A dotted key and a nested table spell the same config
  key. Assigning a table is one commit replacing the whole value and never a
  merge, which is what makes a reload's effect predictable.
- **datetimes (§7).** All four TOML forms normalise to the same value, so two
  offsets naming the same instant compare equal. A `datetime` is 8 bytes and uses
  `value_inline`, so it costs no arena frame — asserted against the frame count
  before and after the write.
- **per-line failure, one whole-file failure (§8).** A failing line is logged with
  its line number, counted in the response and not applied, while the rest of the
  file loads. A file that fails to parse begins no commit and leaves the live
  block untouched — the transaction assertion, and the one `ipc.md` §6's guarantee
  is inherited through. Exceeding `OMNI_CONFIG_MAX_OPS` is the single whole-file
  failure and is reported as `CONFIG_TOO_LARGE` with the count. The guarantee
  covers the block: an `exec` line's effects lie outside it and are not rolled
  back when a later line fails.
- **defaults are per key and never stored (§11).** A program with `rules` and
  `spaces` and no `viewport` gets the static identity viewport; no
  `rearrange_on_focus` gets the static `true`; `viewport` with no `spaces` gets no
  spaces; no absence is an error. The load-bearing assertion is negative — the
  block never carries a seeded copy of a default, so `save` writes out only what
  the user chose and a later change to a default still reaches everyone who has
  not overridden it.
- **blank is not malformed (§11).** A `spaces` array containing a table that is not
  a space is a parse error for that line, reported per §8, and never silently
  replaced by the default. A missing key and a present-but-invalid key take
  different paths and both are asserted, because the first is a decision about
  something the user did not write and the second would discard something they
  did write and did not mean.
- **round-trip (§9).** The parser is also the `save` implementation, so a saved
  config reloads: a declared `u32` writes as a bare integer and reloads as the
  same `u32` through its registration, a literal-derived `f64` reloads as the same
  tag, and a value TOML cannot express — a `blob`, an `option(None)` — is written
  in the explicit table form, which is why that form has to exist in the input
  direction too.

### 5.6 IPC, the testable boundary

The IPC assertions that run without a live compositor, per `ipc.md`:

- **readiness behaviour** (`ipc.md` §5.2): `get`, `watch`, `unwatch`, `save`, and
  `reset` hard are served while `NOT_READY`; `set`, `exec`, `delete`, `reload`, and
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
| the binding index's resolution walk and the binding match test that follows it | `03-input.md` item 5 |
| keymodes, device-rule record, stylus trigger kind | `03-input.md` |
| the IPC live dispatch that needs a live compositor | `02-substrate.md` (D8's harness, D10's server) |
| the render pipeline, the scene graph, animation and shaders | `08-draw.md`, `09-decorate.md`, `10-animate.md` |
| the input pipeline, seats, keyboard handling | `03-input.md` |

Two rows are worth a note. The wlroots harness row names `02-substrate.md` because
the server has no design decision of its own — its open items are the wlroots
surface it wraps and the contents of the registration table, which are answers to
"what does the substrate expose" — so `02-substrate.md` D10 claims it along with
the live dispatch, and the harness it needs is D8's. The looks row previously named
`06-looks.md`, which is not a phase in this plan; looks is split across draw,
decorate and animate, and the render work belongs to all three.

The binding match test was in this document and has moved to `03-input.md` item 5.
Its ordering rule is decided and was wrong here: §5.5 asserted a winner that
inverted `generaldesign.md` §14.4, and the correction is that the **last** match in
config order wins. The test itself has to follow the walk, and the walk is phase
03's subject — a test written before the bucket layout and candidate resolution
exist would have been a guess at a shape, which is the one thing §1 forbids.

The framework they will use is the one defined here; the seam they will run
against is the same boundary. Nothing in the list is deferred because it is hard
to test. Each is absent because §1's ownership rule applies to this phase too: a
test that has to guess at a shape no document has decided is a test that should
not exist yet.

## 8. Done when

- The runner is **specified**, not built. §3 fixes what `omniwm-test` is, what
  `OMNI_TEST` does, how a test registers, and that the runner fails loudly rather
  than reporting green with no cases — that much is this phase's output and it is
  done when the specification above is complete and internally consistent. The
  binary, the `tests` option in `meson_options.txt` and the `test/` tree are not
  owed here: a design phase delivers design, and every artifact §3 describes
  depends on a `libomniwm` that has no symbols until the substrate lands. The
  implementation lands with the phase that owns the substrate, and `meson.build`
  already reserves the position by recording that no test registry exists yet.
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
- The IPC boundary is recorded: §5.6 says what runs here, and §7 assigns the live
  half to `02-substrate.md`.