# Config store: field-level layout (drafting pass 3)

This is the executable refinement of `configstorage.md`. It fixes exact struct
layouts, byte offsets, alignment, capacities and v1 constants. Every value in
this document is a v1 default; nothing here changes the concepts in
`configstorage.md`, it only pins them down.

All offsets are relative to the start of the SHM mapping. Native endianness.
Alignment: 16 bytes for every multi-byte field and every arena allocation
(`configstorage.md` §14 deems mixed-endianness out of scope).

## 1. Conventions

- The block is treated as hostile input on every read. All numbers below are
  bounds that a read path verifies before dereferencing.
- Writers serialize on the single commit futex in the header. Readers never
  lock; they rely on the generation counter. Aligned 8-byte loads/stores of
  `generation` and of the journal ring word use atomic builtins; this is
  market on x86/aarch64 for aligned access and is a required property on any
  platform.
- An "arena value frame" is the storage unit of every named and payload value
  in the pool (see §5). Region payloads are raw pixels, unframed.
- Every structural invariant has a numbered entry in §11; the future fuzz
  target asserts that list (`configstorage.md` §12).

## 2. Constants (single definition point)

All numbers live in one header, `include/shared/omni_layout.h` (not yet
created; the doc is the source of truth until it exists). Changing a value
here must be the only edit needed; no constant is ever duplicated in code.

| constant | value | meaning |
|---|---|---|
| `OMNI_MAGIC` | `0x4F574D42` | "OWMB" in memory-writing order |
| `OMNI_FORMAT_VERSION` | `0` | bumped on any layout change |
| `OMNI_ALIGN` | `16` | global alignment |
| `OMNI_BLOCK_INITIAL_SIZE` | `2 MiB` | created block size (page multiple) |
| `OMNI_BLOCK_MAX_SIZE` | `512 MiB` | hard cap on the mapping |
| `OMNI_HEADER_SIZE` | `256 B` | header region (fields end at 192) |
| `OMNI_SECTION_SLOT_COUNT` | `16` | fixed section table rows |
| `OMNI_CATALOG_OFF` | `0x1000` | catalog section base |
| `OMNI_CATALOG_SLOT_COUNT` | `16384` | fixed catalog capacity |
| `OMNI_CATALOG_ENTRY_SIZE` | `16 B` | catalog entry header size |
| `OMNI_ENTRY_NAME_MAX` | `4095 B` | max framed string length for names |
| `OMNI_JOURNAL_OFF` | `0x41000` | journal section base |
| `OMNI_JOURNAL_CAPACITY` | `4096` | ring slots, constant at v1 |
| `OMNI_JOURNAL_SLOT_SIZE` | `40 B` | fixed-size ring slot |
| `OMNI_JOURNAL_INLINE_LIMIT` | `8 B` | values <= 8 bytes go inline |
| `OMNI_REQUESTS_OFF` | `0x6A000` | request section base |
| `OMNI_REQUEST_SLOT_COUNT` | `64` | fixed request slots |
| `OMNI_REQUEST_SLOT_SIZE` | `256 B` | fixed request slot size |
| `OMNI_REQUEST_NAME_MAX` | `63 B` | inline request name cap |
| `OMNI_REQUEST_VALUE_MAX` | `128 B` | inline request initial-value cap |
| `OMNI_POOL_OFF` | `0x6E000` | storage pool base |
| `OMNI_POOL_GROWTH` | `2` | doubling factor on pool growth |
| `OMNI_REGION_SLOT_COUNT` | `64` | fixed region descriptors |
| `OMNI_REGION_DESC_SIZE` | `32 B` | region descriptor size |
| `OMNI_REGION_SLOT_COUNT_MAX` | `2` | double buffering is 1 or 2 |
| `OMNI_REGION_SLOT_MAX_BYTES` | `34 MiB` | one frame max (a 4K RGBA8 is 33.2 MiB) |

Derived values for v1 (formulas, not constants):

