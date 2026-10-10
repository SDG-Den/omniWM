# omniWM design snapshot: 2026-10-03

This is a snapshot of the omniWM design as it stands on 2026-10-03, taken at
git commit `fb7c1f3` on `main`. It describes the whole design of the window
manager in one place. It is a snapshot and not a specification: the authoritative
documents are the individual files under `devnotes/`, and where this summary and
a source document disagree, the source document wins. A snapshot also goes stale,
so a reader should treat every statement below as "as designed on this date".

The project is **design only**. Every `.c` and `.h` file in `src/` and `include/`
is zero bytes, with one exception: `include/shared/omni_layout.h` (1182 lines) is
the compiled ABI header and the only artifact with content. The design itself is
roughly 18,000 lines of markdown across 30 files in `devnotes/`.

## How to read this document

- Sections 1 to 4 are the premise: what omniWM is, why it is shaped the way it
  is, and the one architectural idea the rest serves.
- Sections 5 to 12 are the designed subsystems: the config store, the component
  substrate, the layout engine, tags, windows, the socket API, the TOML parser,
  and input.
- Sections 13 to 16 are the subsystems that are promised but not yet written:
  monitors, the look pipeline, protocols, and the language clients.
- Sections 17 to 20 cover the build, the test design, the design process itself,
  and the known risks.
- Section 21 is a document map and section 22 is the source tree.

## 1. What omniWM is

omniWM is a Wayland window manager (compositor) written in C. Its organising idea
is that the compositor exposes a mechanism and lets everything user-facing be
built on top of it, so that the compositor does not need to be modified to add a
feature a user wants. It is inspired by MangoWM, and much of its lower-level code
is a deliberate port of MangoWM at commit `d5a0e1e` (branch `config-scriptability`).

The project states three headline promises, each describing where the power lives:

- **Any Language.** Nothing about the user experience lives in C config code. A
  user reaches the compositor through a stable shared-memory interface, so a
  config in any language is an external program rather than a fork.
- **Any Layout.** Tiling, scrolling, floating, and canvas layouts are not
  hardcoded algorithms. They are constraint programs the user writes.
- **Any Look.** Decoration, animation, backgrounds, and custom UI are not a fixed
  option set. They are nodes in a scene graph the user configures.

A fourth, quieter promise underlies the three: **the compositor is not
special-cased**. The same mechanism that lets a user write a config also lets the
compositor ship its own defaults, and the same mechanism that lets a user restyle
a window also lets the compositor draw its own. There is no privileged internal
path.

## 2. Why it is built this way

Every structural choice serves one property: the compositor does not need to be
modified to add a feature. Three ideas follow.

- **The interface is data, not code.** The interface between the compositor and
  everything else is a fixed block of shared memory with a documented layout. Any
  language can speak it by reading one header. `include/shared/` is therefore a
  header-only ABI surface that must compile without the compositor, without
  wlroots, and without a C toolchain that knows this project.
- **A user-facing optional feature is a component, not a branch.** The test for
  "component" is necessity, not taste: if the compositor cannot manage windows
  without it, it is a core service rather than a component. A component exposes
  options, actions, and triggers, and can be enabled, disabled, and reconfigured
  at runtime without a restart.
- **The layout engine is a solver, not a set of layouts.** If layout is a
  constraint program, then a layout nobody anticipated is a config file rather
  than a patch.

## 3. Foundation and dependencies

omniWM is built on **wlroots** and **scenefx**, and is expected to extend both.

- **wlroots** supplies what a window manager should not write: the DRM/KMS
  backend, session and VT handling, the allocator, dmabuf, explicit sync, the
  swapchain, EGL/GLES2 setup, damage tracking and frame timing, direct scanout,
  the output layout, headless and noop backends for testing, the input plumbing
  (libinput, keyboards, pointers, touchpads, touchscreens, tablets, switches),
  and roughly 46 Wayland protocols including pointer constraints, relative
  pointer, keyboard-shortcuts-inhibit, virtual keyboard and pointer, and Xwayland.
- **scenefx** replaces the wlroots scene graph with a renderer that can blur, drop
  shadows, round corners per corner, clip to a region, and apply gradients. It
  keeps the upstream `wlr_scene` and `wlr_scene_tree` names, so patterns transfer
  from wlroots-based compositors.
- **Version coupling:** scenefx 0.5 requires wlroots 0.20 and the two are
  developed together. Which wlroots release omniWM tracks is unresolved.
- **The largest technical risk to Any Look is stated up front.** scenefx 0.5 has
  no public API to register a user shader or attach a filter to a node, and no 3D
  transforms at all (a node carries a 2D affine matrix and nothing more). User
  GLSL shaders and 3D animation therefore require extending scenefx with a new
  node type and a new render-pass entry point. This is the one place where
  "extend the base library" means "maintain a patch".

## 4. The architectural spine: one authority, many clients

The whole system is **one shared-memory configuration block**, owned by the
server for the process lifetime. Every other surface in the project is a client
of that block and nothing else.

```
                 +--------------------------------------+
   config file ->|                                      |
   TOML parser ->|        the shared config block       |<- compositor
   socket API  ->|        (the only authority)          |   components
   external lib->|                                      |
   interpreter ->+--------------------------------------+
```

The properties this buys:

- **The store is an open catalog.** It has no key-specific storage logic and no
  schema. A key nobody has heard of is stored and served verbatim. This is what
  lets a user keep their own state under their own namespace, and why a language
  nobody has written yet works.
- **Every write path is identical.** The built-in TOML parser is a facade, not a
  privileged path: it runs in-process but is a client of the same block, and it
  has no write capability a socket client lacks. A value written from a config
  file and the same value written over IPC are byte-identical in the block.
- **A config file is a stream of operations.** It is a sequence of `set` and
  `exec` operations, not a state dump.
- **Writes are serialised, not locked into a design.** Compositor execution is
  single-threaded on one event loop; the block's futex serialises external
  writers. A component never contends with a socket client, and a socket client
  never sees a half-applied commit.
- **The socket is optional.** Its absence degrades the project to a lower skill
  floor without making it unusable, because the block is authoritative.
- **There is no plugin ABI.** Components are compiled in. Extensibility is data
  and the socket, not dynamic loading.

### 4.1 Core service versus component

The distinction is necessity:

- **Core service:** anything the compositor needs in order to be a compositor.
  The layout engine is one (without a layout there is nowhere to put a window).
  So are the store, the input router, output and surface management, and the
  component registry itself. These are not components, are not toggleable, and
  are not in the activation order; they are what the activation order runs inside.
- **Component:** an optional capability layered on the core. Visuals, decoration,
  effects, and optional built-in layouts belong here.

A capability can be optional and still be core, and the layout engine is the case
that proves it.

### 4.2 One scene graph

There is one scene graph and everything visible is a node in it: client surfaces,
borders, shadows, overlays, underlays, gradients, text, buttons, backgrounds,
vector lines, and the compositor's own widgets. One graph means one place where
effects and animation apply, and a decoration and a background are animated by
the same code that animates a window. The scene is owned by the server and
rendered per output.

## 5. The config store (the block)

The store is the centre of the system. It is specified in two documents:
`configstorage.md` owns the concepts, and `configstorelayout.md` owns the exact
bytes. `include/shared/omni_layout.h` is the single home for every constant.

