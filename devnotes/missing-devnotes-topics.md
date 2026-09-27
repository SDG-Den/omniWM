# Missing devnotes topics

The design documents that `generaldesign.md` promises but that do not exist yet.
`generaldesign.md` owns the high level for every subsystem below; each row names
the document that will own the detail, what that document has **not** answered
yet, the `README.md` roadmap stage that delivers it, and its priority. What a
document has already answered is not repeated here; it lives in that document,
and the decision log at the bottom of this file records what closed it.

Priority follows the `helpers.md` §9 bands, rounded to the subsystem that lands
first: P0 is required before any window appears, P1 is the core look and feel,
P2 is the extensibility surface, P3 is the advanced input and language work.

The work is sequenced in `design-phases/`, one file per phase, with the ordering
and its two departures from the roadmap recorded in that directory's README.

`layoutsystem.md` was deleted, and `looks.md` is an empty placeholder kept on
disk. Neither is a topic of its own: their content is split across the tags,
windows, and layout-engine rows, and across the draw, decorate, and animate rows
respectively, so a single subsystem document does not have to cover a whole
promise. `layoutengine.md` §3.4 records the same decision from the layout side,
naming `layout-test-examples.md` as where the built-in programs are specified
instead.

| topic | document | what hasn't been answered yet | stage | priority |
|---|---|---|---|---|
| tags | `tags.md` | what a tag looks like; tag rules; per-output tag defaults; the surface syntax for setting, unsetting and exclusively setting a tag | 7 | P1 |
| windows | `windows.md` | the surface syntax for anything in it; the client protocol surface; §13's one deferred item (what a cluster contributes to stacking and focus beyond moving as one) and the phase-08 handoff it names | 5, 8 | P0 |
| layout engine | `layoutengine.md` | pass triggers in detail, meaning when a solve is scheduled versus coalesced; the solved-to-arranged persistence rule; idempotence and re-entrancy; animation-driven updates; activation ordering | 6 | P0 |
| draw | `draw.md` | the scene graph node types and their animatable properties; backgrounds as colour, image, shader, vector overlay and panning infinite canvas; vector line objects; text and images; how the compositor's own drawn surfaces are produced; how externally rendered regions become nodes | 12, 13, 14 | P1 |
| decorate | `decorate.md` | multi-layer borders including gradients and textures; shadows; blur; glow; opacity; rounding; window overlays and underlays; dimming; the three per-window override routes converging on WINDOW_DEPENDENT keys; global versus per-window precedence | 8, 11 | P1 |
| animate | `animate.md` | animatable properties on nodes; timelines declared in config; curves; stagger; 2D versus 3D; shader-driven animation; how a live gesture keeps animation live; the interaction with the solver's geometry | 9 | P1 |
| input | `input.md` | the mapping from Mango's modes and tags onto this project's namespaces, which `input.md` §7.1 sets out as four candidates with their costs and does not choose between. Stage 10 owes the stylus binding type, which Mango has no struct to copy: pressure, tilt, absolute pointing and annotation mode | 4, 10 | P0 |
| monitor | `monitor.md` | output hotplug and how it reconfigures layout; per-output configuration matched by name, make, model and serial; virtual monitors; Xwayland integration and its tag, layout and decoration parity | 5 | P1 |
| protocols | `protocols.md` | the Mango protocol set, reused where we can and changed where we must; which wlroots managers the compositor instantiates; the region-based external rendering path; the later Wayland buffer-injection protocol for GPU-path external renderers | 16, 18 | P2 |
| languages | `languages.md` | the C reference client library over `include/shared/`; the Python reference scripting library; the Mango and Hyprland interpreters; why a first-class library in another language needs no compositor change. All four are **separate example programs written after the project is complete**, not deliverables of the window manager, and no interpreter or library is in scope at all — see `generaldesign.md` §17 | 15, 16, 17, 18 | P2 |
| build | `build.md` | the document itself. The dependency set and the wlroots and scenefx version coupling; the exact scenefx extensions needed for user GLSL shaders and 3D transforms, and whether it is a maintained patch or a fork; the solver's arithmetic width and whether a solve runs on CPU or GPU, which the layout engine does not depend on. A `flake.nix` is wanted, with **`cache.nixos.org` set explicitly as the substituter** rather than inheriting the ambient config, so a build is reproducible from a clean machine | 3 | P0 |
| testing | `testing.md` | **stage 1's one missing document.** The store write and read test, the ABI static-assert header, the store fuzz target and its corruption corpora, a layout and constraint solver test, a gesture and binding match test. The write and read test needs only `configstorelayout.md` §3, §5, §6, §6.1, §7, §8, §9, §10 and §13, all of which are byte-exact and internally consistent, plus the invariants in §12 as the assertion set. The fuzz target `configstorage.md` names has no implementation, and `configstorage.md` §14 defers compile-time ABI tests that the `OMNI_STATIC_ASSERT`s in `omni_layout.h` partly cover | 1 | P0 |
| licence | `licence.md` | the licence file itself, and a provenance record for anything actually copied, which is a chore at the moment of the first copy rather than a design question | 3 | P3 |