```
JOURNAL_SIZE     = 8 + OMNI_JOURNAL_CAPACITY * 40          = 167,936 B (0x29000)
REQUESTS_SIZE    = OMNI_REQUEST_SLOT_COUNT * 256           =  16,384 B (0x4000)
CATALOG_SIZE     = OMNI_CATALOG_SLOT_COUNT * 16            = 262,144 B (0x40000)
INITIAL_POOL     = OMNI_BLOCK_INITIAL_SIZE - OMNI_POOL_OFF = 1,646,592 B
MAX_POOL         = OMNI_BLOCK_MAX_SIZE   - OMNI_POOL_OFF
```

Layout overview (one contiguous mapping):

```
0x000000  header (256 B) + section table at 0x100 (16 x 32 B)
0x001000  catalog    : 16,384 x 16 B  entry headers  (fixed)
0x041000  journal    : ring word + 4,096 x 40 B slots (fixed)
0x06A000  requests   : 64 x 256 B slots               (fixed)
0x06E000  pool       : growable tail
```

The pool is the only growable region. Everything before it is fixed at block
creation; the pool absorbs all mremap growth. Initial block is 2 MiB; the pool
starts at 1.57 MiB and grows toward `OMNI_BLOCK_MAX_SIZE`.

## 3. Header (offset 0, 192 bytes used, region 256)

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `magic` | `OMNI_MAGIC`; mismatch means wrong file |
| 4 | 4 | `format_version` | `OMNI_FORMAT_VERSION`; mismatch means incompatible layout |
| 8 | 4 | `block_size` | current mapping size, always a multiple of 4096 |
| 12 | 4 | `header_size` | `OMNI_HEADER_SIZE`; sections start at 0x1000 |
| 16 | 4 | `futex` | atomic commit lock. 0 = free, 1 = held |
| 20 | 1 | `state` | `0` = CREATING, `1` = READY (set after core keys seeded) |
| 21 | 3 | `reserved` | |
| 24 | 8 | `generation` | bumped once per commit; equals the newest journal `seq` |
| 32 | 8 | `epoch` | per-instance id; consumers key resync on this |
| 40 | 8 | `boot_time_ns` | CLOCK_MONOTONIC at creation |
| 48 | 8 | `capabilities` | bit per present section, see below |
| 56 | 4 | `wm_pid` | owner pid, for dead-compositor detection |
| 60 | 4 | `reserved` | |
| 64 | 8 | `pool_base` | offset of the pool (`OMNI_POOL_OFF`) |
| 72 | 8 | `pool_size` | current pool extent |
| 80 | 8 | `arena_end` | arena cursor, bumping up from `pool_base` (16-aligned) |
| 88 | 8 | `region_head` | region cursor, bumping down from `pool_base + pool_size` (16-aligned) |
| 96 | 4 | `catalog_free_head` | freelist head slot index; `0xFFFFFFFF` = empty |
| 100 | 4 | `catalog_free_count` | free slots remaining |
| 104 | 4 | `reserved` | |
| 108 | 4 | `reserved` | |

Capabilities bits (u64, default `0x3F` = all sections present):

```
bit 0  HAS_CATALOG     bit 3  HAS_REQUESTS
bit 1  HAS_ARENA       bit 4  HAS_REGIONS
bit 2  HAS_JOURNAL     bit 5  HAS_SOCKET (optional facade)
```

Mappers must not trust the block until the header parses cleanly against
`OMNI_MAGIC`, `OMNI_FORMAT_VERSION`, and `state == READY`. `state` is set to
READY after the WM seeds its `wm.*` core keys (`configstorage.md` §13).

## 4. Section table (offset 0x100, 16 rows x 32 bytes)

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `id` | one of the ids below; `id == 0` is NONE (unused row) |
| 4 | 4 | `offset` | section offset in the block |
| 8 | 8 | `size` | section size in bytes |
| 16 | 4 | `flags` | reserved |
| 20 | 4 | `reserved` | |

Row order is fixed; `id == row`. Fixed ids:

```
0 NONE     1 CATALOG   2 ARENA     3 JOURNAL
4 REQUESTS 5 REGIONS
```

`ARENA` and `REGIONS` are windows into the same physical pool:
`ARENA.offset = pool_base`, `ARENA.size = arena_end - pool_base`;
`REGIONS.offset = region_head`, `REGIONS.size = (pool_base + pool_size) - region_head`.
Both windows change size on growth/allocation, never their base for handed-out
area. Readers use the windows as read bounds for structural verification.

