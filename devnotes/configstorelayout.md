# Config store: field-level layout

This is the executable refinement of `configstorage.md`. It fixes exact struct
layouts, byte offsets, alignment, capacities and v1 constants. Every value in
this document is a v1 default; nothing here changes the concepts in
`configstorage.md`, it only pins them down.

The identity contract from `configstorage.md` §0.1 is authoritative. This
layout includes the required epoch, commit, journal, and allocation-generation
fields, so the tables below are the current ABI. Do not write code against any
older table.

The ABI was frozen when these tables were written and the freeze was lifted on
2026-09-26, because the compositor subsystems are still at design stage and a
tag-table or payload change is cheap now and expensive after stage 6. The
concepts in `configstorage.md` are not what was frozen and are not in question;
what is reopened is the tag table, the composite payload shapes, the fixed
section set, and the v1 capacity constants. `devnotes/layoutengine.md` §2.9
states the reopening and §2.10 lists the changes the layout engine asked for.
That list has been applied here: two tags (`OMNI_TAG_CONSTRAINT` as the
composite program tree, `OMNI_TAG_CLIENT_RULE` as a 24-byte record with no
payload), a framed `OMNI_TAG_BINDING`, a composite `OMNI_TAG_MAP` for the
window-rule block, and a seventh fixed section for the solved layout.
`OMNI_TAG_MAX_KNOWN` is `0x34` and the unassigned hole starts at `0x35`.
`OMNI_FORMAT_VERSION` stays 1, because the first runnable build is v1 and there
is no earlier format to be incompatible with.
Every number here still has exactly one home, `include/shared/omni_layout.h`,
per §14.

All offsets that remain below are relative to the start of the SHM mapping.
Native endianness. Natural alignment is used for every field; section bases,
fixed arrays, arena allocations, and atomic 64-bit fields are 16-byte aligned.
Mixed-endianness remains out of scope.

## 1. Conventions

- The block is treated as hostile input on every read. All numbers below are
  bounds that a read path verifies before dereferencing.
- Writers serialize on the single commit futex in the header. Readers never
  lock; they rely on the `commit_id` counter. Aligned 8-byte loads/stores of
  `commit_id` and the journal ring metadata use atomic builtins; this is
  expected on x86/aarch64 for aligned access and is a required property on any
  platform.
- An "arena value frame" is the storage unit of every named and payload value
  in the pool (see §5). Region payloads are raw pixels, unframed.
- Every structural invariant has a numbered entry in §12; the future fuzz
  target asserts that list (`configstorage.md` §12).

## 2. Constants (single definition point)

All numbers live in one header, `include/shared/omni_layout.h`. This document is
the source of truth for the values; the header is the single place in code that
states them. Changing a value here must be the only edit needed, and no constant
is ever duplicated elsewhere (`filestructure.md`, `include/shared`).

| constant | value | meaning |
|---|---|---|
| `OMNI_MAGIC` | `0x4F574D42` | "OWMB" in memory-writing order |
| `OMNI_FORMAT_VERSION` | `1` | v1 is the first runnable build; the compositor did not exist when these tables were written, so a change now is a change to the only format there has ever been, not a change to a released one |
| `OMNI_ALIGN` | `16` | section, allocation, and atomic alignment |
| `OMNI_BLOCK_INITIAL_SIZE` | `4 MiB` | created block size (page multiple) |
| `OMNI_BLOCK_MAX_SIZE` | `512 MiB` | hard cap on the mapping |
| `OMNI_HEADER_SIZE` | `0x300` | header fields plus section table |
| `OMNI_SECTION_TABLE_OFFSET` | `0x100` | fixed section table location |
| `OMNI_SECTION_TABLE_SIZE` | `0x200` | 16 rows x 32 bytes |
| `OMNI_SECTION_SLOT_COUNT` | `16` | fixed section table rows |
| `OMNI_FIRST_SECTION_OFF` | `0x1000` | first page-aligned section base |
| `OMNI_CATALOG_OFF` | `0x1000` | catalog section base |
| `OMNI_CATALOG_SLOT_COUNT` | `16384` | fixed catalog capacity |
| `OMNI_CATALOG_ENTRY_SIZE` | `32 B` | catalog entry size |
| `OMNI_ENTRY_NAME_MAX` | `4095 B` | max name bytes including NUL |
| `OMNI_CATALOG_INDEX_OFF` | `0x81000` | catalog name index section base |
| `OMNI_CATALOG_INDEX_HEADER_SIZE` | `32 B` | name index header |
| `OMNI_CATALOG_INDEX_SLOT_COUNT` | `16384` | one index slot per catalog slot |
| `OMNI_CATALOG_INDEX_SLOT_SIZE` | `16 B` | hash, entry_id, generation low half |
| `OMNI_JOURNAL_OFF` | `0xC2000` | journal section base |
| `OMNI_JOURNAL_HEADER_SIZE` | `32 B` | journal ring metadata |
| `OMNI_JOURNAL_CAPACITY` | `4096` | ring slots, constant at v1 |
| `OMNI_JOURNAL_SLOT_SIZE` | `64 B` | fixed-size ring slot |
| `OMNI_JOURNAL_INLINE_LIMIT` | `8 B` | values <= 8 bytes go inline |
| `OMNI_REQUESTS_OFF` | `0x103000` | request section base |
| `OMNI_REQUEST_SLOT_COUNT` | `256` | fixed request slots |
| `OMNI_REQUEST_SLOT_SIZE` | `256 B` | fixed request slot size |
| `OMNI_REQUEST_NAME_MAX` | `4095 B` | max name bytes including NUL |
| `OMNI_REQUEST_VALUE_MAX` | `128 B` | inline request initial-value cap |
| `OMNI_REQUEST_RECLAIM_MS` | `5000` | terminal-slot reclaim deadline |
| `OMNI_REGION_DESC_OFF` | `0x113000` | fixed region descriptor table |
| `OMNI_REGION_DESC_SIZE` | `32 B` | region descriptor size |
| `OMNI_REGION_DESC_COUNT` | `64` | fixed region descriptors |
| `OMNI_REGION_SLOT_COUNT_MAX` | `2` | double buffering is 1 or 2 |
| `OMNI_REGION_FRAME_MAX_BYTES` | `34 MiB` | one frame maximum |
| `OMNI_REGION_TOTAL_MAX_BYTES` | `68 MiB` | two-frame maximum |
| `OMNI_SOLVED_OFF` | `0x113800` | solved layout section base |
| `OMNI_SOLVED_SECTION_SIZE` | `0x8000` | solved layout section size |
| `OMNI_SOLVED_HEADER_SIZE` | `16 B` | solved layout section header |
| `OMNI_SOLVED_NODE_SIZE` | `24 B` | one placed node |
| `OMNI_SOLVED_SLOT_COUNT_MAX` | `1024` | maximum nodes in one solve |
| `OMNI_HDR_OFF_ARENA_FREE_HEAD` | `164` | arena free-list head, in the header |
| `OMNI_POOL_OFF` | `0x11C000` | storage pool base |
| `OMNI_POOL_GROWTH` | `2` | doubling factor on pool growth |

Derived values for v1 (formulas, not constants):