Rows P0 and P1 are the compositor itself and are what stages 1 to 14 deliver.
Rows P2 and P3 are the extensibility surface and the project-level decisions,
and are what stages 15 to 18 and the early documentation deliver.

## What stages 1 to 4 still need

Nothing above needs a decision. What stages 1 to 4 still need is **writing**,
not deciding:

- `testing.md`, which is stage 1's one missing document and the reason stage 1
  cannot be called done.
- `build.md`, whenever someone wants to build.
- The `binding` record offsets, which wait on stage 10.

## How the stages 1 to 4 audit was resolved

`audit-2-findings.md` audited the four earliest stages against the documents
below. Its findings are now split three ways in this file: the stale text and
missing definitions it found have been fixed in place, the documented
deferrals are left as they are because they are deliberate, and what was left
was a short list of decisions that are not derivable from what the documents
already say. All of those decisions are now closed.

Fixed in place since the audit, with no decision needed:

- `configstorelayout.md` §3 and its overview end the header at `0x0A7` and
  reserve from `0x0A8`, matching its own table and `OMNI_HDR_RESERVED_TAIL_START`.
  It was `0x0A3` when the audit was written; the arena free-list head took the
  four bytes at `0x00A4`.
- `configstorage.md` §4 lost the orphaned 32-byte constraint record and the stray
  table fragment, so the section defines the program tree once.
- The known-tag branch in `configstorage.md` §12.1 runs to `0x34`, which is what
  the header, `configstorelayout.md` §12 and the surrounding prose all say.
- `ipc.md` §3 has a `map` row, so the window-rule block has a wire form.
- `ipc.md` §1, §3.3 and `configstorage.md` §13 now have their constants in the
  one file that declares itself their home: 15 new definitions in `omni_layout.h`
  section 11, three static asserts tying them to the geometry they derive from.
- `ipc.md`'s three mis-cited sections now point at the commit protocol (§13) and
  at `configstorage.md` §12 for guards, and its response envelope uses the
  wrapped wide-integer spelling rather than a bare JSON number.
- `helpers.md` §6.2's `client_rule` example no longer carries the `match`,
  `target` and `scope` fields that four other documents removed from the record;
  `helpers.md` §8 no longer keeps a second `struct omni_server` or restates the
  boot order; and `omni_boot()` has one home, with the registry owning the sort
  and the server owning the call.
- `tomlparser.md` §2 dropped the `layout`, `layout_rule` and `space` tags, none of
  which name anything in the store, and reads `constraint` off the key instead;
  its binding field is `action`, and `layoutlanguage.md` §1 no longer defers the
  tag's name to `tomlparser.md`.
- `configstorelayout.md` §5 now carries the byte tables for `binding`,
  `client_rule` and the two variable composites, so the document that owns byte
  layout is self-sufficient for a reader and writer.
