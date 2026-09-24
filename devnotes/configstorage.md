
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
- Single-write-at-a-time: one commit futex in the header serializes writers. Readers never lock: they use the generation counter. The compositor is single-threaded, so it never races with itself; the futex only arbitrates outside writers.
- Whole-block structural safety is the core WM's own discipline, not a contract imposed on third-party libraries (libraries decide their own level of defensiveness; the example library may ship safe-reader helpers).

## 1. Concurrency model

- One futex lives in the block header. A writer takes it, mutates state, writes journal entries, bumps the global generation, and releases. Uncontended acquisition is a single atomic instruction; only on contention does the kernel sleep the waiter.
- Generation counter: readers read it, take a snapshot of whatever they need, read it again, and retry on mismatch. A matching pair guarantees a coherent snapshot with no locks and no kernel involvement.
- Commit = the atomic unit of visibility. Two commit styles are both supported:
  - Single-key commits: one key change, one journal entry, one generation bump.
  - Grouped (multi-key) commits: several related keys changed in one commit, one generation bump, no intermediate state ever visible. Extensions setting related options together use grouped commits.
- Last writer wins for any given key, ordered by commit sequence number. No per-value CAS at v1.

## 2. Block layout

```
+--------------------------------------------------------------+
| header    : magic, format_version, block_size, commit futex, |
|           : generation (u64), epoch (instance id), pid,      |
|           : boot_time, section table, capabilities flags     |
+--------------------------------------------------------------+
| catalog   : fixed-capacity entry-header array + freelist     |
+--------------------------------------------------------------+
| value     : growable arena holding strings, blobs, payloads  |
| arena     : referenced by offsets, never by WM-owned point-  |
|           : ers (offsets re-verified on read)                |
+--------------------------------------------------------------+
| journal / : unified change + event stream (ring, drop-oldest)|
| event     :                                                   |
| ring      :                                                   |
+--------------------------------------------------------------+
| request   : 64 fixed slots for region/key lifecycle requests |
| queue     :                                                   |
+--------------------------------------------------------------+
| region    : fixed-capacity framebuffer slabs (stable offsets |
| slabs     : for in-place renderers), format + stride recorded|
+--------------------------------------------------------------+

The block is one contiguous mapping. It grows via mremap (see section 9);
the header always lives in the first page.
```

Section table entries identify each region by stable id, offset, and size. The WM owns the section table; readers treat it as data, never as a promise.

## 3. The catalog (open key-value store)

The catalog is the single structure that replaced the earlier idea of separate fixed "option slabs". Everything is a catalog entry: core options, bindings, rules, runtime state objects, framebuffer regions, extension keys.

Entry header layout (per entry):

```
name_ref   : u32   offset into value arena (no inline strings)
type_tag   : u16   one of the type tags in section 4
flags      : u16   mutable, destroyed, advisory access bits
length     : u32   payload length bytes
body_ref   : u32   offset into value arena or region slab id
```

- Any key name is accepted. No whitelist, no reject-at-write. Unknown keys are stored and served verbatim.
- Namespace by convention: `wm.*` is reserved for the core; extensions use their own prefixes (`ext.<name>.*`). This is convention, not enforcement: the block cannot identify writers, so there is no real access control. Advisory access flags may exist on entries but are not trusted.
- The WM seeds its core keys at startup as ordinary entries, exactly the way an extension creates theirs at runtime.
- Entries are created live. Fixed-capacity header array with a freelist; live entries never move, so holders can keep references stable.
- Structural rules for the WM read path: verify name_ref/length within block bounds, type_tag known, generation stable, alignment correct, before dereferencing anything. See section 12.

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
| 0x22  | region_ref  | u32 into region slab table (framebuffer)    |
| 0x23  | entry_ref   | u32 catalog entry (generic handle)          |
| 0x24  | enum        | string name of an enum constant             |
| 0x25  | option      | nullable value (no value flag + payload)    |
| 0x26  | array       | bounded: element type tag + count + body    |
| 0x27  | tuple       | bounded: fixed field tag sequence + body    |
| 0x28  | font_desc   | string (pango-style font description)       |
| 0x29  | shader_ref  | u32 handle to a shader source entry         |
| 0x2A  | cursor_ref  | u32 handle to a cursor/theme entry          |
| 0x2B  | binding     | {modmask, keysym/keycode, action_ref}       |
| 0x2C  | rule        | generic match-replace rule record           |
| 0x2D  | gradient     | array of {offset, rgba8/rgba32f} stops      |
| 0x2E  | path        | string (filesystem path)                    |
| 0x2F  | exec        | string (shell command line)                 |
| 0x30  | state       | string (free-form state token)              |
| ...   | reserved    | extension range starts at 0x8000            |