```
CATALOG_SIZE   = OMNI_CATALOG_SLOT_COUNT * OMNI_CATALOG_ENTRY_SIZE
               = 0x80000
CATALOG_INDEX_OFF = align16(OMNI_CATALOG_OFF + CATALOG_SIZE, 0x1000)
               = 0x81000
CATALOG_INDEX_SIZE = OMNI_CATALOG_INDEX_HEADER_SIZE
               + OMNI_CATALOG_INDEX_SLOT_COUNT * OMNI_CATALOG_INDEX_SLOT_SIZE
               = 0x40020
JOURNAL_OFF    = align16(CATALOG_INDEX_OFF + CATALOG_INDEX_SIZE, 0x1000)
               = 0xC2000
JOURNAL_SIZE   = OMNI_JOURNAL_HEADER_SIZE
               + OMNI_JOURNAL_CAPACITY * OMNI_JOURNAL_SLOT_SIZE
               = 0x40020
REQUESTS_OFF   = align16(JOURNAL_OFF + JOURNAL_SIZE, 0x1000)
               = 0x103000
REQUESTS_SIZE  = OMNI_REQUEST_SLOT_COUNT * OMNI_REQUEST_SLOT_SIZE
               = 0x10000
REGION_DESC_OFF = REQUESTS_OFF + REQUESTS_SIZE
               = 0x113000
REGION_DESC_SIZE = OMNI_REGION_DESC_COUNT * OMNI_REGION_DESC_SIZE
               = 0x800
SOLVED_OFF      = REGION_DESC_OFF + REGION_DESC_SIZE
               = 0x113800
SOLVED_BYTES    = OMNI_SOLVED_HEADER_SIZE
               + OMNI_SOLVED_NODE_SIZE * OMNI_SOLVED_SLOT_COUNT_MAX
               = 0x6010
FIXED_END       = align16(SOLVED_OFF + OMNI_SOLVED_SECTION_SIZE, 0x1000)
               = 0x11C000
INITIAL_POOL    = OMNI_BLOCK_INITIAL_SIZE - FIXED_END = 0x2E4000
MAX_POOL        = OMNI_BLOCK_MAX_SIZE - FIXED_END = 0x1FEE4000
```

`SOLVED_BYTES` must be less than `OMNI_SOLVED_SECTION_SIZE`, or a full solve
would not fit its own section. `0x6010` against `0x8000` leaves `0x1FF0` bytes
unused, which is deliberate slack rather than a value to spend: a solve that
fills the section exactly has no room for a count that disagrees with the
header, and the section is single-buffered, so a partial write is visible.

`OMNI_POOL_OFF` is `FIXED_END`. The initial block is 4 MiB because the fixed
region is now dominated by the catalog and its index, 768KB together; at 2 MiB
the initial pool would be `0x25000` (148 KiB), less than one 1080p RGBA frame.
The mapping is demand-paged, so the untouched tail of a 4 MiB block costs no
memory.

**The request queue no longer sets the fixed-region size.** It was 1.1MB of the
old 1.86MB fixed area, entirely because of 256 inline 4096-byte name buffers,
and moving the name into the arena cut it to 64KB. The index added 256KB back,
so the fixed area is `0x11C000` against the previous `0x1DB000`: the block is
764KB smaller overall while `get` gained a lookup structure it did not have.

Layout overview (one contiguous mapping):

```
0x000000  header fields: 0x000..0x0A7; reserved through 0x0FF
0x000100  section table : 16 x 32 B (0x200 bytes)
0x001000  catalog      : 16,384 x 32 B entry headers (fixed)
0x081000  catalog index: 32 B header + 16,384 x 16 B, sorted by name hash
0x0C2000  journal      : 32 B ring metadata + 4,096 x 64 B slots (fixed)
0x103000  requests     : 256 x 256 B slots (fixed)
0x113000  region desc  : 64 x 32 B descriptors (fixed)
0x113800  solved layout: 16 B header + up to 1,024 x 24 B nodes (fixed)
0x11C000  pool         : growable arena + region payload tail
```

The pool is the only growable region. Everything before it is fixed at block
creation; the pool absorbs all mremap growth. The initial block is 4 MiB, so the
initial pool is `0x225000` bytes and grows toward `OMNI_BLOCK_MAX_SIZE`.

## 3. Header (offset 0, fields 0x000..0x0A7, region 0x300)

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `magic` | `OMNI_MAGIC`; mismatch means wrong file |
| 4 | 4 | `format_version` | `OMNI_FORMAT_VERSION`; mismatch means incompatible layout |
| 8 | 4 | `block_size` | current mapping size, always a multiple of 4096 |
| 12 | 4 | `header_size` | `OMNI_HEADER_SIZE`; sections start at `OMNI_FIRST_SECTION_OFF` |
| 16 | 4 | `futex` | atomic commit lock. 0 = free, 1 = held |
| 20 | 1 | `state` | `0` = CREATING, `1` = READY, `2` = BROKEN |
| 21 | 3 | `reserved` | |
| 24 | 8 | `commit_id` | published once per commit; never a journal cursor |
| 32 | 8 | `epoch` | per-instance id; consumers key resync on this |
| 40 | 8 | `boot_time_ns` | CLOCK_MONOTONIC at creation |
| 48 | 8 | `capabilities` | bit per present section, see below |
| 56 | 4 | `wm_pid` | owner pid, for dead-compositor detection |
| 60 | 4 | `reserved` | |
| 64 | 8 | `pool_base` | fixed pool base (`OMNI_POOL_OFF`) |
| 72 | 8 | `pool_size` | current pool extent; grows only with the mapping |
| 80 | 8 | `arena_end` | arena cursor, bumping up from `pool_base` |
| 88 | 8 | `region_head` | region cursor, bumping down from `pool_base + pool_size` |
| 96 | 4 | `catalog_free_head` | freelist head slot index; `0xFFFFFFFF` = empty |
| 100 | 4 | `catalog_free_count` | free slots remaining |
| 104 | 4 | `request_ticket_next` | next ticket within this epoch; must not wrap |
| 108 | 4 | `region_desc_count` | `OMNI_REGION_DESC_COUNT` |
| 112 | 4 | `section_table_offset` | `OMNI_SECTION_TABLE_OFFSET` |
| 116 | 4 | `section_table_size` | `OMNI_SECTION_TABLE_SIZE` |
| 120 | 4 | `first_section_offset` | `OMNI_FIRST_SECTION_OFF` |
| 124 | 4 | `reserved` | |
| 128 | 8 | `region_desc_base` | fixed descriptor table base (`OMNI_REGION_DESC_OFF`) |
| 136 | 4 | `writer_pid` | PID of the current futex holder; zero when idle |
| 140 | 4 | `writer_token` | per-acquisition owner token; zero when idle |
| 144 | 4 | `commit_state` | `0` IDLE, `1` ACTIVE, `2` GROWING, `3` BROKEN |
| 148 | 4 | `reserved` | zero |
| 152 | 8 | `active_commit_id` | commit being published, or zero when idle |
| 160 | 4 | `ready` | `0` NOT_READY, `1` READY, `2` DEGRADED, `3` FAILED |
| 164 | 4 | `arena_free_head` | arena free-list head frame offset, or `OMNI_REF_NONE` |
| 168 | 88 | `reserved` | zero through offset 255 |

The section table starts at `0x100`, so the complete header region is
`OMNI_HEADER_SIZE == 0x300`. The last defined field is `arena_free_head` at
`0x0A4`, so fields from `0x0A8` through `0x0FF` are reserved and must be zero on
creation, which is the row above stating it as `reserved` from offset 168. The
free-list head was the last field added; before it the tail began at `0x0A4`. An earlier
draft of this section said the defined fields ended at `0x087`, which is where
an intermediate layout stopped before `region_head` and the readiness word were
added; the table has always been the authority and `OMNI_HDR_RESERVED_TAIL_START`
in `omni_layout.h` is 168, matching it (it was 164 before the free-list head
took the word at `0x0A4`). `commit_state` and `active_commit_id` are
accessed with aligned atomic operations. `writer_pid` and `writer_token` are
meaningful only while `futex == 1`.

Capabilities bits (u64, default `0xFF` = all store sections present):

```
bit 0  HAS_CATALOG       bit 4  HAS_REGION_DESC
bit 1  HAS_ARENA         bit 5  HAS_REGION_PAYLOAD
bit 2  HAS_JOURNAL       bit 6  HAS_SOCKET (optional facade)
bit 3  HAS_REQUESTS      bit 7  HAS_SOLVED_LAYOUT
bit 8  HAS_CATALOG_INDEX (not optional; see below)
```