- Stale statements cleared in `layoutengine.md` §2.9, `layoutlanguage.md` §9,
  `configstorage.md` §2, `ipc.md` §8, `filestructure.md`, `tags.md` §10 and
  `architecture-audit.md`, the last of which is now marked as not a readiness gate.
- `missing-devnotes-topics.md` itself: the windows, layout-engine and input rows
  now describe what those documents say they cover.

Settled by decision since the audit, recorded in the documents that own them:

- **A layout program is four keys.** `layoutlanguage.md` and
  `layout-test-examples.md` are the authority on keys, so
  `omniwm.layouts.<name>` is `.rules`, `.spaces`, an optional `.viewport`, and an
  optional `.rearrange_on_focus`; the first three are the form all 1,138 lines of
  the test suite already use, and the fourth was added because a focus change is an
  arrange trigger whose answer differs per layout. `layoutlanguage.md` §1 and
  §3.0, `layoutengine.md` §4.4 and `configstorage.md` §4 and
  §13 now say so, and the `save omniwm.layouts.delta.*` scope in §13 records
  that the trailing wildcard is load-bearing rather than a convenience.
- **Input reuses MangoWM's implementation, changing what we need** (`generaldesign.md`
  §14). This closes the modmask grammar, the keysym name table, the key-to-action
  path, and the roadmap inversion between stages 4 and 6, since Mango's
  `KeyBinding` already carries a mod, a keysym and an `Arg`. `layoutengine.md`
  §10 and `helpers.md` §11 both record the unblocking.

Left as documented deferrals: `configstorage.md` §14 (multi-painter arbitration,
value transactions beyond grouped commits, mixed-endianness, extension semantic
validation, compile-time ABI tests), `configstorelayout.md` §14 (v1 capacity
choices), `tomlparser.md` §11 (the full TOML grammar, datetime arithmetic),
`helpers.md` §11 (component dependency edges, the diagnostic `owner` field), and
`licence.md`'s missing file.

Every item that needed a decision is now closed. The list is kept as the record
of what closed each one, because each was a real fork in the design and the
reasoning is the thing worth keeping:

1. **`action_ref`: closed as a frame offset holding the action's name.** The
   deciding fact is that a binding lives in the block, and the block is the
   user's editable configuration, so a binding has to be a name a user can read
   and change. A registry handle is a process-local index: it is meaningless in a
   file, it does not survive a restart, and it cannot survive a `save` and reload.
   `helpers.md` §6.2 said handle and is corrected; `ipc.md` §4,
   `configstorage.md` §4, `configstorelayout.md` §4 and the `omni_layout.h`
   comment agree.
2. **The unresolved name is checked on use, not at set time.** This follows from
   the above and is recorded in `helpers.md` §6.2. A hand-editable store cannot
   promise that every name in it resolves, and refusing the write would mean
   refusing to store a binding for a component that has not activated yet, which
   is the normal state during boot.
3. **`delete` while `NOT_READY`: closed as refused.** `delete` is a write, it
   appends to the journal and publishes a committed state, so it can remove state
   a component is about to populate before initialisation finishes at
   `server.md` §4 step 5. `ipc.md` §5.2 now agrees with `server.md` §4, where
   reads are served in every state and writes are refused with no per-command
   exception. `unwatch` stays served because it removes a subscription rather
   than adding one.
4. **The log and util contracts: written.** `helpers.md` §7 is now a contract
   with every function named: `omni_log_init`, `omni_log_set_level`, the four
   wlroots levels, the thread-local component tag, and Mango's thirteen `util.h`
   functions with the two changes that matter, `omni_now_ms` returning u64 because
   Mango's u32 wraps in 49.7 days and a wrapped animation timestamp subtracts
   badly, and `omni_regex_match` using PCRE2 so the rule filters, `watch`
   patterns and later gesture patterns share one engine. §7.3 adds the container
   kit and §7.4 records what the extensibility work will need from it.