## 5. The storage pool

Two cursors in the header carve one contiguous tail:

```
pool_base                            pool_base + pool_size
   |  arena data (bump up)  | free  |  region payloads (bump down)  |
   ^                     arena_end   region_head                    ^
```

- Arena allocations: `need = 16 + align16(payload_len)` for framed values.
  If `arena_end + need > region_head`, the pool must grow first (see §7).
  Else write the frame at `arena_end`, set `arena_end += need`.
- Region allocations: `need = align16(stride * height * slot_count)`.
  If `region_head - need < arena_end`, grow first. Else set
  `region_head -= need`, the new payload offset is the new `region_head`.
- Reclaim: arena space is never reclaimed except the in-place overwrite of an
  existing value (§6, decision 4). Region space is reclaimed only when the
  destroyed region is the trailing allocation (highest offset), then
  `region_head += freed`.
- Alignment: every allocation base is 16-aligned. Zero pages only hold at
  creation; after reuse, writers initialize every byte they own.

### Arena value frame (every named/value payload)

```
+0  u32 length       payload bytes, not counting the 16-byte header
+4  u32 reserved (0)
+8  u64 reserved (0)
+16 payload[length]  -> allocation continues to next multiple of 16
```

`body_ref` and `name_ref` point at the frame start (offset +0). A name is a
string frame: `length = strlen + 1` including the NUL. For entries whose value
is inline-capable this framing is not used; the catalog + journal carry scalar
values in their own inline fields. Framing exists only for strings, blobs,
arrays, tuples and other variable-size payloads.

## 6. Catalog (section base 0x1000, 16,384 x 16 B)

Entry header:

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `name_ref` | arena offset of framed name; when FREE, the next-free index |
| 4 | 2 | `type_tag` | `configstorage.md` §4 tag (0x01..); 0x8000+ extension |
| 6 | 2 | `flags` | bit0 MUTABLE, bit1 DESTROYED, bit2 FREE; rest reserved |
| 8 | 4 | `length` | payload length (`frame.length` when framed) |
| 12 | 4 | `body_ref` | arena offset of framed payload; for `region_ref` entries this is the region descriptor index instead |

Freelist: a FREE entry keeps `name_ref` as `next_free_index`
(`0xFFFFFFFF` = end), `type_tag = 0`, `length = 0`, `body_ref = 0`. Allocation
takes `catalog_free_head`, calls the slot via the chain in `catalog_free_head`
under the futex. Entries never move once live; their slot index is the stable
entry id used by `entry_ref` values, journal `key_ref`, and request responses.

Value read path: verify slot `< 16384`, flags `(MUTABLE set or extension),
DESTROYED` unset, `FREE` unset, `type_tag` known, `name_ref` and `body_ref`
inside `[pool_base, arena_end)`, `length` inside
`[body_ref + 16, arena_end)` once framed, alignment 16. Any violation refuses
the entry, logs once, skips.

Overwrite rule (decision 4): on `set` of an existing key, if the new payload
fits the existing frame (`align16(new_len) <= align16(old_len)`), write in
place and update `length`. Otherwise bump a new frame, update `body_ref` +
`length`; the old frame leaks until the block is recreated. Names are
replaceable the same way and leak likewise.

Delete: mark DESTROYED, then FREE (push to freelist head). The frame bytes for
name and payload remain in the arena.

## 7. Growth

Growth is end-append on the pool (`configstorage.md` §9):

1. compute `need_pool = max(needed, pool_size * OMNI_POOL_GROWTH)`,
   capped at `MAX_POOL`;
2. `new_block = OMNI_POOL_OFF + need_pool`, page-aligned;
3. `ftruncate` the SHM file to `new_block`, then `mremap` the mapping;
4. update `header.block_size`, `header.pool_size`; cursors unchanged;
5. bump `generation` as part of the same commit that triggered growth.

Mappers detect growth on any generation change: header `block_size` differs
from their mapping extent, so they ftruncate-and-remap to match, then re-read
the section table (`configstorage.md` §9). Bases do not move for either window;
handed-out offsets stay valid.