Bit 7 is not optional, and bit 8 is not either, which is the one place where
this table has an answer with no fallback. A `get` by name reads the index or
it has no answer, and there is deliberately no catalog scan behind it, because
the alternative is a read path whose cost depends on how many unrelated keys the
user has configured, which is how a configuration file turns into a latency
problem for every client on the socket. So a block whose index row is absent or
malformed, or whose `OMNI_CATALOG_INDEX_VERSION` does not match, is one this
build cannot read: `get` returns `BLOCK_UNSUPPORTED` rather than scanning.
A consumer that sees `0x7F` is looking at a block written
before the solved layout section existed, and it must not read section 7; a
consumer that sees bit 7 clear must not read section 7 either. Either way the
absence is a fact about the block rather than an error, so the bit is a presence
statement and `OMNI_CAP_DEFAULT` covers all eight.

Mappers must not trust the block until the header parses cleanly against
`OMNI_MAGIC`, `OMNI_FORMAT_VERSION`, the expected header/section constants,
and `state == READY`. `state == BROKEN` is terminal for this epoch: readers
refuse the block and no writer may mutate it. `state` is set to READY after the
WM seeds its `wm.*` core keys (`configstorage.md` §13).

`ready` is a separate axis from `state`, and the separation is the point.
`state` answers "is this block structurally trustworthy", `ready` answers "is the
compositor able to service requests against it". A freshly created block is
`state == READY` while the WM is still activating components, and the two
conditions call for different client behaviour, so they are not the same field:

```
0 NOT_READY   block is valid, compositor is still activating
1 READY       all components activated; requests are serviceable
2 DEGRADED    serving, but a non-critical component failed to activate
3 FAILED      activation failed; the block stays readable but inert
```

`DEGRADED` exists so a partial failure is distinguishable from a total one, and
`FAILED` is distinct from `state == BROKEN` on purpose: a `FAILED` block is
intact and its data is readable, whereas a `BROKEN` block must be refused
outright. A client that conflated them would discard a recoverable
configuration on a component-init failure. The u32 width leaves room for
per-component detail without a second field.

## 4. Section table (offset 0x100, 16 rows x 32 bytes)

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `id` | one of the ids below; `id == 0` is NONE (unused row) |
| 4 | 4 | `offset` | section offset in the block |
| 8 | 8 | `size` | section size in bytes |
| 16 | 4 | `flags` | bit 0 PRESENT, bit 1 JOURNALLED, rest zero |
| 20 | 4 | `reserved` | zero |

Row order is fixed; `id == row`. Fixed ids:

```
0 NONE       1 CATALOG   2 ARENA          3 JOURNAL
4 REQUESTS   5 REGION_DESC 6 REGION_PAYLOAD 7 SOLVED_LAYOUT
```

`ARENA` and `REGION_PAYLOAD` are windows into the same physical pool:
`ARENA.offset = pool_base`, `ARENA.size = arena_end - pool_base`;
`REGION_PAYLOAD.offset = region_head`,
`REGION_PAYLOAD.size = (pool_base + pool_size) - region_head`.
Both windows change size on growth/allocation, never their base for handed-out
area. Readers use the windows as read bounds for structural verification.

`flags` is a protocol marker, not ownership or access control, and the only bit
that changes behaviour is `JOURNALLED`. A row without it is written by its
producer alone: it is never appended to the journal and never included by a save.
`SOLVED_LAYOUT` is the one section that leaves `JOURNALLED` clear, because
journaling a solve would make the most frequent event in the compositor the most
expensive one, and saving one would store geometry that the next solve overwrites
before anyone reads it. The section is a readable projection of solver state, not
configuration.

## 5. The storage pool

Two cursors in the header carve one contiguous tail:

```
pool_base                            pool_base + pool_size
   |  arena data (bump up)  | free  |  region payloads (bump down)  |
   ^                     arena_end   region_head                    ^
```

- Arena allocations: `need = 16 + align16(payload_len)` for framed values.
  If `arena_end + need > region_head`, the pool must grow first (see §7).
  Otherwise write the frame at `arena_end` and set `arena_end += need`.
- Region allocations: `need = align16(frame_bytes * slot_count)`, where
  `frame_bytes = stride * height`. If `region_head - need < arena_end`, grow
  first. Otherwise set `region_head -= need`; the new payload offset is the
  new `region_head`.
- Reclaim, arena: a frame is freed to a free list, not leaked. The free list is
  a singly linked chain threaded through the free frame's own header
  (`OMNI_FRAME_OFF_NEXT_FREE`), headed by `OMNI_HDR_OFF_ARENA_FREE_HEAD`, and
  allocation is first-fit over the chain before falling back to the bump
  cursor. A free frame's `length` holds its **total** frame bytes rather than a
  payload length, so a split writes a valid header into the remainder; that is
  what lets first-fit satisfy a small request from a large freed frame.
  Coalescing adjacent free frames is an optimization, not a requirement: without
  it the pool fragments, and with it the pool does not.
- Reclaim, region: region space is reclaimed only when the destroyed region is
  the trailing allocation (highest offset), then `region_head += freed`.
- Alignment: every allocation base is 16-aligned. Zero pages only hold at
  creation; after reuse, writers initialize every byte they own.

### Arena value frame (every named/value payload)

```
live frame
+0  u32 length       payload bytes, not counting the 16-byte header
+4  u32 next_free    0; non-zero means the frame is on the free list
+8  u64 reserved (0)
+16 payload[length]  -> allocation continues to next multiple of 16

free frame, same bytes
+0  u32 length       total frame bytes, header included
+4  u32 next_free    next free frame offset, or OMNI_REF_NONE for the tail
+8  u32 free_size    == length; present so a split can rewrite this header
+12 u32 reserved (0)
```

The live layout is byte-identical to what it was before the free list existed;
only the meaning of the reserved bytes for a *free* frame is new. `length` is
therefore the discriminator's companion: a reader following a `body_ref` must
reject a frame whose `next_free` is non-zero, because that frame has been
recycled and its contents are a free-list link rather than a value. §6's guard
tiers apply this at L4.

`name_ref` and framed `body_ref` point at the frame start (offset +0). A name
is a UTF-8 string frame: `length = strlen + 1` including the NUL. Names are
immutable for the lifetime of the catalog entry; replacing a name creates a
new entry identity. A frame is not used for an inline value.

### Composite payload records (tags with a fixed-shape header)

Not every payload is a tree. Four tags carry a fixed-size record, and this
document owns their bytes, so they are tabulated here rather than only in
`configstorage.md` §4, which owns what each field *means*. The constants are
`OMNI_BINDING_OFF_*` and `OMNI_CLIENT_RULE_OFF_*`, both in `omni_layout.h`
section 7, and each record has a static assert on its tail.

`binding` is `0x2B`, a fixed header with the argument array inline after it, so
the whole payload is one frame of `header_bytes + array_bytes`:

| off | size | field | notes |
|---|---|---|---|
| 0 | 4 | `modmask` | u32 bitmask, positions in `configstorage.md` §4 |
| 4 | 4 | `keysym` | u32 XKB keysym |
| 8 | 4 | `keycode` | u32 evdev code, one of three |
| 12 | 4 | `action_ref` | u32, see below |
| 16 | 4 | `args_ref` | u32 frame offset, or `OMNI_REF_NONE` |
| 20 | 4 | `flags` | u32, bit 0 `USE_KEYSYM` |
| 24 | .. | `args` | `array` of `string`, inline, positional |

