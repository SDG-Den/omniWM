
in order to allow third-party libraries to interact with and configure the WM, the live configuration state must be readable and writeable.


I want to achieve this in two ways:

#1: the entire live configuration state will be stored in shared memory

#2: a socket will be exposed that interacts with this live configuration state, which allows IPCs to work as well. 


because of this, it might be a good idea to first build out the "generalized" storage layout and methods for this shared memory block.

---

# Authoritative shared-memory config store: expanded concept

## 0. Core principles

- The SHM block is authoritative over everything. The socket, the built-in TOML parser, internal WM code, and third-party libraries are all facilities over the same block. A well-written omniWM library should not need the socket at all.
- A config file is saved state: a sequence of `set`/`exec` operations replayed into the block at startup.
- The storage layer is an open catalog. Any key may exist, with self-describing values. There is no key-specific code in the core storage or IPC path. Extension layers create keys and are responsible for acting on them.
- Guards-at-consumption: bad data is explicitly allowed to exist in the block and is refused only when a consumer parses it. The block is treated as hostile input on every read.
- Single-write-at-a-time: one commit futex in the header serializes writers. Readers remain lock-free but only accept an idle, commit-id-stable snapshot. The compositor is single-threaded, so it never races with itself; the futex arbitrates outside writers and carries an owner identity for recovery.
- Whole-block structural safety is the core WM's own discipline, not a contract imposed on third-party libraries (libraries decide their own level of defensiveness; the example library may ship safe-reader helpers).

## 0.1 Identity vocabulary

- `epoch` identifies one compositor instance. It changes on every restart and is never reused while stale mappings may exist.
- `commit_id` identifies one atomic state commit. It increases once per commit, including grouped commits, growth, and readiness changes.
- `journal_seq` identifies one journal entry. It increases once per appended entry; grouped entries share a `commit_id` but never a `journal_seq`.
- `entry_id` identifies a catalog slot, while `entry_generation` identifies that slot's allocation lifetime. A key reference is `(epoch, entry_id, entry_generation)`.
- `region_generation` identifies a region descriptor lifetime, while `region_revision` identifies a published image version within that lifetime.
- `request_ticket` identifies a request within an epoch; the complete request identity is `(epoch, request_ticket)`.
- `time_ns` is diagnostic data, not an identity. All numeric identities are scoped to their epoch and do not wrap during that epoch.
- A recycled slot or descriptor is never accepted as the same object without its generation. Names are immutable during one catalog entry lifetime; a rename creates a new entry identity.

## 1. Concurrency model

- One futex lives in the block header. A writer takes it with an acquire CAS, publishes its PID and a per-acquisition owner token, mutates state, writes journal entries, publishes the new `commit_id`, clears the owner fields, and releases. Uncontended acquisition is a single atomic instruction; only on contention does the kernel sleep the waiter.
- The header `commit_state` is `IDLE`, `ACTIVE`, `GROWING`, or `BROKEN`. While a commit is active, readers do not interpret state; they retry until the state returns to `IDLE` or the block is reported `BROKEN`.
- Commit-id counter: readers acquire `commit_state`, read `commit_id`, take a snapshot, then acquire `commit_id` and `commit_state` again. A matching pair with `IDLE` state guarantees a coherent snapshot with no locks and no kernel involvement.
- Commit = the atomic unit of visibility. Two commit styles are both supported:
  - Single-key commits: one key change, one `commit_id`, and one journal entry with one `journal_seq`.
  - Grouped (multi-key) commits: several related keys changed in one commit, one `commit_id`, and several journal entries with distinct `journal_seq` values. A `COMMIT_END` journal entry marks the end of the group.
- Last writer wins for any given key, ordered by `commit_id`. No per-value CAS at v1.
- A writer that dies after entering `ACTIVE` leaves an unrecoverable partial state. Recovery is fail-stop: the compositor marks the block `BROKEN` and recreates it with a new `epoch`; no in-place rollback is attempted.

### 1.1 A commit either applies or leaves no trace

The consumer side of group visibility is already defined: entries whose
`commit_id` has no `COMMIT_END` in the ring are incomplete and none of them are
applied. The writer side needs the matching rule, because a grouped commit can
fail partway through preparing and the two halves of the question are different
questions.

A grouped commit is prepared in full before it is published. The writer
validates every value, allocates every arena frame, writes every catalog slot,
and writes every journal slot including the `COMMIT_END`, and only then
publishes the new `commit_id` and returns `commit_state` to `IDLE`. Readers see
nothing until that publication, because `commit_id` is the only publication
point and the seqlock keeps the journal boundary stable until then.

If preparation fails, the commit is abandoned before publication and the block
is left exactly as it was:

- nothing is published, so no reader ever sees a `commit_id` with a partial
  group;
- the `COMMIT_END` slot is written last, so a group that never reached
  publication has no marker and is correctly invisible;
- arena frames allocated by the failed attempt are reclaimed from the bump
  cursor before the commit returns, since nothing was published and no reference
  to them can exist.

The one exception is a failure *after* `commit_id` has been published, which is
by definition not an abandoned commit but a torn one. That is the fail-stop
case above: the block goes `BROKEN` and the epoch changes. A prepared-but-
unpublished commit is recoverable; a published-but-inconsistent one is not, and
the boundary between them is the publication of `commit_id`.