### 5.1 Core principles

1. **The SHM block is authoritative over everything.** The socket, the TOML
   parser, internal WM code, and third-party libraries are all facilities over
   the same block. A well-written library should not need the socket at all.
2. **A config file is saved state:** a sequence of `set`/`exec` operations
   replayed into the block at startup.
3. **The storage layer is an open catalog.** Any key may exist with
   self-describing values. No key-specific code in core storage or IPC.
4. **Guards at consumption.** Bad data is allowed to exist in the block and is
   refused only when a consumer parses it. The block is hostile input on every read.
5. **Single write at a time.** One commit futex in the header serialises writers;
   readers are lock-free and accept only an idle, commit-id-stable snapshot.
6. **Whole-block structural safety is the core WM's own discipline**, not a
   contract imposed on third parties.

### 5.2 Identity vocabulary

Everything the block names has a defined identity:

| term | meaning | rule |
|---|---|---|
| `epoch` | one compositor instance | changes on every restart; never reused while stale mappings may exist |
| `commit_id` | one atomic state commit | increases once per commit, including grouped commits and growth |
| `journal_seq` | one journal entry | increases once per appended entry; group entries share a `commit_id` but never a `journal_seq` |
| `entry_id` / `entry_generation` | catalog slot / that slot's allocation lifetime | a key reference is the triple `(epoch, entry_id, entry_generation)` |
| `region_generation` / `region_revision` | region descriptor lifetime / published image version | revisions do not wrap within a generation |
| `request_ticket` | a request within an epoch | complete identity is `(epoch, request_ticket)` |
| `time_ns` | diagnostic only | not an identity |

Two further rules: a recycled slot is never accepted as the same object without
its generation, and **a name is immutable during one entry's lifetime; a rename
deletes and recreates the entry**.

### 5.3 Concurrency and the commit protocol

- One futex in the header. A writer takes it with an acquire CAS, publishes its
  PID and an owner token, mutates state, writes journal entries, publishes the
  new `commit_id`, clears the owner fields, and releases.
- `commit_state` is `IDLE`, `ACTIVE`, `GROWING`, or `BROKEN`. While a commit is
  active, readers retry until `IDLE` or `BROKEN`.
- The read is a seqlock: acquire `commit_state`, read `commit_id`, take a
  snapshot, re-read `commit_id` and `commit_state`. A matching pair with `IDLE`
  guarantees a coherent snapshot with no locks.
- **A commit is the atomic unit of visibility.** Two styles: **single-key** (one
  change) and **grouped** (several keys, one `commit_id`, several `journal_seq`,
  terminated by a `COMMIT_END` entry).
- **Last writer wins per key, ordered by `commit_id`. No per-value CAS at v1.**
- A grouped commit is **prepared in full before publication**: validate every
  value, allocate every frame, write every catalog and journal slot including
  `COMMIT_END`, and only then publish the new `commit_id`. If preparation fails,
  the commit is abandoned and the block is left exactly as it was.
- The one unrecoverable case is failure **after** `commit_id` is published, which
  is torn state: mark `BROKEN`, recreate with a new epoch. Recovery is fail-stop,
  never in-place rollback.
- A writer that dies after entering `ACTIVE` leaves partial state; a recovery
  authority may reclaim the futex only after proving the recorded writer is dead
  with a pidfd or equivalent start-time check (PID liveness alone is insufficient).

### 5.4 Block layout

One contiguous mapping, eight sections. The block starts at 4 MiB and can grow to
512 MiB via `mremap`; the header and fixed sections stay at stable offsets while
`arena_end` and `region_head` carve the growable tail.

| section | id | offset | contents |
|---|---|---|---|
| header | - | `0x000` | identity, commit futex, cursors, section table |
| section table | - | `0x100` | 16 rows of 32 bytes |
| catalog | 1 | `0x1000` | 16384 entry headers of 32 bytes |
| catalog index | 8 | `0x81000` | 32-byte header + 16384 slots of 16 bytes, sorted by name hash |
| journal | 3 | `0xC2000` | 32-byte ring metadata + 4096 slots of 64 bytes |
| requests | 4 | `0x103000` | 256 slots of 256 bytes |
| region desc | 5 | `0x113000` | 64 descriptors of 32 bytes |
| solved layout | 7 | `0x113800` | 16-byte header + 1024 nodes of 24 bytes |
| pool | 2/6 | `0x11C000` | growable arena, then region payload tail |

Key constants: `OMNI_MAGIC = 0x4F574D42`, `OMNI_FORMAT_VERSION = 1`,
`OMNI_ALIGN = 16`, `OMNI_HEADER_SIZE = 0x300`,
`OMNI_JOURNAL_CAPACITY = 4096`, `OMNI_CATALOG_SLOT_COUNT = 16384`,
`OMNI_SOLVED_SLOT_COUNT_MAX = 1024`, `OMNI_REGION_DESC_COUNT = 64`.

### 5.5 The catalog entry and the type system

A catalog entry is 32 bytes: `entry_generation`, `name_ref` (arena frame for an
immutable UTF-8 name), `type_tag`, `flags`, `length`, `body_ref`, and
`value_inline`. The name is not stored inline; it lives in the arena.

- **Any key name is accepted. No whitelist, no reject-at-write.** Namespace is by
  convention only: `wm.*` for core, `ext.<name>.*` for extensions.
- **Scope is expressed by exclusion.** An unmarked entry is ordinary
  configuration. `WINDOW_DEPENDENT` marks a value as window-scoped; `EPHEMERAL`
  marks a value with no long-term relevance. `save` and a soft reset both use the
  test "not `WINDOW_DEPENDENT` and not `EPHEMERAL`".
- **The type tag space is a u16.** Tags `0x01..0x34` are defined (bool, all integer
  and float widths, char, string, blob, duration, ratio, percent, keysym,
  keycode, modmask, point, size, rect, offset, vectors, colours, region_ref,
  entry_ref, enum, option, array, tuple, font, shader_ref, cursor_ref, binding,
  rule, gradient, path, exec, state, datetime, constraint, client_rule, map).
  Tags `0x8000` and up are reserved for extensions. `0x00` and `0x35..0x7FFF` are
  unassigned and refused as malformed.
- **Representation rule:** values whose encoded size is at most 8 bytes use
  `value_inline`; everything else uses an arena frame.
- **There is no schema, no defaults, and no semantic validation in the store.**
  The store guarantees a reader can decode a value or gets a named failure; every
  consumer independently decides whether to accept it.
- **The read path** hashes the name (FNV-1a 64 with a splitmix64 finalizer) and
  binary-searches the in-block catalog name index; it never walks the catalog.
  Hot reads use private-copy indexes rebuilt once per change. Change notification
  is a first-class event, and bindings get their own `BINDS_UPDATED` event.

### 5.6 The journal

One ring, carrying:

- `KEY_SET` and `KEY_DELETE` entries with full identity and the typed value.
- Free-form `EVENT` entries for region lifecycle, request completion, custom
  extension events, and coarse announcements.
- **Every visible commit appends at least one entry**, including growth and
  readiness changes. Grouped commits end with `COMMIT_END`.

Capacity is a compile-time constant at v1, enforced as a **ring with drop-oldest**.
Ring metadata is published with an even/odd `publish_seq` seqlock. A subscription
cursor is `(epoch, journal_seq)`, a last-seen marker.