**This table is the v1 minimum and it is now known to be too small**, which is
recorded here rather than left to be discovered during the port. `generaldesign.md`
§14.1 puts all of Mango's `KeyBinding` fields in the block, and this table has
room for one keycode of three, no keysym-vs-keycode discriminant beyond one flag
bit, no mode, and none of Mango's four remaining behaviour flags. The fields that
must exist are listed in `generaldesign.md` §14.1's table. The offsets are
**still deferred**, deliberately: the record grows again the moment tablet and
stylus bindings arrive in stage 10, and Mango has no struct for those to copy, so
fixing offsets now would fix them against a shape that stage 10 revises. What is
settled is the tag, the field set, and that the header is fixed-width with a kind
discriminant so `generaldesign.md` §14.4's index can hash the trigger fields
without walking a variable-length record.

`client_rule` is `0x33`, 24 bytes with no payload, so a frame of exactly 24:

| off | size | field | notes |
|---|---|---|---|
| 0 | 2 | `rule` | u16 `ALIGN`/`MATCH`/`SIZE`/`OFFSET`/`SNAP` |
| 2 | 2 | `against` | u16 `CLIENT`/`VIEWPORT`/`OUTPUT` |
| 4 | 2 | `axis` | u16 `X`/`Y`, `MATCH` only |
| 6 | 2 | `edge` | u16, `ALIGN` only |
| 8 | 2 | `region` | u16, the ten regions of `layoutengine.md` §3.7, `SNAP` only |
| 10 | 2 | reserved | zero |
| 12 | 4 | `width` | u32, `SIZE` only |
| 16 | 4 | `height` | u32, `SIZE` only |
| 20 | 4 | `by` | i32, `OFFSET` only, may be negative |

The other two composite tags have no byte table because they are not
fixed-shape. `constraint` is `0x32` and its payload is
`array of option`, one per space, with a rule as a named key of that option, so
its size is whatever those nested frames come to. `map` is `0x34` and its
payload is `array of entry` where an entry is a `tuple` of a framed key string
and any value, so likewise. Both are bounded by
`OMNI_VALUE_MAX_ARRAY_ELEMS` and `OMNI_VALUE_MAX_FRAMED` rather than by a
length this document could tabulate, and the nesting bound is
`OMNI_VALUE_MAX_NESTING`.

`action_ref` at offset 12 is a frame offset holding the action's name, and that
is settled: `helpers.md` §6.2 originally read it as a registry handle, which is
wrong because a binding is in the block and the block is the user's editable
configuration. A handle is a process-local index, so storing one would put a
value nobody can read in a file people edit, and it would not survive a restart
or a `save` and reload. The name is resolved through the registry when the key
fires, and an unregistered name is `ACTION_NOT_FOUND` on use rather than at set
time, because refusing the write would mean refusing to store a binding for a
component that has not activated yet, which is the normal state during boot.

## 6. Catalog (section base 0x1000, 16,384 x 32 B)

The catalog slot index is the `entry_id`. The following 32-byte entry is the
v1 representation:

| off | size | field | meaning |
|---|---|---|---|
| 0 | 8 | `entry_generation` | non-zero allocation lifetime; incremented on reuse |
| 8 | 4 | `name_ref` | arena frame offset; when FREE, next-free index |
| 12 | 2 | `type_tag` | `configstorage.md` §4 tag; `0x8000+` extension |
| 14 | 2 | `flags` | entry flags below; all other bits reserved |
| 16 | 4 | `length` | inline encoded length or framed payload length |
| 20 | 4 | `body_ref` | arena frame offset, descriptor index, or 0 for inline |
| 24 | 8 | `value_inline` | low bytes for an inline value or a reference generation |
| 32 | total | | |

Entry flags:

```
bit 0 reserved        bit 3 EPHEMERAL
bit 1 DESTROYED       bit 4 WINDOW_DEPENDENT
bit 2 FREE            bits 5..15 reserved
```

Bit 3 previously held `PERSIST`, which was removed. An unmarked entry is
ordinary configuration; scope is expressed by exclusion, not by opt-in.
`WINDOW_DEPENDENT` marks a value scoped to one window (per-client geometry,
focus, per-window overrides) and `EPHEMERAL` marks a value with no long-term
relevance, such as process bookkeeping or a cached derivation. Both are
excluded from `save`. The core and extension facades set these bits; the core
store never interprets namespace ownership. A `wm.*` convention is not
enforcement. Bit 0 is reserved and was `MUTABLE`, which no rule ever read.

`save` and a soft reset use one identical classification, which is the point of
this design:

```
save writes     WINDOW_DEPENDENT clear AND EPHEMERAL clear
soft reset      clears WINDOW_DEPENDENT clear AND EPHEMERAL clear
```

`WINDOW_DEPENDENT` dominates for reset: a per-window override of a
configuration option is never cleared, which is what lets the same option be set
generally and for a specific window. `EPHEMERAL` is independent of it and
excludes from both operations.

The scope bits are checked separately from the `FREE`/`DESTROYED` lifecycle
bits; a FREE or DESTROYED slot is skipped without a per-read diagnostic because
it is a normal steady state (`configstorage.md` §12.2).

Freelist: a FREE entry keeps its last `entry_generation`, uses `name_ref` as
`next_free_index` (`0xFFFFFFFF` = end), clears `type_tag`, `length`, and
`body_ref`, and sets `FREE` plus `DESTROYED`. Allocation takes
`catalog_free_head`, advances the chain under the futex, and increments the
retained generation (with zero treated as the first generation). Entries never
move while live; `entry_id` plus `entry_generation` is the complete reference.

Value read path, by guard tier (`configstorage.md` §12): L3 verifies the slot
index, a non-zero `entry_generation`, `FREE` and `DESTROYED` both clear, the
matching generation for a live reference, and 16-byte alignment. L4 applies the
tag decision, so a known tag requires its exact encoded length, an extension tag
requires `VALUE_FRAMED` plus a verified frame, and an unassigned low tag is
refused. A framed value requires `body_ref` in `[pool_base, arena_end)`, a
16-aligned frame, and the complete frame within `arena_end`, all written as
`length <= limit - offset`. An inline value requires `body_ref == 0` and a length
that matches its type. A `region_ref` requires `body_ref < OMNI_REGION_DESC_COUNT`, `length == 8`,
and a matching descriptor generation in `value_inline`; it has no arena frame.
An `entry_ref` uses a framed payload with u32 `entry_id`, u32 reserved, and
u64 `entry_generation`, for a total encoded length of 16 bytes. An extension tag
is accepted only as a framed value; the core validates the frame, not
extension semantics.

Overwrite rule (decision 4): on `set` of an existing key, if the new payload
fits the existing frame (`align16(new_len) <= align16(old_len)`), write in
place and update `length`. Otherwise allocate a new frame, first-fit from the
§5 free list and then from the bump cursor, and **free the old frame**. The
name frame is never replaced in place because names are immutable.

Delete: mark `DESTROYED`, tombstone the name's index slot in §6.1, then set
`FREE` and push the slot to the catalog freelist. **The name and payload frames
are freed to the arena free list.** A journal key reference remains tied to the
deleted entry generation and can never resolve to a reused slot, so freeing the
frames cannot resurrect it.

**This reverses a leak that was previously written into the design.** The earlier
text said the old frame "leaks until the block is recreated" and that a deleted
entry's "name and payload frames remain in the arena". Both were true, and both
meant a client that looped `set` and `delete` exhausted the pool with no way to
recover but block recreation, which is a denial of service reachable from the
socket. Freeing the frames closes it. Two properties make it safe:

- a stale `body_ref` cannot read recycled bytes as a value, because §12's L4 tier
  rejects a frame whose `next_free` is non-zero, and because the entry
  generation it was read under no longer matches;
- a freed frame is not zeroed, so anything that does read it sees a free-list
  header rather than a plausible-looking value, which is the failure mode the
  guards prefer.

## 6.1 Catalog name index (section base 0x81000, 16,384 x 16 B)

The index exists so that `get` never walks the catalog. It is a second, sorted
view of the same entries, not a second source of truth: the catalog slot is still
the entry, and `entry_id` is still the slot index.