This is what makes a whole-file reload safe. The file is parsed into a private
staging area first, so a config that fails on line 40 never began a commit, and
the live block is untouched.

## 2. Block layout

```
+--------------------------------------------------------------+
| header    : identity, commit futex, cursors, section table   |
|            : and discovery metadata                          |
+--------------------------------------------------------------+
| catalog   : fixed-capacity 32-byte entry array + freelist    |
+--------------------------------------------------------------+
| journal   : fixed ring metadata + fixed 64-byte entry slots   |
+--------------------------------------------------------------+
| requests  : fixed 64-slot request queue with full names       |
+--------------------------------------------------------------+
| region    : fixed descriptor table outside the growable pool   |
| desc      :                                                  |
+--------------------------------------------------------------+
| pool      : growable arena, then region payload tail          |
+--------------------------------------------------------------+
```

The block is one contiguous mapping. It grows via mremap (see section 9);
the header and fixed sections stay at stable offsets, while the pool's
`arena_end` and `region_head` carve the growable tail. The exact sizes and
offsets are in `configstorelayout.md` §2.

```

Section table entries identify each region by stable id, offset, and size. The WM owns the section table; readers treat it as data, never as a promise.

## 3. The catalog (open key-value store)

The catalog is the single structure that replaced the earlier idea of separate fixed "option slabs". Everything is a catalog entry: core options, bindings, rules, runtime state objects, framebuffer regions, extension keys.

Entry header layout (per entry):

```
entry_id         : implicit catalog slot index
entry_generation : u64   non-zero allocation lifetime
name_ref         : u32   arena frame offset for an immutable UTF-8 name
type_tag         : u16   one of the type tags in section 4
flags            : u16   DESTROYED, FREE, EPHEMERAL, WINDOW_DEPENDENT (bit 0 reserved)
length           : u32   inline encoded length or framed payload length
body_ref         : u32   arena frame offset, region descriptor index, or 0
value_inline     : u64   low bytes of an inline value or reference generation
```

- Any key name is accepted. No whitelist, no reject-at-write. Unknown keys are stored and served verbatim.
- Namespace by convention: `wm.*` is reserved for the core; extensions use their own prefixes (`ext.<name>.*`). This is convention, not enforcement: the block cannot identify writers, so there is no real access control. Scope is expressed by exclusion: an unmarked entry is ordinary configuration, and a facade marks a value out of `save` by setting `WINDOW_DEPENDENT` (scoped to one window) or `EPHEMERAL` (no long-term relevance, such as process bookkeeping). `save` and a soft reset use the same test, `WINDOW_DEPENDENT clear AND EPHEMERAL clear`.
- The WM seeds its core keys at startup as ordinary entries, exactly the way an extension creates theirs at runtime.
- Entries are created live. The fixed-capacity array has a freelist; live entries never move, so holders can keep references stable.
- A catalog reference is `(epoch, entry_id, entry_generation)`. `entry_id` is the slot index and `entry_generation` changes whenever that slot is reused.
- Names are immutable during one entry lifetime. A rename deletes and recreates the entry so old journal references cannot acquire a new name.
- Structural rules for the WM read path: verify the representation-specific reference and length, type tag, commit id, entry generation, and alignment before dereferencing. See section 12.

## 4. Value type tags

The tag space is a u16. Values are stored in native endianness and alignment, tied to format_version. Tags 0x8000 and up are reserved for extension-specific types. The list is intentionally broad for now; redundant options can be culled later.

| Tag   | Type        | Size/notes                                  |
|-------|-------------|---------------------------------------------|
| 0x01  | bool        | 1 byte (0/1)                                |
| 0x02  | i8          | 1 byte                                      |
| 0x03  | i16         | 2 bytes                                     |
| 0x04  | i32         | 4 bytes                                     |
| 0x05  | i64         | 8 bytes                                     |
| 0x06  | u8          | 1 byte                                      |
| 0x07  | u16         | 2 bytes                                     |
| 0x08  | u32         | 4 bytes                                     |
| 0x09  | u64         | 8 bytes                                     |
| 0x0A  | f32         | 4 bytes                                     |
| 0x0B  | f64         | 8 bytes                                     |
| 0x0C  | char        | 4 bytes (UTF-32 code point)                 |
| 0x0D  | string      | length-prefixed, UTF-8                      |
| 0x0E  | blob        | length-prefixed raw bytes                   |
| 0x0F  | duration    | u64 nanoseconds                             |
| 0x10  | ratio       | f64                                         |
| 0x11  | percent     | f32 in [0,1]                                |
| 0x12  | keysym      | u32 XKB keysym                              |
| 0x13  | keycode     | u32                                         |
| 0x14  | modmask     | u32 bitmask (ctrl/alt/shift/super/etc)      |
| 0x15  | point       | {i32 x, i32 y}                              |
| 0x16  | size       | {u32 w, u32 h}                              |
| 0x17  | rect        | {i32 x, i32 y, u32 w, u32 h}                |
| 0x18  | offset      | {i32 x, i32 y}                              |
| 0x19  | vec2        | {f32 x, f32 y}                              |
| 0x1A  | vec3        | {f32 x, f32 y, f32 z}                       |
| 0x1B  | vec4        | {f32 x, f32 y, f32 z, f32 w}                |
| 0x1C  | vec2i       | {i32 x, i32 y}                              |
| 0x1D  | vec2u       | {u32 x, u32 y}                              |
| 0x1E  | rgba8       | 4 bytes                                     |
| 0x1F  | argb8       | 4 bytes                                     |
| 0x20  | rgba32f     | 16 bytes (4 x f32)                          |
| 0x21  | hsva8       | 4 bytes                                     |
| 0x22  | region_ref  | region descriptor id plus `region_generation` |
| 0x23  | entry_ref   | `(entry_id, entry_generation)` catalog handle |
| 0x24  | enum        | string name of an enum constant             |
| 0x25  | option      | nullable value (no value flag + payload)    |
| 0x26  | array       | bounded: element type tag + count + body    |
| 0x27  | tuple       | bounded: fixed field tag sequence + body    |
| 0x28  | font_desc   | string (pango-style font description)       |
| 0x29  | shader_ref  | u32 handle to a shader source entry         |
| 0x2A  | cursor_ref  | u32 handle to a cursor/theme entry          |
| 0x2B  | binding     | framed: header + positional argument array   |
| 0x2C  | rule        | generic match-replace rule record           |
| 0x2D  | gradient     | array of {offset, rgba8/rgba32f} stops      |
| 0x2E  | path        | string (filesystem path)                    |
| 0x2F  | exec        | string (shell command line)                 |
| 0x30  | state       | string (free-form state token)              |
| 0x31  | datetime    | i64 nanoseconds since the Unix epoch, UTC   |
| 0x32  | constraint  | generic layout program, a packed array of 16 B records |
| 0x33  | client_rule | 24 B header plus the value its effect applies |
| 0x34..0x7FFF | reserved | the unassigned low hole, see below       |
| 0x8000.. | extension | extension range, structurally valid framed payloads |

For v1 representation, fixed-size scalars and fixed-size composite types whose
encoded size is at most 8 bytes use `value_inline`; `body_ref` is zero. All
strings, blobs, arrays, tuples, options with a payload, bindings, rules,
gradients, paths, commands, and state tokens use an arena frame. An
`option(None)` is encoded as `length == 0`, `body_ref == 0`. A `region_ref`
stores the descriptor index in `body_ref` and its generation in
`value_inline`. An `entry_ref` uses a framed payload containing the u32
`entry_id`, a reserved u32, and the u64 `entry_generation`. A `datetime` is a
fixed 8-byte value and uses `value_inline` like any other 8-byte scalar; the
offset from local time is normalised to UTC on the way in, so two datetimes
naming the same instant from different offsets compare equal. Unknown extension
tags use a framed payload and are not semantically interpreted by the core.
The exact frame header and bounds are in `configstorelayout.md` §5.

Three payloads are shaped rather than scalar, so their fields are named here and
numbered in `omni_layout.h`. Each is a framed value; the numbers are not repeated
in this table on purpose, because `filestructure.md` "The rule" makes
`include/shared` the ABI surface and a number stated in two places is a number
that can disagree with itself.

`binding` is a 24-byte header followed inline by the argument array:

```
off  size  field
0    4     modmask      u32
4    4     keysym       u32
8    4     keycode      u32
12   4     action_ref   u32 arena frame offset, the action name
16   4     args_ref     u32 arena frame offset, or OMNI_REF_NONE
20   4     flags        u32, bit 0 USE_KEYSYM
24   ...   args         array value of strings, inline
```

The header is fixed and the array is variable, which is why this is a framing
change and not a new tag: a binding without arguments is still a binding, and
`args_ref == OMNI_REF_NONE` says so without a length comparison. The argument
array is positional and every element is a string, matching what `exec` already
accepts, so `wm.cycle_layout alpha` and a TOML
`args = ["alpha"]` are the same value in two syntaxes. Argument count and types
are the action's business, not the store's.

`constraint` is a packed array of 16-byte records with no header, so the record
count is derived from the value length and order is the evaluation order:

```
off  size  field
0    2     kind         u16
2    2     flags        u16
4    2     priority     u16, linear weight
6    1     edges        u8, edge mask
7    1     axis         u8
8    4     role         u32, walk index or ROLE_PARENT / ROLE_NONE
12   4     literal      i32
```

There is no subject and no entry identity in a record. The program is the named
layout, stored once and shared by every client it arranges, so a record
constrains the window the solver is currently at and the walk supplies which
window that is. `role` is the operand as a position in the walked set rather than
as a window, which is what lets one program say "left-of the master" without
knowing which window is the master.

`priority` is a weight, not a rank, and the four bands
(`DOMINANT` 60000, `STRONG` 6000, `MEDIUM` 600, `WEAK` 60) are an order of
magnitude apart so one band dominates the compromise rather than competing with
it. `0` means no opinion, which is what an absent record means, so the lowest
useful value is `1`. The `edges` bit positions are the same four river's
`set_tiled` and Hyprland's `Layout::eRectCorner` use, so an adjacency computed
here is the value those protocols already carry. Subject and operand are
`entry_id` plus `entry_generation`; the epoch is implied, and a program naming
another epoch is malformed rather than a dangling reference. A `length` that is
not a multiple of 32 is malformed.

`client_rule` is a 24-byte header followed by the value its effect applies:

```
off  size  field
0    4     match_appid  u32 arena frame offset, or OMNI_REF_NONE
4    4     match_title  u32 arena frame offset, or OMNI_REF_NONE
8    2     effect       u16
10   2     flags        u16, bit 0 TITLE_REGEX
12   4     target_id    u32
16   4     target_gen   u32
20   2     scope        u16
22   2     reserved     zero
24   ...   payload      the value the effect applies
```

`client_rule` is not `rule` with a flag set. `rule` matches a key and replaces a
value; every rule the layout set needs matches a client and changes which scopes
contain it, so it is a different record rather than a variant of one. `scope`
separates the two timings that are otherwise indistinguishable: `MAP_ONLY` is
evaluated when the client maps, `ALWAYS` is re-resolved on reload, which is what
makes a rule survive a restart instead of applying once and being lost.

## 5. Journal / event stream (single unified ring)

There is one ring, not two. It carries:

- `KEY_SET` entries: `{epoch, commit_id, journal_seq, time_ns, entry_id, entry_generation, type, value}`. Values of at most 8 encoded bytes use `value_inline`; framed values use an arena reference. This is the generic "key X now has value Y" channel.
- `KEY_DELETE` entries with the same epoch, commit, journal, and stable catalog identity fields.
- Free-form event entries (`EVENT`): `{epoch, commit_id, journal_seq, time_ns, category, value/event_ref}`, covering region creation and destruction, request completion, custom extension events, and coarse texture-state announcements. Core lifecycle events use a framed payload containing the complete catalog and descriptor identities; request completion carries `(epoch, ticket)`.
- Every visible commit appends at least one entry, including growth, readiness changes, and request completion. A grouped commit shares one `commit_id` across all of its entries and ends with a `COMMIT_END` entry.
- Capacity is a compile-time constant at v1, enforced as a ring with drop-oldest. Older entries are overwritten before newer ones are ever dropped, and the ring metadata records the oldest retained `journal_seq` for gap detection.
- Journal ring metadata is published with an even/odd `publish_seq` seqlock. Writers make the sequence odd before updating metadata and release-store it even after all slots are written; readers retry on an odd or changed sequence.
- `watch <key>` reads the ring forward. `get <key>` reads the current catalog value instead.
- Pixel churn is never journaled. Framebuffer regions use `region_generation` plus `region_revision` (section 7), never a journal sequence.
- A journal cursor is `(epoch, journal_seq)` and is a last-seen marker: replay delivers every retained entry with `journal_seq` greater than the cursor.

### 5.1 Gap detection

The ring has no backpressure and the block registers no consumers, so a writer
never learns that a consumer is behind. Drop-oldest therefore makes loss a
normal condition that the consumer detects at read time, not a store error.

Under the even `publish_seq`, a consumer reads the boundary
`(oldest_journal_seq, next_journal_seq)`. With last-seen cursor `C` in the same
epoch:

```
C < oldest_journal_seq - 1   gap; the lost range is [C+1, oldest_journal_seq-1]
C == oldest_journal_seq - 1  exactly contiguous; always replayable
C >= oldest_journal_seq      ordinary forward read
```

A commit's entries and its `COMMIT_END` are separate slots, so a consumer that
finds entries whose `commit_id` has no `COMMIT_END` in the ring must treat the
group as incomplete and must not apply any of it. Group visibility is
all-or-nothing in both gap policies below.

### 5.2 Gap policy is per subscription

Gap handling is chosen by the consumer, not by the block. The store holds no
policy field, so this is expressed wherever a subscription is created: the
library call for a direct mapper, the request field for the socket facade. The
block ABI is unchanged by this choice.

- `gap_policy = "fail"` (default): the subscription terminates with `WATCH_GAP`
  and the loss range. The client re-attaches with a cursor and receives a fresh
  snapshot. It learns exactly which sequences it missed.
- `gap_policy = "resync"`: the subscription emits a `WATCH_RESYNC` data
  notification and continues. This is a per-consumer condition, so it is never
  appended to the ring; the ring stays the authoritative event stream.

The default is `"fail"` because silent loss is the dangerous default and
failing is trivially recoverable by the caller. `"resync"` is opt-in for
clients that prefer approximate liveness over managing re-attachment.

Resync is the live-attachment procedure, not a cursor jump:

```
acquire commit_state; require IDLE
read commit_id and the ring boundary
read commit_id again; retry on mismatch or non-IDLE
snapshot the catalog for the subscription's filter
resume streaming from the boundary, exclusive
```

Resuming at `oldest_journal_seq` instead would replay a group whose
`COMMIT_END` was evicted and re-apply entries the snapshot already contains.
Because the snapshot already reflects everything up to the boundary, a client
holding derived state across a resync would double-apply; the client must
discard stream-derived state and reload from the snapshot.

The same detection and the same two policies apply to a gap present at attach
time, when a client re-attaches with a `since` cursor already below
`oldest_journal_seq`.

### 5.3 Epoch change is a separate, always-terminal condition

A changed `epoch` is not a ring loss. The old block was unlinked and recreated,
so the old cursor's data is permanently gone and no cursor advancement recovers
it. The subscription terminates with `WATCH_EPOCH_CHANGED` and the caller
re-discovers the instance. `"resync"` covers loss within one epoch only.
Keeping this distinct from `WATCH_GAP` is what lets a client distinguish "I
fell behind" from "the compositor I was watching is gone".

### 5.4 Recycled identities in replay

A replayed `KEY_SET` or `KEY_DELETE` is applied only if its
`entry_generation` still matches the live catalog entry. A mismatch means the
slot was destroyed and reused, so the entry describes an object that no longer
exists; the consumer refuses it and continues rather than writing a stale
value into a live entry. A matched generation is what makes a journal
reference stable across deletion and reuse.

## 6. Commit & guard rules

Commit protocol (identical for WM core and external writers):

1. Take `header.futex` with an acquire CAS. Refuse the block if `state` or
   `commit_state` is `BROKEN`.
2. Publish `writer_pid` and a fresh per-acquisition `writer_token`; only the
   matching token may release the futex.
3. Perform side-effect-free validation and reserve required capacity. If pool
   growth is required, publish `commit_state = GROWING` while the futex is
   held. A failed growth attempt leaves state unchanged and returns a normal
   request error; a partially applied filesystem change marks the block
   `BROKEN`.
4. Set `active_commit_id` to the next `commit_id` and release-store
   `commit_state = ACTIVE`.
5. Mutate entry payloads, catalog metadata, arena cursors, region descriptors,
   request slots, and section metadata. Once this step begins, any failure or
   writer death marks the block `BROKEN`; there is no partial rollback.
6. Append one or more journal entries. Every entry records the current `epoch`
   and `commit_id`; grouped entries have distinct `journal_seq` values. Append
   a `COMMIT_END` entry whose `event_ref` counts the preceding entries in the
   commit.
7. Publish the journal ring metadata with an even `publish_seq` after every
   slot in the commit is complete.
8. Release-store the new `commit_id`, then release-store
   `commit_state = IDLE`.
9. Clear `writer_pid` and `writer_token`, then release `header.futex`.

Readers acquire `commit_state` and refuse `ACTIVE`, `GROWING`, or `BROKEN`.
They then acquire `commit_id`, take their snapshot, and acquire `commit_id` and
`commit_state` again. They retry on any mismatch. The release/acquire pairs
make all payload, catalog, descriptor, and journal writes visible before the
new `commit_id` and `IDLE` state.

Every visible commit appends at least one journal entry and one `COMMIT_END`
entry, so no state change is invisible to the event stream and grouped changes
have an explicit boundary. If a consumer loses the ring's oldest entries, the
oldest retained `journal_seq` exposes the gap and it resynchronizes from a
commit-id-stable snapshot.

Recovery is fail-stop. A recovery authority may reclaim a held futex only
after proving the recorded writer process is dead with a pidfd or equivalent
start-time check; PID liveness alone is insufficient. It marks the block
`BROKEN`, refuses further reads and writes, and recreates the block with a new
`epoch`. It never attempts to interpret partially mutated data.

Writer-side checks are structural convenience only (e.g., request-time sanity
on declared sizes), never enforcement: the requester is not trusted and the WM
re-validates structure on every read. Semantic checks never happen at write
time.

## 7. Framebuffer regions

A region is a catalog entry of type `region_ref`; its descriptor lives in a fixed 64-entry table outside the growable pool, and its pixels live in the pool's region-payload window. Descriptors record format, stride, `slot_count`, `region_generation`, and `region_revision`.

- `region_generation` identifies the descriptor allocation and changes whenever a descriptor is reused. `region_revision` is a publication counter local to that generation. A `region_ref` value carries the descriptor index and that generation; it has no arena frame.
- Descriptor allocation uses a linear scan. A free descriptor with either active bit set is not reusable. Allocation increments the retained generation, clears destruction and active bits, resets revision to `0`, and then publishes the descriptor.
- Painter side: if the current revision is `r`, write slot `(r + 1) & 1` when `slot_count == 2`, otherwise slot 0, then release-store `r + 1` as `region_revision`. The reader selects slot `observed_revision & 1` when `slot_count == 2`, otherwise slot 0. A generation's first published revision is `1`; revisions do not wrap within a generation.
- WM reader side, once per frame: read and validate the descriptor and a non-zero `(region_generation, region_revision)` pair, set `CONSUMER_ACTIVE`, copy the selected slot, and read the pair again. If the pair or liveness changed, discard the sample and retry or skip the frame. Only a stable sample is uploaded via `glTexSubImage2D` and cached. A new descriptor generation always invalidates the cache.
- Producer and consumer active bits surround their respective pixel work. A destroy or descriptor reuse must not reclaim or reuse a payload while either bit is set; a stale bit causes a safe leak until block recreation.
- One painter per region is the v1 convention. The WM validates the descriptor, checked pixel-size arithmetic, and pool window before reading pixels and does not use a journal sequence for region publication.

## 8. Region/key lifecycle and the request queue

Requests are how socket-free programs ask the WM to create or destroy catalog entries and regions.

- 256 fixed request slots, sized for the queue's real load rather than the region count. The queue carries CREATE_ENTRY and CREATE_REGION alike while the catalog holds 16,384 entries, and nothing in the protocol bounds a client's in-flight requests, so a batching client that submits a 200-key theme is legal and would exhaust a 64-slot queue. Each slot contains `{epoch, status: FREE/PENDING/DONE/ERROR, ticket, requester_pid, requester_start_id, type, target, target_generation, params, response, terminal_at_ms}`. The name field is 4096 bytes and accepts names up to 4095 bytes including the terminating NUL; initial values are limited to 128 bytes.
- Submission takes the commit futex, finds a FREE slot, writes the complete slot (`PENDING`, a new `ticket` within the current epoch, `requester_pid`, `requester_start_id`), and releases. A request is refused before any slot is taken when the block is not READY or is BROKEN, so a refused request never consumes a ticket. With no free slot the request fails immediately rather than waiting, so a stuck requester cannot stall a live one. `(epoch, ticket)` is the complete request identity.
- The status is a state machine, not a set of labels. `FREE -> PENDING` on submission; `PENDING -> DONE` when the WM applied the request in full; `PENDING -> ERROR` when it refused, with no partial mutation existing; `DONE|ERROR -> FREE` on acknowledgement or reclaim. Only the requester returns a terminal slot to FREE voluntarily, and the WM does so only through reclaim, so the acknowledgement path and the reclaim path can never both free one slot.
- WM drain, once per tick: validate params and target generation structurally, apply, then publish `DONE` or `ERROR` together with the new identity and a `terminal_at_ms` stamp in a single commit. The `REQUEST_DONE` event is appended by a later commit, never the same one, so a client that trusts the event never reads a slot still marked PENDING. A region create rejects dimensions that do not fit the u16 descriptor fields or that exceed the frame maximum under subtraction-first arithmetic, normalizes `slot_count == 0` to `1`, and treats a count above the maximum as `PARAM_INVALID` rather than clamping it. A rejected request allocates no descriptor and consumes no pool space.
- The requester copies the whole slot out and only then releases it, because the result fields live in the slot. It must also confirm `(epoch, ticket)` still matches its own request before trusting anything it read: the deadline below can free a slot under a live but slow requester, and a mismatch is reported as `REQUEST_EXPIRED` rather than read as another request's answer.
- Reclaim frees a slot whose status is PENDING, DONE, or ERROR when either the owner is dead or the terminal deadline has passed. Dead means `(requester_pid, requester_start_id)` proven gone by a start-time-verified check; a bare `kill(pid, 0)` is insufficient because a pid can be reused. Expired means the status is terminal and `now - terminal_at_ms` exceeds `OMNI_REQUEST_RECLAIM_MS`. The deadline is a single compile-time constant that only the WM reads, and it never applies to PENDING, so a busy WM cannot expire work it has not drained. A re-armed slot is PENDING with `terminal_at_ms` zero and can never be matched by the deadline branch. A crashed producer therefore leaks nothing indefinitely, and an inexperienced client cannot lock the WM out of its own request queue.
- DESTROY requires an exact `(epoch, entry_id, entry_generation)` and matching `region_generation` for a region. It marks the catalog entry and descriptor destroyed, appends a lifecycle event carrying both identities, and never exposes a partial result. A trailing region payload is reclaimed only after both active bits are clear; other payloads remain reserved until block recreation. A reused descriptor receives a new generation. Applied copies inside the WM survive regardless.

## 9. Growth and stale-mapping detection

- Growing the block uses mremap. The futex holder publishes `commit_state = GROWING` before changing the mapping, so readers do not interpret a partially resized block. After growth the WM updates `block_size`, writes any new section table entries, appends a store-growth event and `COMMIT_END` in the same commit, then publishes the new `commit_id` and returns the state to `IDLE`. The header is always in the first page.
- A failed preflight growth attempt leaves the block unchanged and returns the request error. If the filesystem or mapping was partially changed, the writer marks the block `BROKEN` and does not attempt rollback.
- Mappers: on any `commit_id` change, re-read the header; if `block_size` grew, re-map to the new size and re-read the section table and catalog. The client library does this transparently. A mapper that observes `BROKEN` stops and waits for a new `epoch`.
- The header carries `pid`, `boot_time`, `writer_pid`, `writer_token`, and `commit_state`, so a mapper can detect a dead or restarted compositor rather than trusting a stale file. On startup the WM unlinks and recreates the block, ignoring any orphaned mapping.

## 10. Discovery / interop

Decided defaults:

- The compositor sets the environment variable `OMNI_INSTANCE_SIGNATURE` to the absolute path of the SHM file.
- The optional socket facade lives beside it at `<block-path>.sock` inside the same runtime directory.
- Fallback discovery: scan `$XDG_RUNTIME_DIR` for entries matching `omniwm-*.shm` / `omniwm-*.shm.sock`.
- Permissions 0600, same-user only. No authentication inside the block; defense is file permissions plus consumption-side validation.

## 11. Socket facade (optional convenience)

The socket is a thin JSON facade over the exact same primitives, for people who do not want to map the block. A well-written library never needs it. Protocol spec: devnotes/ipc.md.

- `get <key>`: read current value from the catalog.
- `set <key> <typed value>`: commit protocol, single or grouped.
- `watch <key|glob>`: read the journal forward, with a per-subscription `gap_policy` of `fail` or `resync` (§5.2).
- `exec <action> [args]`: invoke a registered action by name (bindings reference actions by string, resolved via the registry at dispatch time).
- `save [path]`, `reload`, `delete <key>`, `reset` with `mode: "soft"|"hard"`.

No key-specific handling anywhere: unknown keys are served verbatim, matching the open-catalog rule. The socket implementation is a client of the block, not a peer of it.

The built-in TOML parser is a facade in the same sense, running in-process against
the block. Its type binding is in `tomlparser.md`.

## 12. Guard rules, precisely

The structural guard is four ordered tiers. Each tier has exactly one failure
action, and the tiers are evaluated in order, so no check is ever stated
alongside an unrelated one. This replaces the single flat bundle that could not
be evaluated as written.

```
L1  header    failure: refuse the whole block, no per-entry logging
L2  section   failure: skip that section, continue with the others
L3  slot      failure: skip that slot, continue with the others
L4  value     failure: skip that entry, continue with the others
```

- **L1 header.** `magic`, `format_version`, `header_size`, section constants,
  `state` in {CREATING, READY}, and `commit_state` readable. A CREATING block
  is `NOT_READY`; a BROKEN block is `STORE_BROKEN`. Both are refusals, not
  corruption, and neither is logged per entry because there is no consumer
  state to log into yet.
- **L2 section.** Each section row has `id == row`, a 16-aligned `offset`, a
  `size` that does not wrap, and a range contained in `[0, block_size)`. A
  section outside the mapping is skipped, never clamped to it.
- **L3 slot.** Slot index inside the section's declared capacity, the slot's
  identity fields non-zero where required, generation matching for a live
  reference, and 16-byte alignment before any dereference.
- **L4 value.** The tag decision, flag state, and length rules below.

Two further tiers are applied at their own sites:

- Replay (mandatory, on every journal entry): apply only entries whose
  `commit_id` has its `COMMIT_END` in the ring, only entries whose
  `entry_generation` matches the live catalog entry, and only after the §5.1 gap
  check. Failure is skip-and-continue or a policy-driven resync, never a partial
  application.
- Semantic (the consumer of the value decides): numeric ranges, enum name
  recognition, action-name registration, sanity relative to the key's meaning.
  Invalid semantic data is refused and skipped, never fatal.

### 12.1 Tag decision

The tag test is a three-way branch, not "is this tag known". This is what lets
the core carry extension values it cannot interpret while still refusing its own
malformed values.

```
0x01..0x33        known:      require the tag's exact encoded length and its
                              representation rule (inline or framed)