- **Gap detection** is a normal condition the consumer detects at read time, not a
  store error. Group visibility is all-or-nothing in both gap policies.
- **Gap policy is per subscription:** `fail` (default, terminates with a
  `WATCH_GAP` and the lost range) or `resync` (emits a `WATCH_RESYNC` and
  continues). The block holds no policy field.
- **Epoch change is always terminal** (`WATCH_EPOCH_CHANGED`); resync covers loss
  within one epoch only.
- **Recycled identities in replay:** a replayed entry is applied only if its
  `entry_generation` still matches the live entry.

### 5.7 Guard tiers and the invariant table

Every structural invariant has a row in `configstorelayout.md` §12, assigned to
exactly one ordered tier with exactly one failure action. There are **51 rows**:
L1 10, L2 10, L3 17, L4 10, plus R 4 replay rows.

| tier | scope | failure action |
|---|---|---|
| L1 header | the block as a whole | refuse the whole block |
| L2 section | one section | drop that section; the block may still be refused based on `PRESENT` |
| L3 slot | one slot | skip that slot, continue |
| L4 value | one entry | skip that entry, continue |
| R replay | journal replay | skip-and-continue or policy resync, never partial application |

All four main tiers are **structural**: each answers whether these bytes can be
read as what they claim to be, never whether a value is sensible. A semantic tier
was considered and deleted.

Notable rules: a `PRESENT` flag **clear** means a section is absent by design
(not a failure); a `PRESENT` section that cannot be read refuses the block
(`OMNI_ERR_BLOCK_UNSUPPORTED`); a short block is refused at rest but skipped while
a growth commit is in flight (`OMNI_ERR_BLOCK_TRUNCATED`). All bounds arithmetic
is written subtraction-first (`offset <= limit and length <= limit - offset`).
Because bad data is designed for, the read path is deliberately fuzzable.

### 5.8 Regions and the request queue

- A **region** is a catalog entry of type `region_ref`; its descriptor lives in a
  fixed 64-entry table and its pixels in the pool. Descriptors record format,
  stride, slot count, `region_generation`, and `region_revision`.
- Publication is a double-buffer or single-buffer scheme with the descriptor
  pairs read before and after a copy; a changed pair or liveness change means the
  sample is discarded. A new descriptor generation always invalidates the cache.
- **Requests** are how socket-free programs ask the WM to create or destroy
  entries and regions. 256 fixed 256-byte slots, names held in the arena.
  Status is a state machine (`FREE`, `PENDING`, `DONE`, `ERROR`) where only the
  requester releases a terminal slot voluntarily, and the WM reclaims only on
  proven owner death or a terminal deadline that never applies to `PENDING`.

### 5.9 Persistence and discovery

- On startup the WM unlinks and recreates the block. Header fields have explicit
  initial values, and the mapping is zeroed before any field is written. Two
  values are defended in the docs because a naive implementation gets them wrong:
  `state` starts at `CREATING`, not `READY`, and `catalog_free_head` starts at the
  first unseeded slot (a fresh catalog is a full freelist).
- `OMNI_CONFIG_MAX_OPS = 4096` is a global limit on how many operations one config
  may contain. A file exceeding it is rejected whole, never truncated.
- A partial-invalid config still loads: line-level failures are logged and that
  line is skipped, never aborting the file.
- Discovery: the compositor sets `OMNI_INSTANCE_SIGNATURE` to the SHM file path;
  the socket lives at `<block-path>.sock`; fallback scans `$XDG_RUNTIME_DIR` for
  `omniwm-*.shm`. Permissions are 0600, same-user only, with no in-block auth.

## 6. The component substrate

`core/` is the component substrate, designed so that adding a feature is one
component: write its files, add one register function to the table in `server.c`.
It is specified in `helpers.md`, with the server lifecycle in `server.md`.

### 6.1 Descriptor versus instance

The hard split between two record types is load-bearing:

- **`struct omni_component`** is the immutable registration record: `name`,
  `priority`, `critical`, `enable_key`, arrays of options, actions, and triggers,
  and the `init`/`destroy`/`suspend`/`resume` callbacks. It holds **no mutable
  state at all**, so a client holding a descriptor pointer may trust it forever.
- **`struct omni_instance`** is the mutable runtime record, exactly one per
  activation: a borrowed descriptor pointer, a state, a diagnostic owner, an
  init errno, and initialised/suspended flags.

Instance states move between `SUSPENDED`, `ACTIVE`, and `FAILED`. A toggle moves
between `ACTIVE` and `SUSPENDED` and never destroys the instance or re-runs
`init`.

### 6.2 Registration, activation, and priority

Registration and activation are two phases. Every component ships one public
register function that appends descriptors via an append API; registration never
runs `init`. Activation happens in `omni_boot()`, which sorts descriptors by
priority (registration order as tiebreak) and, for each, creates an instance,
seeds options, subscribes triggers, registers actions, and calls `init`.

Priority bands:

| band | components |
|---|---|
| 0 | core (server, store, registry) |
| 10 | config facade (TOML, IPC bootstrap) |
| 100 | input |
| 200 | tags, windows |
| 300 | decorate, draw |
| 400 | ext-protocol, misc |

**Priority is advisory and is not a dependency edge.** There is no cycle check or
topological sort; a component activating before something it needs surfaces as an
init failure. The registry does not infer dependencies.

### 6.3 Readiness

The header carries a `ready` field separate from `state`. `state` says whether the
block is structurally trustworthy; `ready` says whether the compositor can service
requests. A `critical` component failure yields `FAILED`; a non-critical failure
yields `DEGRADED`; all critical components activating yields `READY`. `FAILED` is
**not** `BROKEN`: the block is intact and reads are still served; only writes are
refused with `NOT_READY` until `READY`.

### 6.4 Actions

`core/actions` is a namespaced handler registry feeding IPC `exec` and keybindings.

- `omni_action_register(name, handler, context)` returns a handle;
  `omni_action_release(handle)` is the only removal path. Unregistering by name is
  not offered.
- The first registration of a name wins; a later one is refused and logged.
- Dispatch validates the handler and the argument shape, then calls it. Three
  distinct codes: `ACTION_NOT_FOUND`, `ARGS_INVALID`, `ACTION_FAILED`. Collapsing
  them would make a broken action indistinguishable from a typo.
- An action's arguments are a positional, typed array validated against the
  action's declared schema before the handler is entered. Results are typed by a
  declared result tag, with `0` meaning "no result" (`null`).
- **A single action call is not a transaction.** Multi-step atomic changes must use
  a grouped `set`.

### 6.5 Triggers and events

`core/events` is journal-backed subscription: pattern to callback. A trigger is
exactly a `(pattern, callback)` pair; the pattern grammar is identical to IPC
`watch` (a single `*` wildcard). `omni_subscribe()` takes the same `gap_policy` as
IPC `watch`.

**The `omni_event` payload mirrors one journal slot and its field set is final.**
A synthetic in-process event is deliberately not expressible, because it would let
a dispatch deliver something no commit produced. `COMMIT_END` is enforced by the
dispatcher before delivery; a custom event arrives as an ordinary match.
Reentrancy is safe because a callback receives values already resolved from the
journal entry, never a pointer into live store state.