```
header, 32 bytes
+0  u32 live_count     non-tombstone slots
+4  u32 used_count     live + tombstones; slots past this are unused
+8..31 reserved, zero

slot, 16 bytes, sorted ascending by (name_hash, entry_id)
+0  u64 name_hash      FNV-1a 64 then splitmix64, over the name bytes
+8  u32 entry_id       0xFFFFFFFF when the slot is a tombstone or unused
+12 u32 generation_lo  low half of the entry generation when inserted
```

- **The catalog itself is not sorted**, and this is the one place the design
  departs from the obvious implementation. `entry_id` is the slot index, live
  entries never move (§6), and the freelist chains through slots, so sorting the
  catalog in place would move live entries, invalidate every held
  `(entry_id, entry_generation)`, and make delete an O(n) memmove of the catalog
  rather than of a derived index. A separate index leaves the catalog alone and
  confines the cost of a delete to the index.
- **Lookup is a binary search, then a run walk.** Binary search on `name_hash`
  descends 14 levels for 16,384 slots, all within one 256KB section. A hash
  collision means several slots share a value, and because the array is sorted
  they are *contiguous*, so a hit walks the equal-hash run and compares names.
  The normal run length is 1, so a successful lookup costs one string compare,
  which it would need anyway to confirm the match, and a miss costs none.
- **Insert** places the slot at the upper bound of its hash and shifts the tail
  up, so insert is O(n) memmove of 16-byte slots. That is the cost of a sorted
  array and it is paid on `CREATE_ENTRY`, which is a configuration write.
- **Delete does not memmove.** It sets `entry_id` to
  `OMNI_CATALOG_INDEX_ENTRY_ID_TOMB`, which keeps the sort order intact, and
  compaction runs inside a commit when `used_count` exceeds twice `live_count`.
  This matters more than it looks: an O(n) delete is a loop a client can run,
  and delete-in-a-loop must not become a quadratic-cost denial of service.
- **The writer maintains it transactionally**, under the commit futex, in the
  same commit as the catalog mutation. It is therefore never stale, never
  rebuilt, and never a source of divergence: there is no window in which the
  catalog and the index disagree, because they are updated together or not at
  all.
- `generation_lo` is a filter, not the authority. A hit whose stored low half
  does not match the live catalog entry is skipped without reading further; the
  authoritative check remains §12's L3 test against the full u64 generation in
  the catalog slot.
- The L2 section guard applies here unchanged: `id == row`, 16-aligned offset,
  non-wrapping size inside `[0, block_size)`. Unlike every other section, a
  missing or malformed row is not a degradation: §3 above makes capability bit 8
  mandatory, so the block is one this build refuses rather than one it reads
  slowly.
- This interacts with §12.5's truncation rule and the two are consistent. §12.5
  covers a block *smaller* than the layout, where a reader uses the sections it
  can see. Growth here is end-append only and cursors never move, so a block
  cannot lose a fixed section it once had; an index that is gone or short is a
  block this build does not support, which is a different condition and gets a
  different answer.
- The result code for that refusal is **not yet decided**, and it is a real gap
  rather than an oversight: `omni_layout.h` enumerates request results
  (`OMNI_REQ_ERR_*`) for the queue, but a direct `get` has no read-result
  vocabulary at all, so there is nothing today to return. One is needed before
  this is implementable. It should be a separate `OMNI_GET_*` set rather than a
  reuse of `OMNI_REQ_ERR_*`, because a read has no slot to hold a `ticket` and a
  reused code would imply a request lifecycle that does not happen. This is
  recorded in §14.

## 7. Growth

Growth is end-append on the pool (`configstorage.md` §9):

1. compute `need_pool = max(needed, pool_size * OMNI_POOL_GROWTH)`,
   capped at `MAX_POOL`;
2. `new_block = OMNI_POOL_OFF + need_pool`, page-aligned;
3. `ftruncate` the SHM file to `new_block`, then `mremap` the mapping;
4. update `header.block_size`, `header.pool_size`; cursors unchanged;
5. append a store-growth event and publish the new `commit_id` as part of the
   same commit that triggered growth.

Mappers detect growth on any `commit_id` change: header `block_size` differs
from their mapping extent, so they ftruncate-and-remap to match, then re-read
the section table (`configstorage.md` §9). Bases do not move for either window;
handed-out offsets stay valid.

Exhaustion: when the cap blocks a needed allocation on either side, the
request fails with `BLOCK_EXHAUSTED`; the block never silently truncates.

## 8. Journal (section base 0xC2000)

The journal section starts with a 32-byte ring metadata block:

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `head` | oldest retained slot index, `[0, 4096)` |
| 4 | 4 | `count` | retained slot count, `[0, 4096]` |
| 8 | 8 | `oldest_journal_seq` | sequence of the oldest retained entry |
| 16 | 8 | `next_journal_seq` | sequence assigned to the next append |
| 24 | 8 | `publish_seq` | even/odd journal metadata seqlock |

Slot `i` begins at `OMNI_JOURNAL_OFF + OMNI_JOURNAL_HEADER_SIZE +
i * OMNI_JOURNAL_SLOT_SIZE`. The slot layout is exactly 64 bytes:

| off | size | field | meaning |
|---|---|---|---|
| 0 | 8 | `epoch` | instance identity copied from the header at append time |
| 8 | 8 | `commit_id` | transaction that published the entry |
| 16 | 8 | `journal_seq` | unique append order within the epoch |
| 24 | 8 | `time_ns` | CLOCK_MONOTONIC publication time; diagnostic only |
| 32 | 8 | `entry_generation` | catalog allocation lifetime, or 0 for EVENT |
| 40 | 4 | `entry_id` | catalog slot, or 0 for EVENT |
| 44 | 2 | `type_tag` | KEY_SET value type, EVENT category, or 0 |
| 46 | 1 | `kind` | 1 KEY_SET, 2 KEY_DELETE, 3 EVENT, 4 COMMIT_END, 5 BINDS_UPDATED |
| 47 | 1 | `flags` | bit0 VALUE_FRAMED, bit1 ENTRY_VALID, bit2 EVENT_REF_VALID |
| 48 | 8 | `value_inline` | inline value bytes, zero-padded |
| 56 | 4 | `body_ref` | framed payload or EVENT payload reference, else 0 |
| 60 | 4 | `event_ref` | extension or request identity reference, else 0 |

For `KEY_SET` and `KEY_DELETE`, `ENTRY_VALID` must be set and
`entry_generation` must be non-zero. For `EVENT`, both entry fields are zero.
When `VALUE_FRAMED` is set, `body_ref` points to a verified arena frame and
`value_inline` is zero. Otherwise `body_ref` is zero and `value_inline` is the
low-order bytes of the value. Unknown tag ranges use the same framed rule and
are not semantically interpreted by the core.

For lifecycle and request events, `kind == EVENT`, `entry_id` and
`entry_generation` are zero, and `body_ref` points to a framed event payload.
A region lifecycle payload contains `epoch`, `entry_id`, `entry_generation`,
`descriptor_index`, and `region_generation`. A request-completion payload
contains `epoch` and `request_ticket`. `event_ref` is reserved for an
extension-defined reference and is zero for these core events.

`kind == BINDS_UPDATED` is `5`. It is a separate kind rather than a run of
`KEY_SET` entries because a binding change invalidates a *derived structure*, not
one cached value: a process-private binding index (`generaldesign.md` §14.4) has
to be rebuilt, and a client that only needs to know "the binds are not what I
read" needs one event rather than one per binding. It has no catalog identity, no
value, and `event_ref` counts the bindings changed in the group, so a watcher can
tell one rebind from a config reload. The writer appends it in the same commit as
the `KEY_SET` entries it summarises, which means a reader that processes the
journal in order sees the individual changes first and the invalidation last,
never the reverse.