Exhaustion: when the cap blocks a needed allocation on either side, the
request fails with `BLOCK_EXHAUSTED`; the block never silently truncates.

## 8. Journal (section base 0x41000)

Ring word at section start: a single aligned u64.

```
+0  u64 ring   lo32 = head index, hi32 = count (count in [0, 4096])
+8  slots, slot_i at 8 + i * 40
```

One store updates `ring` after the slot write, so lock-free readers observe a
consistent head/count pair. Append:

```
idx  = (head + count) % OMNI_JOURNAL_CAPACITY
write slot[idx]                 # fields below
if count == OMNI_JOURNAL_CAPACITY:  head = (head + 1) % CAP  # drop oldest
else:                               count += 1
ring = (head << 32) | count
```

Slot:

| off | size | field | meaning |
|---|---|---|---|
| 0 | 1 | `kind` | 1 KEY_SET, 2 KEY_DELETE, 3 EVENT |
| 1 | 1 | `reserved` | |
| 2 | 2 | `type_tag` | KEY_SET: value type; EVENT: event category; else 0 |
| 4 | 4 | `key_ref` | catalog slot index (0 for EVENT) |
| 8 | 8 | `seq` | the commit's `generation` value |
| 16 | 8 | `time_ns` | CLOCK_MONOTONIC |
| 24 | 8 | `value_inline` | value bytes if `type` size <= 8, zero-padded to 8 |
| 32 | 4 | `body_ref` | arena frame offset for non-inline values / EVENT payload; else 0 |
| 36 | 4 | `reserved` | |

Inline rule (decision 6): scalars of size <= 8 (`bool`..`option`, fixed vecs
up to 8 bytes, `keysym`, `modmask`, ids...) ride in `value_inline`. Strings,
blobs, arrays, tuples, gradients always use `body_ref`. EVENT uses `body_ref`
for a framed free-form string payload. Stale bytes in recycled slots must be
treated as garbage: readers parse only by `kind` + `type_tag`+ verified refs.

Resync (`configstorage.md` §5): entries carry `epoch`; a consumer whose cursor
lives in a different epoch drops it, reads a generation-stable catalog
snapshot, then follows the ring forward.

## 9. Request queue (section base 0x6A000, 64 x 256 B)

| off | size | field | meaning |
|---|---|---|---|
| 0 | 1 | `status` | 0 FREE, 1 PENDING, 2 DONE, 3 ERROR |
| 1 | 1 | `type` | 1 CREATE_ENTRY, 2 CREATE_REGION, 3 DESTROY_ENTRY, 4 DESTROY_REGION |
| 2 | 1 | `result_code` | 0 OK when done, else error code |
| 3 | 1 | `reserved` | |
| 4 | 4 | `ticket` | monotonically increasing, arrival order |
| 8 | 4 | `requester_pid` | for lazy reclaim of crashed writers |
| 12 | 4 | `target` | DESTROY: the entry/region id; else 0 |
| 16 | 2 | `type_tag` | CREATE_ENTRY initial value type (0 = no value) |
| 18 | 2 | `reg_format` | CREATE_REGION pixel format |
| 20 | 4 | `width` | CREATE_REGION width in px |
| 24 | 4 | `height` | CREATE_REGION height in px |
| 28 | 1 | `slot_count` | 0/1/2 for CREATE_REGION (0 treated as 1) |
| 29 | 3 | `reserved` | |
| 32 | 4 | `value_len` | CREATE_ENTRY initial payload bytes (<= 128) |
| 36 | 64 | `name` | NUL-terminated, max `OMNI_REQUEST_NAME_MAX` |
| 100 | 128 | `value` | CREATE_ENTRY initial payload |
| 228 | 28 | `reserved` | |

Protocol (`configstorage.md` §8): requester takes the commit futex, finds a
FREE slot, fills it (PENDING, new ticket), releases. The WM drains once per
tick: validates structurally, creates, appends the discovery journal entry
(KEY_SET), sets DONE + `target` (new id), appends REQUEST_DONE event. Requester
reads its slot, then under the futex flips back to FREE. Crash reclaim: WM
lazily frees PENDING/DONE slots whose `requester_pid` is gone.