### 6.6 Lifecycle, threading, and shutdown

- **Every dispatch is a call on the event loop. There is no second thread of
  control.** This removes the need for a callback refcount or quiescence protocol.
  A component that starts a thread must join it inside `destroy` or `suspend`.
- **Shutdown is reverse activation order.** Close the socket, unlink the block,
  destroy each `ACTIVE` instance in reverse priority order, then release instance
  records last. A `FAILED` instance gets no `destroy`.
- **The server is owned by `core/server.c` and handed to components as a pointer
  inside their action or event context, never as a reachable global.**

### 6.7 Logging and utilities

- Logging is a Mango port with four meaningful levels (`SILENT`, `ERROR`, `INFO`,
  `DEBUG`, mirroring `wlr_log_importance`). `wm.log.level` is read before any
  component `init`; `ERROR` and above always reach stderr. Component tags are
  bound once into a thread-local slot.
- Utilities are a Mango port (`omni_die`, `omni_ecalloc`, non-block fd,
  `omni_regex_match` (PCRE2), list helpers, `omni_now_ms` as a u64, string
  helpers) plus a **container kit**: one dynamic array, one string builder, one
  open-addressed hash map, one ring buffer, one interning table, and intrusive
  `wl_list`. **No component writes its own container.**
- Categories explicitly **not** specified: geometry/rect math and path/filesystem
  helpers. Time is minimal (no monotonic/realtime distinction, no timer helper).

### 6.8 The extension boundary

There is **no in-process extension interface**: no host API, ABI version,
capability negotiation, or load/unload boundary. This is a rejection rather than a
deferral. A program in any language extends the compositor by writing to the block
and observing the journal. The trade is stated: such a program cannot be called
in-process or given a lifecycle by the WM, but it works in any language, survives
a restart, and needs no build-time coupling.

## 7. The layout engine

Layout is a constraint solver, not a set of layout functions. This is the
mechanism behind Any Layout. The engine is specified across `layoutengine.md`
(the solver), `layoutlanguage.md` (the normative syntax), and
`layout-test-examples.md` (a companion suite that finds awkward, ambiguous, or
missing parts of the language).

### 7.1 What a layout is

A layout is a **named program of soft constraints over spaces**, stored as data in
the block, with an optional viewport transform. It is not code, not a struct of
function pointers, and not an index into a fixed array. **A program names no
client.** The solver arranges *spaces* (placeholders a client maps to), never
windows, which decouples a program from a tag's current membership. This is
Mango's division kept deliberately; what differs is the mechanism underneath: a
soft-constraint program solved by weighted least squares rather than a direct
assignment of rectangles.

The engine is a **core service**, not a component. A *built-in layout* is the
opposite case and is a component.

### 7.2 The layout language

Four nouns:

| word | means |
|---|---|
| program | a named layout holding one rule list |
| space | a rectangle holding zero or one occupant |
| group | a space carrying a `layout`, so it holds a nested program |
| cluster | a runtime set of clients occupying one space as one occupant |

Program keys (all optional, each defaulting in consumer code independently):

```
omniwm.layouts.<name>.rules              array of rule
omniwm.layouts.<name>.spaces             array of space
omniwm.layouts.<name>.viewport           the viewport, at most one
omniwm.layouts.<name>.rearrange_on_focus boolean, at most one
```

Three of the four keys are structural and carry the program; the fourth,
`rearrange_on_focus`, is a boolean about when a pass runs with no representation
in the block.

**Ten program rule kinds**, each with a trigger family rather than a `when` key:

| `rule` | family | meaning |
|---|---|---|
| `run` | arrangement | lay members out in a line along an axis |
| `stack` | arrangement | every member fills the group, coincident |
| `grid` | arrangement | lay members out row-major into a grid |
| `scatter` | arrangement | members take the rectangles their clients carry |
| `split` | occupancy (full) | divide the full member's rectangle in two |
| `spawn` | occupancy (full) | add a member and give it to the leaver |
| `push` | occupancy (full) | move the leaver to another space |
| `float` | occupancy (full) | the leaver leaves the group entirely |
| `share` | geometry | the subject takes a fraction of the program |
| `inset` | geometry | the subject's edges pull in by a distance |

Binding rules: **rules are tried in order and the first that fits wins** (no
`priority`, no weight bands); a group has at most one arrangement rule and it must
come first; **the family is the trigger**. Only `share` and `inset` are true
geometry constraints and are solved by the weighted solver.

**Five client rule kinds** place a client inside a box it has been handed:
`align`, `match`, `size`, `offset`, and `snap`. Snapping has ten regions (four
halves, four quarters, `center`, `full`). A snapped client is floating.

**Two client namespaces:** `omniwm.clients.<set>.rules` (a set *is* a surface's
identity) and `omniwm.snaps.<set>.rules` (a set is a destination a move can be
sent to). The two share their keys, solver, and weights and nothing else.

**Frozen limits and deliberate absences:** no expressions or arithmetic, no
priority, no `when`, no `kind` on a space, no `members` or `group` field, one rule
list per program, no parent links, no user code or includes or conditionals, no
transforms other than `raise`, and no snapping at the program level.

There are **nineteen load-time validations** (thirteen program checks and six
client-layer checks), and they belong to the layout engine rather than to the
store because they are questions about meaning rather than bytes.

### 7.3 The solve

- The solve is **weighted least squares over the constraint system**. The
  reference implementation is DuckWM. It solves **per strongly-connected
  component**, **warms each component from the previous frame's solution**, and
  treats the highest-weighted equation whose variables are all solved as a fixed
  point.
- Exactly two weights: `OMNI_SOLVER_WEIGHT_PROGRAM` (60000) for `share`/`inset`
  and `OMNI_SOLVER_WEIGHT_CLIENT_RULE` (6000) for snapping and sizing. The decade
  between them means a client relation loses the compromise to a program relation.
  Neither value can be written by a program.
- **The solver degrades, it does not fail.** When constraints cannot be satisfied,
  nothing is discarded; the solver returns the geometry minimising total weighted
  violation. There is no hard constraint, so "unsatisfiable" is not a detectable
  event: there is only a geometry with a cost.
- **Determinism is a requirement:** the solve is a pure function of the committed
  block state. Iteration order is defined rather than inherited from a hash
  iteration, and the warm start is keyed on something stable across an animation.

### 7.4 The update pipeline

The arrange cycle is nine steps:

1. a call to update the layout is made;
2. the cluster resolution pass runs, collapsing each cluster to one virtual client;
3. the mapping phase assigns clients to spaces (every membership change happens here);
4. the solver solves the new layout, over spaces;
5. the relaxation pass runs, if the program has one;
6. the solved layout is written but not active;
7. animations are triggered, if enabled;
8. the animation runs (skipped if disabled);
9. the new layout is arranged, which activates it.

Mapping is before the solve because a reflow is a membership change and a layout
that has not decided which client is in which space has nothing to solve for.
Resolution (moving a client from one space to another) is a mapping operation and
is deliberately outside the solver, because the solver is a pure function of
committed state. `push` termination is guaranteed by a strictly decreasing
`order` rank checked at load.

### 7.5 Layout state