`kind == COMMIT_END` has zero catalog identity, no value, and `event_ref`
equal to the number of preceding journal entries in the same commit. It is the
consumer-visible boundary for a grouped commit. A watcher may buffer grouped
entries until this marker; if the marker is lost to ring replacement, the
consumer must resynchronize from the oldest retained sequence.

Append while holding the commit futex. A writer first writes every complete
slot for the commit, including its `COMMIT_END`, and only then publishes the
ring metadata:

```
publish_seq = load(publish_seq) + 1        # odd: metadata is being changed
compute final head, count, oldest_journal_seq, and next_journal_seq
store head, count, oldest_journal_seq, and next_journal_seq
release-store publish_seq = odd_value + 1  # even: metadata is stable
```

`head` and `count` are updated as if each slot were appended in sequence order;
when the ring is full, the oldest slot is dropped and `oldest_journal_seq` is
read from the new head. A reader acquires `publish_seq`, reads metadata and
slots, acquires it again, and retries if it is odd or changed.

A slot is valid only after all fields and the ring metadata are published.
Readers treat stale recycled-slot bytes as garbage and parse only by verified
`kind`, `type_tag`, identity, and references.

Resync (`configstorage.md` §5): a cursor is `(epoch, journal_seq)` and is a
last-seen marker, so replay delivers `journal_seq` greater than the cursor. A
consumer holding `C` in the current epoch compares it against the published
`oldest_journal_seq`: `C < oldest_journal_seq - 1` is a gap, `C ==
oldest_journal_seq - 1` is exactly contiguous, and `C >= oldest_journal_seq` is
an ordinary forward read. The consumer's `gap_policy` decides whether a gap
terminates the subscription with `WATCH_GAP` or triggers a resynchronization.
Resynchronization is a fresh commit-id-stable snapshot taken at the current
ring boundary, never a jump to `oldest_journal_seq`, because a gap can bisect a
group whose `COMMIT_END` is no longer retained.

A consumer whose `epoch` differs from the header cannot use its cursor at all.
It reports `WATCH_EPOCH_CHANGED` and re-discovers the instance; no cursor
value recovers data from a previous block.

## 9. Request queue (section base 0x103000, 256 x 256 B)

| off | size | field | meaning |
|---|---|---|---|
| 0 | 1 | `status` | 0 FREE, 1 PENDING, 2 DONE, 3 ERROR |
| 1 | 1 | `type` | 1 CREATE_ENTRY, 2 CREATE_REGION, 3 DESTROY_ENTRY, 4 DESTROY_REGION |
| 2 | 1 | `result_code` | 0 OK when done, else error code |
| 3 | 1 | `flags` | reserved, zero |
| 4 | 4 | `ticket` | monotonically increasing within the epoch |
| 8 | 8 | `epoch` | compositor instance identity; request identity is `(epoch, ticket)` |
| 16 | 4 | `requester_pid` | owner pid, paired with `requester_start_id` |
| 20 | 4 | `target` | DESTROY: descriptor or catalog slot index; else 0 |
| 24 | 2 | `type_tag` | CREATE_ENTRY initial value type (0 = no value) |
| 26 | 2 | `reg_format` | CREATE_REGION pixel format |
| 28 | 4 | `width` | CREATE_REGION width in px |
| 32 | 4 | `height` | CREATE_REGION height in px |
| 36 | 1 | `slot_count` | 0/1/2 for CREATE_REGION (0 treated as 1) |
| 37 | 3 | `reserved` | zero |
| 40 | 4 | `value_len` | CREATE_ENTRY initial payload bytes (<= 128) |
| 44 | 4 | `name_len` | name bytes including NUL; <= `OMNI_REQUEST_NAME_MAX` |
| 48 | 8 | `target_generation` | destroy target generation; 0 for create |
| 56 | 4 | `name_ref` | arena frame offset of the name, or `OMNI_REF_NONE` |
| 60 | 4 | `reserved` | zero |
| 64 | 128 | `value` | CREATE_ENTRY initial payload |
| 192 | 4 | `requester_start_id` | process start time; disambiguates a reused pid |
| 196 | 4 | `reserved` | zero; exists to 8-align `terminal_at_ms` |
| 200 | 8 | `terminal_at_ms` | CLOCK_MONOTONIC ms the slot became terminal; 0 otherwise |
| 208 | 48 | `reserved` | zero |
| 256 | total | | |

**The name is an arena frame, not an inline field**, which is what shrinks the
slot from `0x1100` to 256 bytes. The queue was 1.1MB, about 58% of the block's
fixed area, and essentially all of that was 256 copies of a 4096-byte name
buffer, while the catalog got 512KB for 16,384 entries. The queue is now 64KB.

The ownership rule is what makes this correct rather than merely smaller: the
name frame belongs to the slot from submission until the slot is released or
reclaimed, and is freed to the §5 arena free list at that point. It is not
freed earlier, so the WM can always read the name while draining, and the
requester copies the name out of the arena *before* it releases the slot, for
the same reason it copies the whole slot: the results live in the slot, and the
name now lives next door.

`name_len` must include the terminating NUL and match the frame's payload
length, and `name_ref` must be 16-aligned inside `[pool_base, arena_end)` with
`next_free == 0`, so a name that has been freed and reused is detected rather
than read. An oversized or unterminated name is `PARAM_INVALID` and takes no
slot. A request with a value larger than 128 bytes creates an empty entry and
uses `set` afterward.

Protocol (`configstorage.md` §8). Submission takes the commit futex, finds a
`FREE` slot, fills it completely (`PENDING`, new `ticket`, current `epoch`,
`requester_pid`, `requester_start_id`), and releases. A request is refused
before any slot is taken when the header is not `READY` (`NOT_READY`) or is
`BROKEN` (`STORE_BROKEN`), so a refused request never consumes a ticket. When
no slot is free the request fails with `BLOCK_EXHAUSTED` rather than waiting on
the futex, so a dead requester cannot stall a live one.

State machine:

```
FREE    -> PENDING   requester wrote a complete slot
PENDING -> DONE      the WM applied the request in full
PENDING -> ERROR     the WM refused it; no partial mutation exists
DONE    -> FREE      requester acknowledged, or reclaim fired
ERROR   -> FREE      requester acknowledged, or reclaim fired
```

Only the requester returns a terminal slot to `FREE` voluntarily. The WM does so
only through reclaim, so the acknowledgement path and the reclaim path can
never both free the same slot.

Drain, once per tick: validate structurally, apply, publish `DONE` or `ERROR`
with the new `target` and generation, and stamp `terminal_at_ms` in the same
commit. The `REQUEST_DONE` event is appended by a later commit, never the same
one, so a client that trusts the event never reads a slot still marked
`PENDING`. The requester copies the whole slot out and only then releases it,
because `target` and `target_generation` live in the slot.

Reclaim rule. A slot is reclaimable when its status is `PENDING`, `DONE`, or
`ERROR` and either the owner is dead or the terminal deadline has passed:

```
dead owner    (requester_pid, requester_start_id) proven gone by a
              start-time-verified check
expired       status is terminal and
              now - terminal_at_ms > OMNI_REQUEST_RECLAIM_MS
```

`CLOCK_MONOTONIC` is system-wide, so `terminal_at_ms` is comparable across
processes. The deadline never applies to `PENDING`, so a busy WM cannot expire
work it has not drained. A re-armed slot is `PENDING` with `terminal_at_ms`
zero, so the deadline branch can never match it. A bare pid check is never used;
`kill(pid, 0)` alone is insufficient because a pid can be reused.

Because the deadline can free a slot under a live but slow requester, the
requester must confirm `(epoch, ticket)` still matches its own request before
trusting any field in the slot. A mismatch means the result was reclaimed and is
reported as `REQUEST_EXPIRED`, never read as a different request's answer.