0x8000..0xFFFF     extension:  require VALUE_FRAMED, a verified frame, and a
                              length the core never interprets
0x00, 0x34..0x7FFF unassigned: malformed, refused by the core
```

`0x00` is unassigned in the catalog and is used only by journal entries that
carry no value. An unassigned low tag is a hole in the core's own table, not an
extension, so it is refused. Save preserves entries on the extension branch and
skips entries that fail any structural tier.

The upper end of the known range is a moving target: `0x32` and `0x33` were added
for the layout engine, and the hole simply moved from `0x32..0x7FFF` to
`0x34..0x7FFF`. `OMNI_TAG_MAX_KNOWN` and `OMNI_TAG_UNASSIGNED_LO` are asserted to
be adjacent in `omni_layout.h` so the two cannot drift apart, which is the only
thing that makes a hole detectable at all.

### 12.2 Flag state

Lifecycle bits and scope bits are checked separately. Conflating them is what
made the original predicate unreadable.

Lifecycle (`FREE`, `DESTROYED`):

```
FREE set        length, body_ref, and value_inline MUST be zero;
                no value tier runs at all
FREE clear      DESTROYED may be set; the entry is not live and MUST NOT be
                returned by get, save, or any live reference
both clear      the only normal live state
```

A FREE or DESTROYED slot is a normal steady state, not corruption, so it is
skipped without a per-read diagnostic.

Scope (`EPHEMERAL`, `WINDOW_DEPENDENT`) applies only to a live entry:

```
both unset              ordinary configuration; saved, and cleared by a soft reset
WINDOW_DEPENDENT set    window-scoped; never saved, never cleared by a reset
EPHEMERAL set           no long-term relevance; never saved, never cleared
both set                never saved; WINDOW_DEPENDENT governs reset behaviour
```

`WINDOW_DEPENDENT` dominates, so a per-window override of a configuration
option survives a soft reset. That is what makes "set generally and for one
window" work without a second mechanism.

### 12.3 Length and bounds

A value length is never compared against an offset range. The two cases are
distinct and fail differently.

```
inline   length <= 8 and length == the tag's exact encoded size
framed   length == frame.used and the complete frame lies inside both the
         declared arena window and block_size