| state | home | persistence |
|---|---|---|
| a program | catalog entry under `omniwm.layouts.<name>`, tag `0x32` | ordinary configuration |
| a user's layout name | catalog entry name | exists only once written |
| `rearrange_on_focus` | same entry | never seeded into the block |
| a tag's layout name | a value under the tag's entry | ordinary configuration |
| tag membership | one `WINDOW_DEPENDENT` child per member, on the tag | excluded from `save` |

The solved geometry lives in section 7 (`OMNI_SECTION_SOLVED_LAYOUT`), a fixed
single-buffered, non-journalled section of up to 1024 nodes of 24 bytes, each
keyed by `(entry_id, entry_generation)` with canvas coordinates.

## 8. Tags

Tags are the project's model of "where a window is", and the one place where
omniWM makes a deliberate, differently-shaped break with Mango.

### 8.1 Identity

A tag is a container of three things: a client set, a layout, and other tag data.
It does **not** own floating state, because floating is a fact about a client.

**A tag is identified by the pair (monitor, tag number), and the monitor is half
of the identity.** Monitor 1's tag 3 and monitor 2's tag 3 are unrelated tags. A
per-monitor tag lives under its monitor's entry (`wm.monitor.<id>.tag.<n>`), so
the monitor half is structural: the parent *is* the monitor. A singleton tag (the
shared scratchpad) has no monitor in its identity and lives at `wm.tag.<name>`.

The break with Mango is that the bitmask and the client-held mask are gone, so
there is no way to name a tag without naming its monitor. **Mirroring is the bug
the per-monitor identity makes unrepresentable**, not a feature that was given up.

### 8.2 Display

A monitor displays an **ordered list of its own tags**, not a set, stored at
`wm.monitor.<id>.tags`. Order is chronological and the list is not re-sorted. The
first tag still set is the **primary**, and it supplies the layout and everything
else the tag holds. A secondary tag contributes its clients and nothing else.
**The primary is derived from list order, not stored.** There is no operation that
reorders the list in place.

The solver's input is the primary's layout, the union of the client sets of every
tag in the monitor's list, and each client's own floating state.

### 8.3 The content swap

There is no operation that moves a tag, because under the per-monitor identity a
tag that appeared on another monitor would be a different tag. What users call a
move is a **content swap**: the two tags stay where they are and their clients,
layout, and viewport swap places. **The whole swap is one IPC operation and one
grouped commit**, so no reader sees a half-finished state. The two arguments are
full keypaths (`wm.monitor.1.tag.3`, `wm.monitor.2.tag.7`).

### 8.4 Special tags

- **Overview** is per monitor. Its client set is the union of every *other* tag
  on that monitor, so it is the degenerate case of the union rule and needs no
  special case in the solver.
- **Scratchpad** has a **layout of its own** and is an **overlay workspace**:
  summoning it places its clients over the top of the monitor's existing view.
  It is **outside the union**: no other tag's layout places its clients.
  Membership is retained and participation is withheld, so dismissal is a single
  membership deletion. It can be **shared across monitors** (`wm.scratchpad.shared`,
  bool, default `true`, openable on at most one monitor at a time) or **one per
  monitor**; both are the same code path with N tags.

Focus, hiding, and minimisation are three distinct operations. Focus is not a
membership mutation; hiding into the scratchpad is an addition; minimisation is a
per-client visibility state and neither of the others.

### 8.5 Store obligations

The tag's entry is a container, which requires the store to support three
operations, each distinct:

- **subtree delete** frees frames and is how client teardown removes membership;
- **subtree exchange** frees nothing, because every child of one tag is still live
  in the other, so it is not a special case of delete;
- **generation-correct deletion**, so a recycled slot is not mistaken for the old
  client.

A reverse index is not required.

## 9. Windows, groups, and clusters

The window layer owns clients, the floating layer, focus and stacking, and
per-window identity. `windows.md` calls itself a prototype; phase 06 will make it
a specification.

### 9.1 Clients

A client is a **catalog entry**, exactly like a tag, identified by `entry_ref`
(`(epoch, entry_id, entry_generation)`). Per-client values are children of that
entry and are `WINDOW_DEPENDENT`.

The value table splits into **observable** fields (protocol facts mirrored into
the block so a rule can filter on them: `kind`, `appid`, `title`, `pid`,
`mapped`, `focus`, `cluster`, `layer`) and **settable** fields (preferences:
`floating`, `visible`, decoration values, geometry pins, set bindings). A rule
must not be able to overwrite the identity of the client it matches. A client
does **not** store where it is; the solved section stores where it is going, and
the client keeps the previous solved rect so animation has a start point.

### 9.2 Client kinds

The kind set is **closed at seven**: `XDG_TOPLEVEL`, `LAYER_SHELL`, `X11`,
`SNAPSHOT`, `XDG_POPUP`, `XDG_INPUT_METHOD_POPUP`, and `FAKE_CLIENT` (omniWM's
own). Mango's sixth protocol kind, `GroupBar`, is not carried over because
omniWM's group bar is a placed participant rather than chrome. A kind is a tag on
the client, never a branching site.

### 9.3 Groups and clusters

- A **group** is a nested layout with a titlebar. It is picked up and moved as one
  window. Its titlebar is a fake client, not a decoration.
- A **cluster** is several windows tied together so they move as one unit. It adds
  no chrome of its own and **occupies one space of its parent**, which is why the
  update pipeline is nine steps: a cluster is resolved to a single virtual client
  before the program places anything. Its members stay individually focusable and
  individually interactive, are not raised as a unit, and are not one focus target.
- A **group raises its members as a unit on focus; a cluster does not.**

### 9.4 Window rules

A window rule is a **declarative seed applied once**, when a client is created.
The block carries no record that it fired. A rule has an `if` block (matched as
an unanchored PCRE2 regex against any observable client field) and a `then` block
(which seeds values and binds named client-rule sets). **A rule with no `if` is
inert, not match-all.** All filters are evaluated against the pre-rule state, then
all `then` blocks apply in array order. The rule list is sorted by name, and the
name is the order, which answers seed precedence, multi-set resolution order, and
tie-breaks in one place. Re-applying is a capability, never an automatic
consequence of a remap.

`appid` and `title` are not in the block natively, so they become client fields
(written at creation, refreshed on change, empty string when unset) so that a
block-resident rule can match on them.

### 9.5 Focus and floating

- One focus pointer per monitor plus a remembered predecessor.
- Focus recency is a separate list from stacking order, so "next" and "previous"
  survive a layout that reorders the scene.
- Raising is a parameter of focusing, not a side effect.
- **Floating is a per-client `WINDOW_DEPENDENT` value.** A floating client is
  excluded from every layout program and drawn in the floating layer. A client
  that floats floats everywhere it appears.

### 9.6 Fake clients

A fake client is a widget the compositor draws that behaves like a window: bars,
panels, launchers, on-screen displays, counters. It is a catalog entry, has the
seven-kind `kind`, and takes part in layout and focus exactly as a real window
does. **Participation in focus is settled; whether a fake client holds keyboard
focus is a per-client property defaulting to non-focusable, deferred to phase 06.**

## 10. The socket API (IPC)

The socket is a thin JSON facade over the store primitives. The full protocol is
`ipc.md`; the design summary follows.

### 10.1 Transport and framing