Dimension and count limits, checked before any allocation: `width` and `height`
are u32 in the slot but must fit the descriptor's u16 fields, and
`stride * height` must satisfy `OMNI_REGION_FRAME_MAX_BYTES` using
subtraction-first arithmetic. A rejected request allocates no descriptor and
consumes no pool space, and reports `PARAM_INVALID`. `slot_count` 0 normalizes
to 1; a value above `OMNI_REGION_SLOT_COUNT_MAX` is `PARAM_INVALID`, never
silently clamped.

Error codes:

```
1 PARAM_INVALID   2 NAME_TOO_LONG    3 CATALOG_FULL
4 ARENA_FULL      5 REGION_FULL      6 REGION_TOO_LARGE
7 BLOCK_EXHAUSTED 8 BAD_TARGET
```

## 10. Region descriptors (fixed section 0x113000, 64 x 32 B)

A `CREATE_REGION` produces one catalog entry of type `region_ref` and one
descriptor. Descriptors are a fixed linear array at
`OMNI_REGION_DESC_OFF`; pixel payloads are allocated only in the pool's
`REGION_PAYLOAD` window, so descriptor bytes can never be overwritten by pixel
writes.

| off | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `payload_offset` | block offset of pixels, 16-aligned |
| 4 | 4 | `payload_bytes` | `stride * height * slot_count` |
| 8 | 4 | `stride` | bytes per row, including row padding |
| 12 | 2 | `width` | pixels, non-zero and at most 65535 |
| 14 | 2 | `height` | pixels, non-zero and at most 65535 |
| 16 | 1 | `format` | 0 ARGB8888, 1 RGBA8888, 2 XRGB8888 (all bpp 4) |
| 17 | 1 | `slot_count` | 1 or 2 in a published descriptor |
| 18 | 1 | `flags` | descriptor flags below; bits 4..7 reserved |
| 19 | 1 | `reserved` | zero |
| 20 | 4 | `region_revision` | publisher's revision within this descriptor generation |
| 24 | 8 | `region_generation` | allocation lifetime; changes on every allocation |

Descriptor flags:

```
bit 0 IN_USE            bit 2 PRODUCER_ACTIVE
bit 1 DESTROYED         bit 3 CONSUMER_ACTIVE
```

A descriptor is free when `IN_USE` is clear. A free descriptor may retain
`DESTROYED` from its previous lifetime. Allocation accepts only a free
descriptor whose `PRODUCER_ACTIVE` and `CONSUMER_ACTIVE` bits are clear. It
increments the retained `region_generation` (the first allocation uses `1`),
clears `DESTROYED` and both active bits, resets `region_revision` to `0`, fills
the geometry fields, and sets `IN_USE`. Generation wrap permanently retires the
descriptor; the block is the recovery boundary. The v1 allocator uses a linear
scan and keeps no descriptor freelist.

A region reference is a catalog value with `type_tag == 0x22`, `length == 8`,
`body_ref` equal to the descriptor index, and `value_inline` equal to the
descriptor's `region_generation`. It has no arena frame. A reference is valid
only when the catalog entry generation and the descriptor generation both still
match.

Validation on creation: reject a zero or oversized width/height, an unknown
format, `slot_count` outside `[1, 2]`, `stride < width * 4`, checked
`frame_bytes = stride * height` overflow, `frame_bytes` above
`OMNI_REGION_FRAME_MAX_BYTES`, or total `frame_bytes * slot_count` above
`OMNI_REGION_TOTAL_MAX_BYTES`. `slot_count == 0` is normalized to `1` while
filling the request, but a published descriptor contains only `1` or `2`.
Compute the payload allocation with checked multiplication and addition, and
reject a payload outside the current `REGION_PAYLOAD` window.

Publish/read semantics match `configstorage.md` §7: a generation starts at
revision `0`. A producer validates `(IN_USE, !DESTROYED, region_generation)`,
sets `PRODUCER_ACTIVE`, rechecks the descriptor, writes slot
`(r + 1) & 1` for two slots or slot 0 for one slot, and release-stores
`r + 1` only if the descriptor is still live. It clears `PRODUCER_ACTIVE`
afterwards. A consumer validates the descriptor, reads a non-zero
`(region_generation, region_revision)`, sets `CONSUMER_ACTIVE`, copies the
selected slot, and reads the pair again. If the generation, revision, or
liveness changed during the copy, it discards the sample and retries or skips
the frame; it never uploads an unstable pair. It clears `CONSUMER_ACTIVE`
afterwards. Revision `0` means unpublished, and revisions do not wrap within a
generation.

Destruction requires the request's `(epoch, entry_id, entry_generation)` and
`region_generation` to match. The WM marks the catalog entry destroyed and
marks the descriptor `DESTROYED` and not `IN_USE`. It appends a lifecycle event
carrying both catalog and descriptor identities. A trailing payload is returned
to `region_head` only when the destroyed descriptor is the highest payload
allocation and both active bits are clear. Otherwise the payload remains
reserved until block recreation. A later allocation may reuse the descriptor
only after both active bits are clear, and receives a new generation. Stale
active bits therefore cause a bounded leak, never unsafe reuse.

The active bits are protocol markers, not ownership or access control. A
producer or consumer that dies while active leaves its bit set; the safe
response is to skip reuse and reclamation, not to trust a PID or timeout.

## 11. Solved layout (fixed section 0x113800, 0x8000 bytes)

A readable projection of the most recent solve, so a script can ask where a
window is without reconstructing the program that placed it. It is overwritten,
never appended, and carries no `JOURNALLED` flag (§4).

```
off  size  field
0    8     generation      u64, bumped once per completed solve
8    4     node_count      u32, records actually written
12   4     reserved        zero
16   ...   nodes           node_count x 24 B, ascending node index
```

One node is 24 bytes:

```
off  size  field
0    4     entry_id        u32 catalog entry id
4    4     entry_generation u32, the entry's generation at solve time
8    4     x               i32, canvas coordinates
12   4     y               i32
16   4     width           i32
20   4     height          i32
```

`entry_id` plus `entry_generation` identifies the node, and the epoch is
implied: a solve only concerns the current epoch, so a node naming another epoch
is malformed rather than a dangling reference. The four extents are the whole of
the record. What a node was placed against is the address in the constraint
program, not
here, which is the difference between this section and a second copy of the
layout: geometry is a fact, relationships are configuration.

`generation` is bumped only after a complete node array is in place, so a reader
that samples it before and after a copy either sees one solve or retries. A
`node_count` below `OMNI_SOLVED_SLOT_COUNT_MAX` is normal and means the solve
placed fewer windows than the section can hold; a count above it is malformed,
because the array would run past `0x1DA800` and into the pool.

The whole occupied range is `16 + node_count * 24` bytes, so the section is
self-delimiting and the tail is stale bytes that belong to no node. A reader must
use `node_count` and never infer it from the section size.

## 12. Invariants (checked; the fuzz target asserts these)

The tier column names the guard that enforces each invariant
(`configstorage.md` §12): `L1` header, `L2` section, `L3` slot, `L4` value,
`R` replay. Every row maps to exactly one tier, so no invariant depends on a
compound condition.