region   length == 8, body_ref < OMNI_REGION_DESC_COUNT, generation non-zero
```

An over-length inline value is malformed. A framed value whose declared length
disagrees with its frame header is malformed. Neither is repaired.

### 12.4 Overflow-safe arithmetic

Every `offset + length <= limit` in the codebase is written as:

```
offset <= limit  and  length <= limit - offset
```

Subtract first, then compare. `offset + length < offset` is never used as the
wrap test, and no guard computes an end address and compares it to a start.
This is the mechanical substitution that makes all bounds checks provably
non-wrapping and directly assertable by the fuzz target.

### 12.5 Truncated and unmapped blocks

`block_size` smaller than a fixed section means that section is unusable, not
partially usable. A reader uses only the sections fully contained in the
mapping and treats the rest as absent. A section whose declared range extends
past `block_size` is skipped. Truncating a live block therefore degrades the
reader to fewer sections; it never produces a header claiming more than the
mapping holds.

### 12.6 Diagnostic policy

One policy, one counter. A per-consumer dedupe keyed by
`(tier, section, slot index)` logs a given corrupt slot once instead of once
per frame, while a different corrupt slot is still reported. The key is reset
when the slot's `entry_generation` changes, so a slot that becomes valid and
corrupt again is reported again. Diagnostics never change control flow: a
logged entry and a silently skipped entry are the same outcome.

Every read treats the whole block as hostile input, because every mapping process can write every byte of it; page-level write protection cannot work inside one RW mapping.

Free win: because bad data is a designed-for state, hardening is fuzzable. Garbage the block (mutate random bytes, truncate, clobber lengths/refs), run the WM, assert it never crashes and only skips-and-continues. This is a first-class test target. Because each invariant in `configstorelayout.md` §11 names exactly one tier, the corpus is organised per tier: a mutation that should trip L3 is asserted to skip one slot and leave every other slot readable, and a mutation that should trip L1 is asserted to refuse the block rather than degrade any single entry.

## 13. Startup, config-as-saved-state, and save-file semantics

- Startup: create block, seed core `wm.*` keys, set `OMNI_INSTANCE_SIGNATURE`, serve.
- A config file is a sequence of `set`/`exec` replayed into the block.
- `OMNI_CONFIG_MAX_OPS` is 4096, the number of operations one config may contain,
  and it is a global limit rather than a per-command one. It equals the journal
  ring capacity, because a whole-file reload is published as one grouped commit
  and a group cannot exceed the ring. A config that exceeds it is rejected whole,
  with the offending count in the response, and never silently truncated.
  Reaching this limit is a signal rather than an obstacle: a configuration
  complex enough to want 4096 operations is a configuration better driven by a
  script or library, which can apply it in as many commits as it likes and owns
  the observability consequences of doing so.
- Partial-invalid config: the whole file still loads. Line-level failures are logged and that line is not applied, but never abort the file. Whatever reached the block stays; consumers refuse invalid keys when they parse them. The config fails per line, never as a whole. A line-level failure is distinct from exceeding `OMNI_CONFIG_MAX_OPS`, which aborts the file rather than skipping the excess.
- Save: every live entry with `WINDOW_DEPENDENT` clear and `EPHEMERAL` clear is written out, so a config file replayed from the server's own `save` reproduces configuration rather than geometry or bookkeeping. Window-dependent state (per-client geometry, focus, per-window overrides) is excluded, as is anything a facade marked `EPHEMERAL`.
- `save` takes an optional key-path pattern that narrows the scope, and with no argument it writes the whole block under that same test, so the argument is purely additive. The pattern is a dotted prefix with a single trailing `*` meaning the subtree below it, which is what makes a layout slot extractable: a layout authored over IPC at `omniwm.layouts.delta` is written by `save omniwm.layouts.delta.*` and nothing else is in the file. The exclusion test is identical inside a narrowed scope, so narrowing cannot leak window-dependent or ephemeral state, and it is the same test the soft reset uses, so `save`, a narrowed `save` and a soft reset can never disagree about which entries are configuration. A general glob is deliberately not supported; one trailing wildcard is enough to name a namespace, and a full pattern language would make the interaction between a pattern and the exclusion test harder to reason about than the feature is worth.
- A soft reset uses the identical test as `save` (`WINDOW_DEPENDENT` clear and `EPHEMERAL` clear), so the two operations can never disagree about which entries are configuration.
- Export policy for bad data: entries that fail any structural tier (L1-L4) are skipped with a warning; entries on the extension branch of the tag decision that are structurally valid are preserved as-is so their owning extensions can re-import them on the next boot. An unassigned low tag is a structural failure, not an extension, and is never exported.

## 14. Deferred, flagged for later

Resolved during the field-level layout pass (see `configstorelayout.md`):

- Fixed: exact field-level struct definitions, byte offsets, alignment, section
  sizes, and capacities.
- Fixed: inline-versus-framed representation rules, extension validation,
  persistence flags, and v1 reclamation policy.
- Fixed: journal ring capacity is a constant at v1 (`OMNI_JOURNAL_CAPACITY`),
  defined once in `include/shared/omni_layout.h` per decision 6.

Still deferred:

- Multi-painter arbitration on a single region (out of scope: one painter per region).
- Value transactions beyond grouped commits (no CAS at v1).
- Mixed-endianness hosts (native, tied to format_version).
- Extension-specific semantic validation beyond structural framing checks.
- Compile-time ABI tests asserting the header's values against this document.