- `AF_UNIX`, `SOCK_STREAM`, non-blocking, served from the WM event loop.
- Path is `<block-path>.sock` beside the SHM file; permissions 0600, same-user
  only; backlog 16; no handshake (a new connection is a new watcher).
- **Newline-delimited JSON (NDJSON), one object per message.** A new connection
  is a new watcher.
- Limits: `OMNI_SOCK_MAX_LINE` 1 MiB, `OMNI_SOCK_SEND_BUF` 64 KiB,
  `OMNI_SOCK_STALL_MS` 2000, `OMNI_SOCK_DISCONNECT_MS` 60000.
- Partial reads have per-connection accumulators; the line-cap check happens as
  bytes accumulate. Partial writes are driven by socket readiness, never treated
  as a liveness signal.
- Within one connection, responses and pushes preserve production order. Between
  connections there is no ordering guarantee; a client needing read-after-write
  uses the ordered `commit_id` in a response.

### 10.2 Commands

| command | purpose |
|---|---|
| `get <key>` | read the current value of one key |
| `set <key> <typed value>` or grouped | one commit, single or grouped |
| `watch <pattern> [since] [gap_policy]` | journal-forward event stream |
| `unwatch <sub>` | close one subscription |
| `delete <key>` | remove an entry outright |
| `exec <action> [args]` | invoke a registered action |
| `save [path] [pattern]` | serialise configuration to TOML |
| `reload <path>` | replay a config file as one grouped commit |
| `reset <mode>` | soft or hard reset |
| `swap_tags <a> <b>` | exchange two tags' contents |

`set` maps to the commit protocol. `reload` is the transaction boundary: a file
that fails to parse never begins a commit, and a successful reload is one grouped
commit. `save` excludes `WINDOW_DEPENDENT` and `EPHEMERAL` entries, is written to
a temp path and renamed into place, and round-trips through the built-in parser.
`exec` invokes an action by namespace-qualified name and is **not** a transaction.

### 10.3 Errors and readiness

There is one `OMNI_ERR_*` result vocabulary shared by the read path, the request
queue, and the socket, plus `OMNI_SOCK_ERR_*` for wrapper-only facts. `KEY_NOT_FOUND`
is an absence, not a failure; `VALUE_UNREADABLE` is a fact about the block. Reads
are served in every readiness state; writes are refused with `NOT_READY` until
`READY`. A key the core does not understand is stored and served verbatim.

## 11. The TOML parser

The built-in TOML parser is a **facade, not a second source of truth**. It parses
a file into a private staging area and commits it through the same primitives the
socket uses, so a value written from a file and the same value written over IPC
are byte-identical in the block in both directions. It reuses the IPC encoder and
decoder verbatim, so a watcher cannot tell which surface produced a commit.

- TOML literals bind to the core tags and add none of their own; the tag is read
  off the key. All four TOML date/time forms land on the single `datetime` tag.
- Tables, arrays of tables, dotted keys, and quoting flatten to dotted key paths.
- Known keys decode to their declared tag; unknown keys get an inferred type
  (`i64` for integers, `f64` for floats).
- Errors fail per line, never as a whole, except the structural `OMNI_CONFIG_MAX_OPS`
  limit which rejects the file whole.
- The parser also drives a **key registration helper** so a component can declare
  a key's type, which is what makes a bare integer unambiguous.

## 12. Input

`input.md` is explicitly **not** a second design of input: it is the file-level
port map from Mango, the three seams the port must get right, and the stage 4
versus stage 10 split.

- Input is **Mango's implementation, ported and adapted**: 4,775 lines across 14
  files covering keyboard, mouse buttons, the wheel, touchpad gestures, the switch
  (lid), tablet plumbing, and touch-to-pointer emulation.
- The only part replaced rather than moved is the binding-matching loops, which
  are replaced with a hash index (see below). `binds.c` is fresh work, 2,674
  lines, the translation of Mango's function pointers into names and schemas.
- **The three seams:** the block replaces Mango's config arrays (each binding is
  one catalog entry); actions replace function pointers (`func` becomes
  `action_ref`, a frame offset holding the action's name; Mango's `Arg` becomes a
  positional typed `args` array); omniWM's namespaces and tags replace Mango's
  modes.
- **The binding index:** a process-private hash table keyed on
  `(normalized modmask, trigger, mode_id)`, mapping to a candidate list in config
  order with a cached resolved action handle. The last match in config order wins,
  which is the block's own overwrite semantics arriving at the index. The index
  is maintained incrementally from the journal and is derived state, rebuildable
  from the block at any time.
- Input is a component at priority 100, with `enable_key = "wm.input.enabled"`
  default `true`, and two timing options (`wm.input.axis_apply_timeout_ms`,
  default 100 clamped 0..1000, and `wm.input.key_order`).
- **Stage 4** covers devices, keyboard, binds, and input options. **Stage 10**
  covers mouse, trackpad, switch, touch, tablet, and the stylus binding type. The
  split exists because the pointer and trackpad files reach directly into client
  and monitor structs that do not exist at stage 4.
- **The mode and tag mapping is the substantive open question:** Mango's `mode`
  is folded into a binding at parse time, and omniWM has no key mode. Four
  candidate mappings are recorded and none is chosen. The index is already shaped
  for an integer `mode_id`, so the answer will be a u32 rather than a string.

## 13. Monitors and outputs (promised, not designed)

`generaldesign.md` §15 promises outputs support:

- **hotplug** (an output appearing or disappearing reconfigures layout without a
  restart);
- **per-output configuration** matched by name, make, model, or serial, setting
  scale, position, refresh, and layout defaults;
- **virtual monitors** (headless outputs);
- **Xwayland** running X11 clients inside the same tags, layouts, and decorations
  as Wayland clients.

`monitor.md` is not written. `devnotes/design-phases/04-monitor.md` owns it, and
it reads `tags.md` (which owns the monitor tag list) rather than the reverse.

## 14. The look pipeline (promised, not designed)

Three documents own the visual design and none is written yet. The design phases
deliberately order them **draw, decorate, animate**, because both decoration and
animation need the node model that drawing owns.

- **Draw** (`draw.md`) owns the scene-graph node model, backgrounds, the infinite
  panning canvas, vector line objects, text and images, compositor-drawn
  surfaces, and the bridge from externally rendered regions to nodes. Its first
  question is the roadmap inversion: the node model comes first, and the solver's
  output shape *is* the node shape.
- **Decorate** (`decorate.md`) owns per-client and per-output decoration:
  multi-layer borders, shadows, blur, glow, opacity, rounding, overlays and
  underlays, dimming, and the three override routes. It depends on the node model.
- **Animate** (`animate.md`) owns animation as a configurable, store-resident
  subsystem: timelines declared in config, animatable properties on nodes,
  shader-driven animation, and the three named answers for a mid-flight update
  (**retarget**, **queue**, **replace**), with the scratchpad a distinct fourth
  case.

`generaldesign.md` §9 to §12 state the intent: one scene graph, animation as a
node property driven by config timelines, decoration configured globally and
individualised per window, and the drawing layer producing every pixel the
compositor draws itself.

## 15. Protocols and external rendering (promised, not designed)

The protocol surface follows MangoWM's ("use where we can, change what we need").
The compositor instantiates the wlroots protocol managers it needs and adds the
Mango set on top. `protocols.md` is not written.