```
tier  invariant
----  --------------------------------------------------------------------
L1    block_size is a multiple of 4096; header_size <= block_size <= MAX
L1    exactly one writer at a time (futex protocol, incl. machine writers)
L1    while futex == 1, writer_pid and writer_token identify the current holder
L1    commit_state is IDLE whenever no writer holds the futex
L1    readers never accept a snapshot taken while commit_state is not IDLE
L1    a BROKEN block is never mutated, and no recovery path reuses its epoch
L2    OMNI_POOL_OFF <= arena_end <= region_head <= pool_base + pool_size
L2    every offset/length dereferenced lies in [0, block_size)
L2    every section row has id == row, a 16-aligned offset, and a non-overlapping range
L2    fixed catalog, journal, request, descriptor, and solved ranges fit before OMNI_POOL_OFF
L2    solved node_count <= OMNI_SOLVED_SLOT_COUNT_MAX and 16 + count * 24 <= section size
L2    a solved layout read samples generation before and after, and retries on change
L2    journal count <= OMNI_JOURNAL_CAPACITY
L2    journal publish_seq is even outside a writer's metadata update
L2    a section extending past block_size is skipped, never clamped
L3    catalog freelist is an acyclic index chain, head/count consistent
L3    every arena allocation base and every region payload base is 16-aligned
L3    no live reader result depends on stale bytes of a recycled slot
L3    every journal entry has the current epoch, its commit_id, and a unique journal_seq
L3    every catalog and region reference includes the matching allocation generation
L3    descriptor reuse increments region_generation and clears prior active bits
L3    a consumer never uploads a region sample whose generation or revision changed
L3    trailing payload reclamation requires DESTROYED and both active bits clear
L3    a FREE or DESTROYED slot carries zero length, body_ref, and value_inline
L3    a request slot is FREE only because its requester released it or reclaim proved it reclaimable
L3    a terminal request slot carries a non-zero terminal_at_ms from the commit that set its status
L3    a PENDING request slot carries terminal_at_ms == 0 and is never deadline-reclaimed
L3    a request refused for readiness never consumed a slot or a ticket
L4    framed values and region payloads stay within their declared section windows
L4    a body_ref resolving to a frame whose next_free is non-zero is refused as free
L4    inline values have body_ref == 0 and a type-valid length
L4    extension tags are accepted only with a structurally valid framed payload
L4    an unassigned low tag (0x00, 0x35..0x7FFF) is refused, not read as an extension
L4    request names include NUL, respect the fixed limit, and target generations match
L4    CREATE_REGION width and height fit the descriptor u16 fields and slot_count is within range
L4    a requester validates (epoch, ticket) before trusting any field of a terminal slot
L4    a region reference carries a live descriptor index and matching generation
L4    no bounds check adds before comparing; every limit test subtracts first
R     a region lifecycle event carries catalog and descriptor identities
R     a published commit_id is never visible without its COMMIT_END entry
R     no consumer applies an entry whose commit_id lacks a retained COMMIT_END
R     no consumer applies a journal entry whose entry_generation no longer matches the live entry
```

Every invariant above is enforced by exactly one guard tier, and the tiers,
their failure actions, the tag decision, the flag states, the overflow-safe
subtraction rule, and the diagnostic dedupe are defined in
`configstorage.md` §12. No guard sentence joins two unrelated comparisons; the
audit's malformed predicate does not survive anywhere in this document.

## 13. Commit protocol, concrete

Field semantics:

- `header.futex` is 0 when free and 1 when held. It is acquired with an
  acquire CAS and released with a release store followed by a futex wake.
- `writer_pid` and `writer_token` are written after acquisition and cleared
  before release. `writer_token` is a per-acquisition random value, never a
  timestamp or a counter a crashed writer could have left in a reproducible
  state.
- `commit_state` is `IDLE` when the block is quiescent, `GROWING` while the
  futex holder is changing the mapping, `ACTIVE` while committed state is
  being mutated, and `BROKEN` once the block is unrecoverable.
- `active_commit_id` is the commit being published and is zero when idle.

Writer sequence:

1. Acquire the futex with `cas(0, 1, acquire)`. If `state` or `commit_state`
   is `BROKEN`, release and fail the request with `STORE_BROKEN`.
2. Store `writer_pid` and a fresh `writer_token`.
3. Validate structurally and reserve capacity without mutating committed
   state. If the pool must grow, release-store `commit_state = GROWING` before
   the resize. A failed attempt that changed nothing restores `IDLE` and fails
   the request normally; a partially applied resize marks the block `BROKEN`.
4. Store `active_commit_id = new_commit_id` and release-store
   `commit_state = ACTIVE`.
5. Mutate catalog, catalog name index, arena, region descriptors, request
   slots, and section metadata. **The name index is mutated in this step, in the
   same commit as the catalog entry it describes**, so the two are never
   observably inconsistent; there is no state in which an index lookup returns a
   slot whose catalog entry has not been written. From this point any failure or
   writer death marks the block `BROKEN`.
6. Write every journal slot for the commit, ending with `COMMIT_END`, then
   publish the ring metadata with the even `publish_seq`.
7. Release-store `commit_id = new_commit_id`, then release-store
   `commit_state = IDLE`.
8. Clear `writer_pid` and `writer_token`, release-store `futex = 0`, and wake
   waiters.

Reader sequence:

```
retry:
  s1 = acquire(commit_state)
  if s1 != IDLE: retry
  c1 = acquire(commit_id)
  snapshot whatever the caller needs
  c2 = acquire(commit_id)
  s2 = acquire(commit_state)
  if c1 != c2 or s2 != IDLE: retry
  accept snapshot
```

The release/acquire pairs order every payload, catalog, descriptor, request,
and journal write before the new `commit_id` and `IDLE` state. A reader that
observes `BROKEN` fails with `STORE_BROKEN` and does not inspect the block.

Recovery is fail-stop. A recovery authority reclaims a held futex only after
proving the recorded writer is dead with a pidfd or an equivalent
start-time-verified check; `kill(pid, 0)` alone is insufficient because a pid
may be reused. The authority marks the block `BROKEN`, refuses further access,
and the compositor recreates the block with a new `epoch`. No path attempts an
in-place rollback of a partially mutated block.

Worked example (single-key set): store `wm.focus.color` = u32 0xFF0000FF.
`commit_id` goes 42 -> 43 and the entry receives `journal_seq` 100. The
journal slot records `epoch`, `commit_id = 43`, `journal_seq = 100`,
`entry_id`, `entry_generation`, `type = 0x08`, and the inline value. The entry
body fits the existing frame, so it is overwritten in place. No arena growth
is required, so `commit_state` goes straight from `IDLE` to `ACTIVE`, and the
`COMMIT_END` entry with `event_ref = 1` is `journal_seq` 101.

## 14. Open items

- **A direct `get` has no result vocabulary.** `omni_layout.h` enumerates
  `OMNI_REQ_ERR_*` for the request queue, but `get` is a plain memory read and
  has nothing to return. §6.1 needs one, because an absent or malformed
  catalog index is now a refusal rather than a slow answer, and refusal needs a
  code. It should be a separate `OMNI_GET_*` set: a read has no slot, no ticket
  and no lifecycle, so reusing `OMNI_REQ_ERR_*` would imply machinery that does
  not exist. Deciding it is a prerequisite for implementing §3.1, and it also
  forces the other `get` failures to be named at the same time: name not found,
  free slot, entry generation mismatch, block broken, block truncated, and block
  unsupported.
- `OMNI_CATALOG_SLOT_COUNT = 16384` and `OMNI_ENTRY_NAME_MAX = 4095` are v1
  limits; changing either changes a fixed section size and every derived
  offset.
- `OMNI_CATALOG_INDEX_SLOT_COUNT` is tied to `OMNI_CATALOG_SLOT_COUNT` at one
  index slot per catalog slot, so it is a derived constant rather than an
  independent one; both are v1 limits and changing the catalog count changes
  §6.1's size and the offsets that follow it.
- Two-slot double buffering with sample revalidation is the v1 region
  contract; a lease or larger slot count may be added if sustained painter
  rates outrun the reader.
- A stale producer or consumer active bit intentionally leaks that descriptor
  or payload until block recreation; automatic recovery is deferred.
- Request slots permit at most 128 bytes of initial value; larger CREATE_ENTRY
  values must go through `set` after the empty entry exists.
- v1 has no external recovery tool: the compositor's own startup path is the
  only recovery authority, and it recreates rather than repairs.
- `include/shared/omni_layout.h` is the only home for every constant in §2, and
  it carries compile-time assertions for the invariants this document states.