5. **The `binding` record: closed in principle by a bigger decision.** All
   binding data lives in the block (`generaldesign.md` §14.1), which answers the
   question the deferral was protecting, because it is the port that determines
   what a binding has to express. Mango's five structs share a header and differ
   only in the trigger fields, so this is one tag with a kind discriminant rather
   than five tags. `configstorelayout.md` §4 now records that its 24-byte table
   is the v1 minimum and too small, and lists the fields that must exist. The
   **offsets stay open** on purpose: the header grows again in stage 10 for
   tablet and stylus, which Mango has no struct to copy, so fixing offsets now
   fixes them against a shape stage 10 revises.
6. **The keypress access pattern: designed.** Mango's handler is a linear scan
   over 144-byte records, 2.25 cache lines each, with 39% of each record an
   argument payload the scan never reads, and its tests ordered worst-first with a
   mode `strcmp` ahead of the modmask compare. `generaldesign.md` §14.2 to §14.4
   keep the block as the source of truth and put a hash index over it in
   `core/input`, keyed on `(normalized modmask, trigger, mode_id)`, with the
   resolved action handle cached in the index. Ordering within a bucket is config
   order and is load-bearing, because Mango's `isallowconflict` means the first
   match in document order wins.
7. **The store read path: optimized, and it changed the layout.** The reason it
   needed optimizing is that the storage model is not Mango's. Mango's read is a
   load from a struct the parser filled; a read here is a hash, an index probe, a
   slot validation, a frame dereference and a tagged decode. As written, a
   name-based read had no lookup structure at all and walked up to 16,384 catalog
   entries with a string comparison each. Three additions, in
   `configstorage.md` §3.1 and `configstorelayout.md` §6.1:
   - an in-block **catalog name index** (`OMNI_SECTION_CATALOG_INDEX` id 8,
     16,384 x 16 B, sorted by name hash, binary searched, collision runs walked),
     maintained by the writer in the same commit as the catalog entry, so it is
     never stale and never rebuilt, and refusing rather than degrading if its
     section row does not validate: `get` returns
     `OMNI_ERR_BLOCK_UNSUPPORTED` and there is no walk behind it, because a
     fallback scan's cost scales with how many keys the user has configured
     rather than with the size of the block;
   - a **`BINDS_UPDATED` journal kind** (id 5), because a binding change
     invalidates a derived structure rather than one cached value, so one rebind
     and a two-hundred-binding reload are one event to a client that only needs
     to know its index is stale;
   - **process-private copies for hot reads**, refreshed on change, so forty
     per-frame option reads cost forty `u64` compares and zero hash lookups.

   **No per-entry revision field.** The journal already carries the entry
   identity per entry and appends nothing a reader cares about when nothing
   changed, so it is the cheaper mechanism, it is already specified, and the
   catalog entry stays 32 bytes.
8. **The arena got a free list, as a prerequisite rather than a nicety.** The
   arena was a pure bump allocator whose documented behaviour was "the old frame
   leaks until the block is recreated", so a request name allocated in the arena
   would have leaked per request and a client cycling requests would have
   exhausted the pool. `configstorelayout.md` §5 threads the free list through the
   free frame's own header, so recycling costs no extra space, and §6's L4 guard
   rejects a frame whose `next_free` is non-zero. This also closed two leaks that
   predated the change, since a deleted entry's name and payload frames used to
   stay in the arena for the life of the block, which made a client looping `set`
   and `delete` a denial of service reachable from the socket.
9. **The request queue now uses a `body_ref`.** Slot is 256 B instead of
   `0x1100`; the queue is 64KB instead of 1.1MB. The name frame belongs to the
   slot from submission until release or reclaim, and the requester copies it out
   of the arena before releasing the slot.
10. **Net layout effect:** the fixed area is `0x11C000` (1,163,264 B) against
    the previous `0x1DB000` (1,945,600 B). The block is 782,336 bytes smaller
    *while* `get` gained a lookup structure it did not have. Every section after
    the catalog moved, so the offsets in `configstorelayout.md` §2 and
    `omni_layout.h` §2 are new and the derived-offset static asserts were
    re-derived rather than hand-edited.