For string and blob values the entry stores length + body_ref; multi-entry payloads are not supported at v1 (a blob is bounded to the entry's declared length, set at creation).

## 5. Journal / event stream (single unified ring)

There is one ring, not two. It carries:

- `KEY_SET` entries: `{epoch, commit seq, time, key_ref, type, value_ref}`. Small values (<= 8 bytes) inline; strings/blobs via arena ref. This is the generic "key X now has value Y" channel.
- `KEY_DELETE` entries.
- Free-form event entries (`EVENT`): `{epoch, seq, time, category, event_ref}`, covering REGION_CREATED, REGION_DESTROYED, REQUEST_DONE, custom extension events, and coarse texture-state announcements.
- Capacity is a compile-time constant at v1, enforced as a ring with drop-oldest. Older entries are overwritten before newer ones are ever dropped.
- `watch <key>` reads the ring forward. `get <key>` reads the current catalog value instead.
- Pixel churn is never journaled. Framebuffer regions use their own per-region seq (section 7).
- WM restart: each entry carries `epoch` (a per-instance id). A consumer whose remembered cursor belongs to a different epoch discards it and resynchronizes by reading a full generation-stable catalog snapshot, then follows the ring forward from where it left off.

## 6. Commit & guard rules

Commit protocol (identical for WM core and external writers):

1. Take the commit futex.
2. Mutate entry payloads / catalog as needed.
3. Append one or more journal entries (grouped commits append their entries together).
4. Bump the global generation.
5. Release the futex.

Writer-side checks are structural convenience only (e.g., request-time sanity on declared sizes), never enforcement: the requester is not trusted and the WM re-validates structure on every read. Semantic checks never happen at write time.

## 7. Framebuffer regions

A region is a catalog entry of type `region_ref`; its payload lives in a fixed-capacity region slab with recorded format, stride (= width x bytes per pixel), `slot_count`, and `seq`.

- Painter side: write pixels, then publish by storing `seq = seq + 1` as the final aligned write. One painter per region by convention; the WM never arbitrates pixel writers. When `slot_count == 2`, the painter alternates slots; the parity of seq picks the slot.
- WM reader side, once per frame: read `seq`; if it differs from the cached uploaded seq, derive the slot (`seq & 1` when slot_count == 2, else slot 0), read within recorded bounds, upload via glTexSubImage2D, cache seq. Identical code for 1 or 2 slots; slot_count only affects slot derivation.
- The WM keeps a local applied copy of every uploaded texture. Applied surfaces reference that copy, never the SHM region directly. Destroying a source region therefore cannot crash or tear down anything; the applied copy lives until overwritten by new data. This is the destroy-safe guarantee.
- Tradeoff accepted at v1: tearing is possible if a painter is mid-write when the WM samples. It self-heals on the next paint. `slot_count == 2` with painter-side flip removes it for painters that opt in; the WM does not need to know which.

## 8. Region/key lifecycle and the request queue

Requests are how socket-free programs ask the WM to create or destroy catalog entries and regions.

- 64 fixed request slots. Each slot: `{status: FREE/PENDING/DONE, ticket, pid, type, params, response}`. Type is CREATE_ENTRY, CREATE_REGION, DESTROY_ENTRY, DESTROY_REGION. Params carry name, format, width, height, slot_count; response carries the new entry/region id or an error.
- Append under the commit futex: take it, find a FREE slot, write params and a monotonically increasing ticket, set PENDING, release. Requests are rare, so contention is negligible. Tickets give arrival order so the WM processes deterministically.
- WM drain, once per tick: validate params structurally, carve slab / create entry, append a discovery-bearing journal entry, set DONE, and append a REQUEST_DONE event with the ticket.
- Requester reads its slot status or the event ring, then flips the slot back to FREE after consuming the response (under the futex).
- Reclaim: each request records the requester pid. The WM lazily frees PENDING/DONE slots whose pid is gone (`kill(pid, 0)`), so a crashed producer leaks a bounded handful of slots at most.
- DESTROY marks the entry destroyed and returns its slab to the freelist; the slab may be reused. Painters and readers detect destruction via the entry's destroyed flag / catalog lookup failure (new or cached). Applied copies inside the WM survive regardless.

## 9. Growth and stale-mapping detection

- Growing the block uses mremap. After growth the WM updates `block_size`, writes any new section table entries, then bumps generation. The header is always in the first page.
- Mappers: on any generation change, re-read the header; if `block_size` grew, re-map to the new size and re-read the section table / catalog. The client library does this transparently.
- The header carries `pid` and `boot_time`, so a mapper can detect a dead or restarted compositor (compare against the known instance) rather than trusting a stale file. On startup the WM unlinks and recreates the block, ignoring any orphaned mapping.

## 10. Discovery / interop

Decided defaults:

- The compositor sets the environment variable `OMNI_INSTANCE_SIGNATURE` to the absolute path of the SHM file.
- The optional socket facade lives beside it at `<block-path>.sock` inside the same runtime directory.
- Fallback discovery: scan `$XDG_RUNTIME_DIR` for entries matching `omniwm-*.shm` / `omniwm-*.shm.sock`.
- Permissions 0600, same-user only. No authentication inside the block; defense is file permissions plus consumption-side validation.

## 11. Socket facade (optional convenience)

The socket is a thin JSON facade over the exact same primitives, for people who do not want to map the block. A well-written library never needs it.

- `get <key>`: read current value from the catalog.
- `set <key> <typed value>`: commit protocol, single or grouped.
- `watch <key|glob>`: read the journal forward.
- `exec <action> [args]`: invoke a registered action by name (bindings reference actions by string, resolved via the registry at dispatch time).
- `save [path]`, `reload`, `reset <key>`.

No key-specific handling anywhere: unknown keys are served verbatim, matching the open-catalog rule. The socket implementation is a client of the block, not a peer of it.

The built-in TOML parser is a facade in the same sense, running in-process against the block.

## 12. Guard rules, precisely

Two tiers, both applied at consumption:

- Structural (mandatory, on every dereference): refs within current block size, lengths within both declared capacity and block bounds, known type tags, committed-stable generation, correct alignment. Violation means refuse the entry, log once, skip-and-continue. This is memory safety, not policing.
- Semantic (the consumer of the value decides): numeric ranges, enum name recognition, action-name registration, sanity relative to the key's meaning. Invalid semantic data is refused and skipped, never fatal.

Every read treats the whole block as hostile input, because every mapping process can write every byte of it; page-level write protection cannot work inside one RW mapping.

Free win: because bad data is a designed-for state, hardening is fuzzable. Garbage the block (mutate random bytes, truncate, clobber lengths/refs), run the WM, assert it never crashes and only skips-and-continues. This is a first-class test target.

## 13. Startup, config-as-saved-state, and save-file semantics

- Startup: create block, seed core `wm.*` keys, set `OMNI_INSTANCE_SIGNATURE`, serve.
- A config file is a sequence of `set`/`exec` replayed into the block.
- Partial-invalid config: the whole file still loads. Line-level failures are logged and that line is not applied, but never abort the file. Whatever reached the block stays; consumers refuse invalid keys when they parse them. The config fails per line, never as a whole.
- Save: all currently set non-window-specific keys are written out (a config file replayed from the server's own `save` must reproduce configuration, not geometry). Window-dependent state (per-client geometry, focus, per-window overrides) is excluded.
- Export policy for bad data: entries that fail structural checks are skipped with a warning; structurally valid but semantically unknown extension keys are preserved as-is so their owning extensions can re-import them on the next boot.

## 14. Deferred, flagged for later

- Exact field-level struct definitions and byte offsets (next drafting pass).
- Catalog and arena capacity budgets / defaults at v1.
- Whether journal ring capacity should become configurable (constant at v1).
- Multi-painter arbitration on a single region (out of scope: one painter per region).
- Value transactions beyond grouped commits (no CAS at v1).
- Mixed-endianness hosts (native, tied to format_version).
- Per-entry value size caps for requests, to bound abusive slabs.

