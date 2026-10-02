# Phase 01: `testing.md`

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md).*

**Citation corrected 2026-09-27.** This stub's preamble used to name a known
wrong citation, deferred to this phase by `00-reconciliation.md`'s resolution
item 8. It is now repaired: item 3 cites `configstorage.md` §12.6, which is the
section that names the fuzz target, rather than §14, which is the deferral list.

Stub. Not started. Build scaffolding in the tree; the document is not.

**Build scaffolding added 2026-09-27.** `flake.nix`, `meson.build`,
`meson_options.txt`, `protocols/` and `nix/` are in the tree, and all four Nix
entry points work. This is recorded here rather than in `02-substrate.md` because
the build exists to make item 2 and item 3 runnable, and it was created before the
document was. See [Build scaffolding](#build-scaffolding) below for what it is and
what it deliberately does not do.

**Package split added 2026-09-29.** The build is five packages, not one:
`libomniwm`, `libomniwm-ipc`, `libomniwm-debugger`, `omniwm`, `omniwm-debug`.
`devnotes/build.md` §2.1 has the table and the closure check. This is recorded
here because it decides where the test binaries belong, which is not a question
the scaffolding answered on its own: a test for the store is a link-time
dependency on `libomniwm`, so it does not have to live in the compositor's package,
and `03-input.md` and `07-layoutengine.md` should not each have to re-derive that.

**Two decisions recorded 2026-09-29.** Phase 01 owns the test seam and states how
it works, which is what D8 above leaves open, and a test for a component with no
design yet lives in that component's design phase rather than here. The second one
moves item 5 of the list below: what stays in this document is the framework plus
the tests for the components that are already designed, and what leaves is
anything that would have to guess at a shape no document has.

**Stage 1's one missing document.** `missing-devnotes-topics.md` names it as the
reason stage 1 cannot be called done, and it is the only P0 row that is a
document rather than a decision.

## What it should contain

**1. The store write and read test.** The largest single item, and the one with
the most already written for it. It needs `configstorelayout.md` §2, §3, §4, §5,
§6, §6.1, §7, §8, §9, §10, §11 and §13, all byte-exact, plus the invariant table
in §12 as the assertion set. Phase 00 assigns the guard tiers, which is what makes
this an assertion list rather than a list of opinions.

The list grew by three sections when it was re-derived against the 51-row table,
and the reason each is in it is the row it carries. **§2** is the constants
section, and it is the one `build.md` §1's compile actually checks: every
`OMNI_STATIC_ASSERT` in `include/shared/omni_layout.h` holds a §2 value, so §2 is
where the header and the document are compared. **§4** carries two L2 rows, one
for `id == row` with 16-alignment and non-overlapping ranges and one for the
fixed ranges fitting before `OMNI_POOL_OFF`, so a write/read test that omits it
has no assertion for the section table it wrote. **§11** carries three more, the
`node_count` bound, the before/after generation sample with its retry, and the
`stage` byte being `SOLVED` or `ARRANGED`; item 4 has the solver's test and §11 is
where the block's own geometry lives. The original list came from
`missing-devnotes-topics.md` verbatim and predates the split of §12 into 51 rows
with one tier each.

**2. The ABI static-assert header.** This exists already, in
`include/shared/omni_layout.h` as `OMNI_STATIC_ASSERT`. What the document has to
do is state which invariants are asserted where, so a future change that moves an
offset has to touch the assert rather than discover the break.

**3. The store fuzz target and its corruption corpora.** `configstorage.md` §12.6
names a fuzz target that has no implementation, and `configstorelayout.md` §12's
heading calls the same table the thing the fuzz target asserts. The document has
to specify the entry point, the mutation strategy, and the corpus: a valid block, a
block with a truncated section, a block with a stale generation, a block with a
recycled frame, a block whose journal is inconsistent, a block past `block_size`.

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
and one test for the old flat bundle is not equivalent to five: `configstorage.md`
§12 orders four structural tiers and then calls replay a fifth, and
`configstorelayout.md` §12's table carries four `R` rows beside them. The replay
tier is not a variant of L3. It is where an entry's `entry_generation` is checked
against the live entry and where a `commit_id` is checked for its retained
`COMMIT_END`, so it is the tier the third bullet above is actually exercised
through, and a corpus organised over four tiers would have no case that reaches
it.

## Build scaffolding

The build exists so that item 2 is runnable and item 3 has somewhere to live. It
was written before this document, and it is Mango's shape with omniWM's tree.

Four commands, all working as of 2026-09-27:

| Command | What it does now |
| --- | --- |
| `nix build` | Builds `packages.default`, which is `omniwm`. Installs the ABI header and `libomniwm-ipc.so`. |
| `nix run` | Runs the ABI header check, since no compositor binary exists. |
| `nix flake check` | Builds and verifies the ABI header check. |
| `nix develop` | Shell with the full Mango dependency base, plus gdb, valgrind, clang-tools. |

**The four files, and what each is.**

- `flake.nix` is `flake-parts` with no Home Manager and no NixOS module, since
  both were declined. It has `nixConfig.extra-substituters` naming
  `cache.nixos.org`, which `02-substrate.md` B5 makes a requirement rather than a
  preference.
- `meson.build` is Mango's, with `core_sources` naming all 62 intended
  translation units, `ipc_sources` and `debugger_sources` naming the two smaller
  groups, and the `executable()` call commented out with the reason inline. Every
  dependency lookup above it is live, because a build that cannot resolve wlroots
  is a build whose pins are unverified.
- `nix/default.nix` is Mango's, with Mango's dependency list unchanged and the
  five packages wired as `build.md` §2.1 describes.
- `protocols/` is Mango's, including the three vendored `wlr-*.xml` files.

**What the build already answered.** `02-substrate.md` B2 was the phase's largest
external risk: `generaldesign.md` §19 says the tracked wlroots release was
unresolved, and the coupling to scenefx 0.5 was a hard constraint on everything
else. nixpkgs-unstable resolves the pair, wlroots 0.20.2 against scenefx 0.5.0,
and the whole list configures. `scenefx` is taken from nixpkgs rather than from
Mango's `github:wlrfx/scenefx` flake input, because Mango pins that input for
scenefx extensions nixpkgs does not carry and B3 has not yet decided whether we
need them. That is a deliberate deferral, not an oversight, and B3 remains open.

**What the build deliberately does not do.** The `omniwm` executable is not
built, because all 62 source files are zero bytes and a build that fails for a
reason unrelated to the design teaches a reader to ignore build failures. So
`nix build` installs the header and `libomniwm-ipc.so`, and `nix run` runs the
check. All of that changes when the substrate lands, which is
`02-substrate.md`.

The three shared libraries do build and link, with every source in them empty,
which means they carry no symbols. That asymmetry is deliberate and worth naming
plainly: the libraries are enabled so the source split and the package
boundaries are exercised now, and the executable is disabled because there is
nothing to put in it.

**The three source files that were missing.** `src/core/layoutengine.c`,
`src/input/switch.c` and `src/input/touch.c` were named in `filestructure.md` and
in the design documents but did not exist in the tree. They were created as empty
files so that `core_sources` is the real list rather than a list with three
gaps, and so that the failure mode when they are written is "a source file that
does nothing" rather than "a target that does not build". They are empty and are
not implemented.

## What it resolves

- Stage 1's completion criterion, which is currently undefined.
- The gap between "the document is byte-exact" and "the document is correct". A
  byte-exact layout can be internally consistent and still be unimplementable, and
  the write/read test is what catches that.
- The fuzz corpus, which is the only way the free-list and generation guards get
  exercised adversarially rather than by construction. The corpus must be organised
  per tier across all five tiers (L1-L4 and R), because the replay tier checks
  `commit_id` and its retained `COMMIT_END` and the `entry_generation` match, and
  a corpus that stops at four has no case to reach them.
- Whether the subtree exchange and the subtree delete are genuinely two operations,
  which is settled in `configstorage.md` §14.1 and asserted in item 7 here. That
  question was invisible while the obligation was listed as a name.
- The test seam: every component needs tests, and the seam decides which run in
  the plain unit runner and which under a wlroots test harness. `libomniwm` is an
  internal library and links wlroots directly, so wlroots-bound components are
  tested against the real library; the seam is about testability, not about
  shielding the core from dependencies. `02-substrate.md` item D8 and
  `scaffolding.md` call this out; the test document states the seam (§2 there),
  which is what D8 asked for.
- Whether a phase can have a runnable test at all. It could not before this, and
  the answer shapes the document: `01-testing.md` is the phase that makes the
  verification claim true rather than asserted, and a document that specifies
  tests with no runner in the tree is a specification with nothing checking it.
  The seam is now decided (§2 of the document), and its internal/external shape
  is recorded in `02-substrate.md` B1 and `decisions-2026-09-29.md` §0.

## Why it is second

It depends on phase 00 for the guard tiers, and it is the only way to check phase
02's build actually produces something.

It does not depend on any of the missing documents, with one exception. **Item 5
does.** `input.md` §7.2 assigns it a question whose answer is not in any written
document: `generaldesign.md` §14.4 puts `mode_id` in the binding index key and its
meaning is undecided until `03-input.md` item 1, the device-rule record shape is
undecided until item 2, and the stylus trigger kind is fresh work until item 3.
Every other item is writable against documents that exist.

## Done when

- `nix flake check` and `nix build` both pass, and `nix run` runs something. They
  do as of 2026-09-27, but they do so by checking one header, and the criterion
  here is not "the build is green" — it is that the assertion set the document
  specifies is actually executed by the build, which is a later item and does not
  follow from the header compiling.
- Every invariant row in `configstorelayout.md` §12 maps to at least one named
  assertion, and every guard in `configstorage.md` §12 that is a tier invariant has
  a row for it. The four that are **not** tier invariants are named as such rather
  than mapped: `magic`, `format_version`, `header_size` and the section constants
  are preconditions on mapping, so §12.6's check covers each of them once and the
  fuzz target does not assert them 51 times. A clause that asked for a row for each
  would be asking for the thing `configstorage.md` §12 decided against.
- The fuzz corpus list is written down as cases rather than as "malformed
  blocks".
- The pass half of the layout test is owned by `07-layoutengine.md` (items 15, 16
  and 17: what triggers a pass, when the first pass can run, and whether it is
  idempotent for an unchanged input set). The solve half is the one that is
  deterministic and writable now; a test of the full pass must not be asserted as
  closed in this phase. The determinism claim therefore covers the solve and the
  test should say so explicitly.
- IPC is part of stage 1 ("config store and ipc functional"). The IPC assertions
  that are testable without the compositor are limited: the readiness behaviour in
  `ipc.md` §5.2 (the per-command behaviour while `NOT_READY`), the envelope/value
  limits of §3.3-§3.4 where they constrain the wire format, and the `save`/`reload`
  round-trip shape from §4's `save` with an optional pattern (noting that
  `configstorage.md` §13 and the wire form both cover it). The command dispatch that
  requires a live compositor belongs elsewhere, and the test document must record
  that boundary.
- The three container operations of item 7 have assertions, and the exchange's
  free-count-unchanged assertion exists, since a missing one is how the two
  operations silently collapse into each other.
- Each of the five guard tiers of `configstorage.md` §12 has at least one named
  case, the fifth being replay, and the L2 cases assert the *differences* between
  the tiers rather than the refusals alone.
- **Item 5 has a decision recorded against it, or an owner.** Either the mode
  scoping, the device-rule record and the stylus trigger kind are decided and the
  binding match test is written against all three, or the test is written against
  what is known and names the three as the reason it is partial. The clause
  exists because item 5 is the one item with no document behind it, and a phase
  that reports itself done over an item nothing else constrains is a phase that
  reports itself done.