External rendering comes in two stages: **regions first** (an external program
paints into a framebuffer region and stores a region reference that the compositor
uploads as a scene node, reusing the existing region mechanism with no new
protocol), then a **buffer-injection protocol** so an external renderer can hand
the compositor a real buffer for the GPU path. The motivating case is compositing
an externally rendered scene (for example a terminal rendered as a CRT on an
object flying past). omniWM offers the compositing capability; what to render is
the external program's business.

## 16. Any Language: libraries and interpreters (promised, not designed)

The endgame of Any Language is that a first-class library in another language
requires no change to the compositor. The demonstration programs are:

- a C reference client library that maps the block using `include/shared/` alone;
- a Python reference scripting library;
- an interpreter for Mango configs;
- an interpreter for Hyprland configs.

**None of these is a deliverable of the window manager.** They come after the
project is otherwise complete, and they are separate programs with no
cross-dependency in either direction. They share exactly one thing with the
compositor: they expect the SHM block to exist. The reason is substantive: a
language-specific component inside the compositor would falsify the claim it is
meant to support, because the surface would become "Lua with a C wrapper" rather
than language-neutral. This also closes the Lua question: nothing language-specific
is in the compositor in either direction.

## 17. Build and test design

### 17.1 Build

- Meson (>= 1.3), C11, `warning_level=2`. Options: `xwayland` (feature, default
  enabled) and `asan` (boolean, default false).
- Dependencies: `wlroots-0.20 >= 0.20.0`, `scenefx-0.5 >= 0.5.0`,
  `wayland-server >= 1.23.1`, `libinput >= 1.27.1`, `libdrm`, `xcb`, `xcb-icccm`,
  `xkbcommon`, `wayland-client`, `libpcre2-8`, `pixman-1`, `libcjson`,
  `pangocairo`, and `libm`.
- Three shared libraries are produced: `libomniwm` (everything in `src/` except
  the entry point), `libomniwm-ipc` (the socket facade, a separate library even
  though it runs in-process), and `libomniwm-debugger` (the SHM block visual
  analyser, deliberately not a dependency of the WM).
- Two executables (`omniwm`, `omniwm-debug`) are **commented out** until the
  substrate lands, because `src/main.c` is zero bytes. The WM executable links
  `libomniwm-ipc` and not `libomniwm` directly, so the in-process path is not
  available to the WM alongside the socket path.
- **The one target that builds today is the ABI header check.** `cc.compiles`
  includes `shared/omni_layout.h` at `meson setup` time, and every
  `OMNI_STATIC_ASSERT` in the header is its executable content. A wrong byte-level
  constant is a build failure rather than a disagreement between a header and a
  document. This is the one property in the design that is verified rather than
  asserted.
- The Nix flake exposes four packages (`libomniwm`, `libomniwm-ipc`,
  `libomniwm-debugger`, `omniwm-debug`), an overlay, and `nix run` for the ABI
  check. `.clang-format` exists.

### 17.2 Testing

`testing.md` (the phase 01 document) states the framework, the seam, and tests
only the components already designed when it was written.

- **Ownership rule:** a test for a component lives in that component's design
  phase. This document owns the framework and the seam.
- **The seam:** `libomniwm` is an internal development library that links wlroots
  and scenefx directly, so a component test uses the real dependencies rather than
  stubs. Pure components (store, parsers, solver) run in the unit runner; wlroots
  bound components run under a wlroots test harness. **There is no fake wlroots.**
  Time is an injected `omni_clock`.
- **Store write/read** is the assertion backbone; the coverage matrix maps every
  one of the 51 guard-tier rows to a named assertion. The **store fuzz target** is
  a separate binary (`omniwm-fuzz`) whose pass condition is "no crash and correct
  skip-and-continue", with a per-tier corpus.
- **The layout solve** is tested for the solve half only; the pass half belongs to
  phase 07. Determinism is tested by feeding a shuffled input order and asserting
  reproducible output bytes, because nothing may be ordered by address or hash.
- **The TOML parser** is tested primarily for facade equality: a value written
  from a file and the same value written over IPC are byte-identical.
- Tests register with `OMNI_TEST(name)`, are one source file per unit named
  `test_<unit>.c`, and run in one binary `omniwm-test` built only when `tests` is
  enabled. A registration-count check makes a test that failed to compile a build
  failure rather than a silent green.

## 18. The design process itself

The design is being produced through a documented process, and the process is
part of the snapshot because the documents reference it constantly.

- **Document authority.** Each subject has exactly one owning document. Anything
  stated in two places is treated as a bug. `include/shared/omni_layout.h` is the
  single home for every ABI constant. `layoutlanguage.md` is normative for layout
  syntax; `configstorage.md`/`configstorelayout.md` own the store; `helpers.md`
  owns the substrate; `server.md` owns the server lifecycle.
- **Design phases.** `devnotes/design-phases/` is a plan for finishing the design,
  one file per phase, numbered 00 to 12. **A design phase delivers design, not
  code.** The design lives in the devnotes documents; the phase directory says what
  is left to do, in what order, and why.

| phase | subject | status |
|---|---|---|
| 00 | reconciliation of the documents that exist | complete |
| 01 | `testing.md` | complete |
| 02 | `build.md`, `licence.md`, the internal codebase design | pending |
| 03 | `input.md` finalization | pending |
| 04 | `monitor.md` | pending |
| 05 | `tags.md` finalization | pending |
| 06 | `windows.md` finalization | pending |
| 07 | `layoutengine.md` finalization | pending |
| 08 | `draw.md` | pending |
| 09 | `decorate.md` | pending |
| 10 | `animate.md` | pending |
| 11 | `protocols.md` | pending |
| 12 | `languages.md` | pending |

- **Phase 00 (reconciliation)** closed ten authoritative decisions before any new
  document was written. The most load-bearing: one shared `OMNI_ERR_*` result
  vocabulary; four guard tiers plus a replay tier with 51 rows and no semantic
  tier; the three-key layout program (`rules`, `spaces`, `viewport`, plus the
  boolean `rearrange_on_focus`); explicit creation-time header initial values
  (`state` starts `CREATING`; a fresh catalog is a full freelist); one buffer with
  two stages for solved versus arranged layout; two solver weights a decade apart;
  the membership walk at ascending `entry_id`; a seven-member closed client kind
  set; a group raising as a unit and a cluster not; the `omni_event` payload final;
  stored not derived tag membership; and the removal of a never-existent
  `xref.py`/`lcheck.py` gate.
- **The reconciliation also resolved a real contradiction:** the scratchpad's
  "exclusivity" is about arrangement, not membership, because the strong reading
  (deleting ordinary membership when parked) is unimplementable and would require
  Mango's hidden second copy of the truth.
- **Work logs are explicitly not maintained.** The dated sections at the end of
  files are records of what happened, not living text, and a work log disagreeing
  with a live section is a property, not a defect.

## 19. Known risks and open questions

These are the risks and open items as of this date. An item with an owner is an
open question; an item without one is a bug.

- **The scenefx extension set, and patch versus fork.** The largest external risk
  in the project. Owned by phase 02 items B2 and B3.
- **Which wlroots release the build tracks.** Unresolved; coupled to scenefx 0.5
  requiring wlroots 0.20. Owned by phase 02 item B2.