Error codes:

```
1 PARAM_INVALID   2 NAME_TOO_LONG    3 CATALOG_FULL
4 ARENA_FULL      5 REGION_FULL      6 REGION_TOO_LARGE
7 BLOCK_EXHAUSTED 8 BAD_TARGET
```

## 10. Region descriptors (inside the pool's REGIONS window)

A `CREATE_REGION` produces one catalog entry of type `region_ref` whose
`body_ref` is the region id, and one descriptor. Descriptors are a fixed
linear array of 64 x 32 B at the pool start, before any region payload bytes.

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `payload_offset` | block offset of pixels, 16-aligned |
| 4 | 4 | `payload_bytes` | stride * height * slot_count |
| 8 | 4 | `stride` | bytes per row |
| 12 | 2 | `width` | px |
| 14 | 2 | `height` | px |
| 16 | 1 | `format` | 0 ARGB8888, 1 RGBA8888, 2 XRGB8888 (all bpp 4) |
| 17 | 1 | `slot_count` | 1 or 2 |
| 18 | 1 | `flags` | bit0 IN_USE, bit1 DESTROYED |
| 19 | 1 | `reserved` | |
| 20 | 4 | `seq` | painter's publish counter per §7 of configstorage.md |
| 24 | 8 | `reserved` | |

Validation on creation: `stride >= width * 4`, `stride * height <=
OMNI_REGION_SLOT_MAX_BYTES` (34 MiB, one frame), `slot_count` in [1,2], total
payload fits the pool. Find a free descriptor by linear scan of `flags`.

Publish/read semantics match `configstorage.md` §7 exactly: the painter writes
pixels for the active slot (`seq & 1` when `slot_count == 2`, else slot 0),
then stores `seq + 1` as the final aligned u32 write. The WM uploads on
observed `seq` change and caches an applied copy; a destroyed source region
cannot tear down the applied copy.

## 11. Invariants (checked; the fuzz target asserts these)

```
I1  OMNI_POOL_OFF <= arena_end <= region_head <= pool_base + pool_size
I2  every offset/length dereferenced lies in [0, block_size)
I3  block_size multiple of 4096; header_size <= block_size <= MAX
I4  catalog freelist is an acyclic index chain, head/count consistent
I5  journal count <= OMNI_JOURNAL_CAPACITY
I6  every arena allocation base and every region payload base is 16-aligned
I7  no live reader result depends on stale bytes of a recycled slot
I8  exactly one writer at a time (futex protocol, incl. machine writers)
```

Guard bundle on every dereference: offsets in block bounds, lengths in
declared capacity and block bounds, known type tags, generation-stable
snapshot, 16 alignment. Violation = refuse entry, log once, skip-and-continue
(`configstorage.md` §12).

## 12. Commit protocol, concrete

1. take `header.futex`
2. `newgen = header.generation + 1`
3. mutate catalog / arena / region descriptors as needed
4. append journal entries (each with `seq = newgen`, `time_ns`, `epoch`)
5. aligned atomic store `header.generation = newgen`
6. release `header.futex`

Readers: load `generation` once (aligned, atomic), take the snapshot, load
again, retry on mismatch.

Worked example (single-key set): store `wm.focus.color` = u32 0xFF0000FF.
gen goes 42 -> 43. Journal slot appended: kind=KEY_SET, key_ref=idx of the
entry, type=0x08 (u32), value_inline = 0xFF0000FF zero-padded, seq=43,
time_ns=now. Entry body: fits the existing frame, so overwritten in place.
No arena growth.

## 13. Open items

- `OMNI_CATALOG_SLOT_COUNT = 16384` and `OMNI_ENTRY_NAME_MAX = 4095` are my
  picks; adjust freely, they only widen/narrow the fixed array.
- Region `slot_count == 2` parity convention is assumed sufficient for v1
  double buffering; revisit if painters need arbitration.
- Request slots permit at most 128 bytes of initial value; larger CREATE_ENTRY
  values must go through `set` after the empty entry exists.
- The future `include/shared/omni_layout.h` must be the only home for every
  constant in §2.