- **The roadmap inversion in the look phases.** The roadmap puts basic
  decorations at stage 8 and the scene graph at stages 12 to 14, but decorate and
  animate both need the node model that draw owns. Phase 07 decided the node model
  comes first; phase 08 owns the rest.
- **Input's mode and tag mapping.** Four candidates, none chosen. Owned by phase
  03. The index is already shaped for an integer `mode_id`.
- **The document set is incomplete.** Not yet written: `monitor.md`, `draw.md`,
  `decorate.md`, `animate.md`, `protocols.md`, `languages.md`, `licence.md`
  (GPL-3.0 is the decision but the file does not exist), tag rules and per-output
  tag defaults, the tag surface syntax, the bar-side surface, the client protocol
  surface, and the storage for a deferred tag seed.
- **The layout engine is the most complete document** but the solver's *pass*
  half (what triggers a pass, when the first pass may run, whether it is
  idempotent) is unfinished, owned by phase 07 items 15 to 17.
- **Audit findings.** Two audits exist: `architecture-audit.md` (2026-09-25,
  marked historical) and `audit-2-findings.md` (stage-readiness, with blockers).
  Several audit-2 blockers were resolved by phase 00 (the `action_ref` semantic,
  the three-key program shape, the `0x34` known-tag range, the MAP wire form), but
  the audits remain the place known constraints are recorded. Audit-2's verdict
  was that stage 1 is implementable after a cleanup pass while stages 2 to 4 were
  blocked on the phantom tags, missing constants, stale encodings, and the
  stage 4 versus stage 6 dependency cycle; the design documents have moved since.
- **The build has no commented-out explicitness about the stage 4 versus stage 6
  dependency:** `input.md` cannot finish before `layoutengine.md` because the snap
  operation needs the solver's pointer placement, and the solver needs input's
  keybind direction. This cycle is a roadmap-ordering question rather than a design
  one.

## 20. What is actually implemented today

Almost nothing, and that is the intended state.

- `include/shared/omni_layout.h` (1182 lines) is the only file with content. It
  holds every ABI constant, the type-tag table, the composite payload field
  offsets, the section geometry, the error vocabulary, and a set of
  `OMNI_STATIC_ASSERT`s that make a wrong constant a build failure.
- All other `.c` and `.h` files (118 files) are zero bytes. They are the
  scaffolded shape of the tree, kept because it is the targets' shape rather than
  their implementation.
- Three shared libraries build and link with every source inside them empty, so
  they carry no symbols. `nix build` installs the header and `libomniwm-ipc.so`;
  `nix run` runs the ABI check.

## 21. Document map

| subject | document |
|---|---|
| whole-system shape, promises, the compositor | `devnotes/generaldesign.md` |
| store concepts, identity, journal, requests, regions | `devnotes/configstorage.md` |
| store bytes, offsets, capacities, guard rows | `devnotes/configstorelayout.md` |
| the ABI header (single home for constants) | `include/shared/omni_layout.h` |
| component substrate, actions, triggers, logging, containers | `devnotes/helpers.md` |
| server lifecycle, boot order, readiness, shutdown | `devnotes/server.md` |
| socket protocol, commands, JSON encoding | `devnotes/ipc.md` |
| TOML facade and key registration | `devnotes/tomlparser.md` |
| solver, pipeline, determinism, weights | `devnotes/layoutengine.md` |
| normative layout syntax, rules, guards | `devnotes/layoutlanguage.md` |
| layout language companion suite | `devnotes/layout-test-examples.md` |
| tags, display, special tags, content swap | `devnotes/tags.md` |
| clients, groups, clusters, window rules, fake clients | `devnotes/windows.md` |
| input port map and seams | `devnotes/input.md` |
| test framework and seam | `devnotes/testing.md` |
| file and directory boundaries | `devnotes/filestructure.md` |
| build system | `devnotes/build.md` |
| design phase plan | `devnotes/design-phases/` |
| build-stage plan | `devnotes/plan-phases/` |
| audits | `devnotes/audits/` |
| unwritten-topic inventory | `devnotes/missing-devnotes-topics.md` |

## 22. Source tree

The tree is scaffolded and empty. The intended boundaries (`filestructure.md`):

```
src/core/        component substrate: server, registry, component, events,
                 actions, log, util, layoutengine
src/config/      store: parse, store, types
src/ipc/         socket facade
src/windows/     clients, groups, clusters, layout glue, constraints,
                 transforms, fake clients
src/monitor/     monitors, virtual monitors, Xwayland primary
src/input/       devices, keyboard, mouse, trackpad, switch, touch, tablet,
                 gestures, binds, input
src/tags/        tags, scratchpad, overview
src/draw/        renderers, shaders, background, vector, text, image, button
src/decorate/    borders, shadows, blur, glow, opacity, gradient, overlay,
                 underlay, textures
src/animate/     client, layer, fakeclient
src/ext-protocol/ foreign-toplevel, xdg-output, xdg-activation, hdr, tearing,
                 text-input, ext-workspace
include/         mirrored headers, plus include/shared/ (the ABI) and
                 include/commonheaders.h
ipc/             the socket facade, outside src/ per filestructure.md
debugger/        the SHM block visual analyser
protocols/       wlr-foreign-toplevel-management, wlr-output-power-management,
                 wlr-layer-shell
nix/             common.nix, default.nix, abi-check.nix
assets/ docs/    placeholders
```

Two paths have no Mango counterpart and are where omniWM is Mango's shape but not
Mango's file list: `ipc/` (the socket facade, outside `src/`) and `debugger/`
(the SHM block visual analyser).

## 23. Glossary

- **block / store**: the shared-memory configuration block, the single authority.
- **catalog**: the open, schema-less table of typed values in the block.
- **entry_ref**: `(epoch, entry_id, entry_generation)`, the identity of any
  catalog entity.
- **commit**: the atomic unit of visibility; single-key or grouped.
- **journal**: the append-only ring of change events; the channel every writer
  shares.
- **catalog index**: an in-block, hash-sorted index that makes `get` a binary
  search rather than a walk.
- **component**: an optional, toggleable capability in the substrate; contrast
  core service.
- **core service**: a capability the compositor cannot run without (store, layout
  engine, input router, registry).
- **program**: a named layout, a soft-constraint tree over spaces.
- **space**: a rectangle holding zero or one occupant; the layout engine arranges
  spaces, never clients.
- **group**: a space carrying a nested program; has chrome and a titlebar.
- **cluster**: several clients occupying one space as one occupant; no chrome.
- **tag**: the project's model of "where a window is"; identified by
  `(monitor, number)` or as a singleton.
- **primary tag**: the first still-set tag in a monitor's ordered list; supplies
  the layout.
- **swap_tags**: the operation that exchanges two tags' contents in one commit.
- **fake client**: a compositor-drawn widget that takes part in layout and focus.
- **window rule**: a one-shot declarative seed applied at client creation.
- **binding**: a catalog entry mapping an input trigger to a named action.
- **action**: a namespaced, schema-typed handler the socket, a binding, or a rule
  can invoke.
- **trigger**: a `(pattern, callback)` subscription to the journal.
- **region**: a framebuffer rectangle an external program can paint into.
- **facade**: a client of the block that is not privileged (the TOML parser, the
  socket).
- **WINDOW_DEPENDENT / EPHEMERAL**: scope flags excluded from `save` and reset.
```
