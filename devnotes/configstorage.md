
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
| catindex  : 16-byte slots sorted by name hash, binary search |
+--------------------------------------------------------------+
| journal   : fixed ring metadata + fixed 64-byte entry slots   |
+--------------------------------------------------------------+
| requests  : fixed 256-slot request queue, names in the pool   |
+--------------------------------------------------------------+
| region    : fixed descriptor table outside the growable pool   |
| desc      :                                                  |
+--------------------------------------------------------------+
| solved    : fixed single-buffered geometry, never journalled   |
+--------------------------------------------------------------+
| pool      : growable arena, then region payload tail          |
+--------------------------------------------------------------+
```

This is an overview rather than the authority: the sizes and offsets are in
`configstorelayout.md` §2 and the `OMNI_SECTION_*` ids in `omni_layout.h`
section 2. There are eight of them. The first seven are ids 1 to 7 in `omni_layout.h`
section 2, which is catalog, arena, journal, requests, region desc, region
payload and solved, the last being `OMNI_SECTION_SOLVED_LAYOUT` at `0x113800`.
`catindex` is the eighth, `OMNI_SECTION_CATALOG_INDEX` id 8 at `0x81000`, added
by §3.1 below, and it sits between catalog and journal in address order even
though its id is last. An earlier draft of this diagram omitted both `solved`
and `catindex`.

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

### 3.1 The read path, which is where the storage model actually costs something

This subsection exists because the storage model is not Mango's and the read
path is where that difference is paid. Mango's config is a `Config` struct
filled once at parse time, so reading `config.gaps` is a load at a known offset
out of a struct the parser already built. Here, a name is a string in the arena
and a value is a typed frame reached through a catalog slot, so the equivalent
read is: hash the name, find the slot, validate the slot, follow a frame
reference, decode a tagged value.

Done naively that is a walk of up to `OMNI_CATALOG_SLOT_COUNT` entries with a
string comparison each, against Mango's one load. Three things fix it, and none
of them is "make the struct access faster", because there is no struct.

**1. `get` binary searches an in-block index; it never walks the catalog.**
`configstorelayout.md` §6.1 defines a second, sorted view of the catalog: 16,384
slots of `{u64 name_hash, u32 entry_id, u32 generation_lo}`, sorted ascending,
looked up by binary search and then a walk of the equal-hash run to confirm the
name. The hash is FNV-1a 64 with a splitmix64 finalizer, and the finalizer is
load-bearing rather than decorative: the array is sorted by the full 64-bit
value, so a binary search discriminates on the high bits first, and FNV-1a's
high bits mix poorly for inputs as short as a key path.

Two properties matter more than the asymptotics:

- **The writer maintains it transactionally**, in the same commit as the catalog
  mutation, under the futex. So it is never stale, never rebuilt, and never
  disagrees with the catalog, because there is no window in which it could.
- **There is no walk behind it, by design.** A missing or malformed
  `CATALOG_INDEX` row, or a version this build does not know, makes the block
  unreadable to this build; it does not make `get` slow. The reason is the one
  that makes the whole design worth its 256KB: a fallback scan makes `get` cost
  proportional to the number of configured keys, which is fine on a test machine
  with 40 keys and not fine on a real session with 4,000, and a cost that
  depends on unrelated configuration is exactly the cost nobody profiles. A
  wrong answer is worse than an error, and an error is worse than a clear
  refusal.

The catalog itself is deliberately not sorted. `entry_id` is the slot index,
live entries never move, and the freelist chains through slots, so sorting in
place would move live entries, invalidate every held
`(entry_id, entry_generation)`, and make delete an O(n) move of the catalog
rather than of a derived index.

**2. Hot reads use private-copy indexes, rebuilt once per change.** A process
that reads the same keys every frame keeps its own copy of what it needs, and
refreshes it when the store says something changed rather than re-resolving per
frame:

```
resolve once:   name -> hash -> index -> (entry_id, entry_generation)
per frame:      if commit_id unchanged, use the private copy
on a change:    invalidate and re-resolve
```

The steady-state cost of forty options read every frame is forty `u64` compares
and zero hash lookups, so the per-frame cost stops scaling with the number of
options. This is the same pattern the binding index uses
(`generaldesign.md` §14.4) and the same one watchers use, which is what makes it
a project-wide convention rather than a special case.

**3. Change notification is a first-class event, and bindings get their own.**
An ordinary cached value is invalidated by `KEY_SET`, which already carries
`(epoch, commit_id, journal_seq, time_ns, entry_id, entry_generation, type,
value)`; a reader matching on `entry_id` needs nothing more. A *derived*
structure needs something different, because it is not invalidated by any single
entry, it is invalidated by the set changing. So the journal has
`OMNI_JOURNAL_KIND_BINDS_UPDATED` (`5`), appended in the same commit as the
`KEY_SET` entries it summarises, and any process holding a binding index
rebuilds on it. It exists as a kind rather than as a run of `KEY_SET` entries
because one rebind and a two-hundred-binding config reload should be one event
to a client that only needs to know its index is stale.

**No per-entry revision field.** The obvious alternative is a `revision` u32 in
each catalog entry so a reader can poll one entry instead of the journal. The
journal already carries the entry identity per entry and appends nothing a
reader cares about when nothing changed, so it is the cheaper mechanism, it is
already specified, and the entry stays 32 bytes.

**Mutable per-frame state does not belong in the journalled catalog.** Every
visible commit appends at least one journal entry plus a `COMMIT_END`, so a
catalog entry that changes once per frame consumes a journal slot per frame,
evicts configuration history, and makes the §5.1 gap check fire continuously.
The block already has the right two answers and this subsection only makes them
mandatory:

- `solved` is a fixed single-buffered section that is never journalled, and it
  is where layout results belong;
- the region descriptor table is outside the growable pool, carries a
  `region_revision`, and publishes with a seqlock, which is the pattern for a
  value that changes often and whose readers need a stable sample.

So the rule is: **the catalog holds configuration and state whose *changes* a
client needs to observe. Everything a client would merely poll belongs in a
fixed, non-journalled section read by offset.** Pointer position, scroll deltas
and focus bookkeeping are the obvious cases.

`EPHEMERAL` does **not** express this. `EPHEMERAL` is the `save` test, meaning
"no long-term relevance, so do not write this to a config file". An ephemeral
entry is still journalled, still costs a slot per change, and still evicts
history. Conflating "not worth saving" with "not worth notifying about" is the
trap here, and the two sets are genuinely different. A tag's membership is not
worth saving to a file and is worth a journal slot, because a client watches it.
The pointer position is not worth saving *and* is not worth a journal slot,
because nothing watches it and everyone polls it.

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
| 0x32  | constraint  | a nestable layout program, see below |
| 0x33  | client_rule | one client rule, a 24 B record, see below |
| 0x34  | map         | a keyed block of typed values, see below |
| 0x35..0x7FFF | reserved | the unassigned low hole, see below       |
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

Four payloads are shaped rather than scalar, so their fields are named here and
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
12   4     action_ref   u32 frame offset of the action's name, never a handle
16   4     args_ref     u32 arena frame offset, or OMNI_REF_NONE
20   4     flags        u32, bit 0 USE_KEYSYM
24   ...   args         array value of strings, inline
```

The header is fixed and the array is variable, which is why this is a framing
change and not a new tag: a binding without arguments is still a binding, and
`args_ref == OMNI_REF_NONE` says so without a length comparison. The argument
array is positional and every element is a string, matching what `exec` already
accepts, so `wm.cycle_layout master_stack` and a TOML
`args = ["master_stack"]` are the same value in two syntaxes. Argument count and types
are the action's business, not the store's.

`constraint` was a packed array of 16-byte records with no header, and it cannot
express `layoutlanguage.md` §3, because a program's spaces form a tree: a list of
spaces where a space may name a nested `layout`, up to
`OMNI_LAYOUT_MAX_NEST_DEPTH`. A flat record array has no place to put a child
program. So the tag still marks the domain, and the payload became the tree
itself, composed of tags that already exist. It is the tag of
`omniwm.layouts.<name>.spaces`; the sibling keys `.rules` and `.viewport` are an
array of `option` and an `option` respectively (`layoutlanguage.md` §1):

```
constraint := array of space
space      := option          (framed: name string, then the value)
value      := the space's own arguments, in `layoutlanguage.md` §11.2 and §11.7
            | constraint       (a nested `layout`, absent unless the space is a group)
```

`kind` became the `rule` key of the language and the record's `flags`, `edges`, `axis`
and `role` went with it, because they described one fixed-width record where the
language has one argument per rule kind and a rule states only the arguments its own
kind takes. `priority` is gone because the language has no priority: `layoutlanguage.md`
§3 says order in the list is the order, and a linear weight was a solver artefact from
the weighted formulation `share` and `inset` still use. `role`, and with it
`ROLE_PARENT`, went because `operand` did; there is no rule that measures a parent
rectangle any more. Order is the array's order and needs no field to say so.

A program is therefore a value like any other, which is what
`layoutengine.md` §4.6 asks for: `save` writes the tree and a user can edit it,
where a record array was a positional tuple in a file nobody reads. The tag is worth
keeping because a core reader can recognise a layout program without a schema, and
`0x32` costs one row in three tables.

`client_rule` is one rule of one client rule set, 24 bytes with no payload, and its
fields are the keys of `layoutlanguage.md` §11.11:

```
off  size  field
0    2     rule      u16, ALIGN | MATCH | SIZE | OFFSET | SNAP
2    2     against   u16, CLIENT | VIEWPORT | OUTPUT
4    2     axis      u16, X | Y, MATCH only
6    2     edge      u16, LEFT | RIGHT | TOP | BOTTOM | CENTER_X | CENTER_Y, ALIGN only
8    2     region    u16, the ten regions of §3.7, SNAP only
10   2     reserved  zero
12   4     width     u32, SIZE only, pixels
16   4     height    u32, SIZE only, pixels
20   4     by        i32, OFFSET only, pixels, may be negative
```

The record lost five fields it used to have, and each loss is a decision the language
made rather than a simplification. `match_appid`, `match_title` and the `TITLE_REGEX`
flag are gone **from this record**, because this record is a solve-time constraint
rather than a selector. A `clients` set is reached by name, not by a property
comparison against a surface, and a `snaps` set is reached by a drag region or a
keybind that names it. `rule = "match"` is a geometry relation that takes the
reference's extent on an axis, and it is a different thing from matching a window's
title, which is the confusion that had `match` carrying a matcher. `target_id` and
`target_gen` are gone for the same reason: a rule names no target, it is reached by
name. `scope`, and with it `MAP_ONLY` and `ALWAYS`, is gone because a set is resolved
when it is reached and the language states no re-resolve timing; the reload case is
the store replaying the config, not a second timing for a rule.

What reaches a set is a separate concern from what a record inside one contains, and
`windows.md` §9 settles the first without reopening the second. A fake client is
bound by the creating action naming the set, and a real client is bound by a window
rule matching it, but in both cases the set is named and what lands on the client is a
`0x33` record. The matcher therefore has no reason to return to this record: it lives
in a block-structured if/then at `omniwm.window_rules.<name>`, and its `then` side
binds sets by name.

`map` is the other way: a value that is a keyed block of typed values, which the
store did not have. Section 3 is an open key-value catalog, so a TOML table flattens
into a key path rather than becoming a value, and every "block" in the design so far
has been a key path. A window rule is the first thing that needs a block to *be* a
value, and a map is the shape it needs:

```
map := array of entry
entry := tuple          (framed: the key string, then the value)
```

It is an array of pairs and not a hash table, which is the same choice `binding`
makes in being a positional array rather than a structure with named offsets. The
order of the entries is the order they were written, it is not semantically
meaningful, and a reader that cares about a key finds it by comparison. Keys are
UTF-8 strings, unique within one map, and the same immutability rule as a catalog
entry's own name applies to them: a rename is a delete and a create.

**A window rule is a `map` with two well-known keys, `if` and `then`, and it gets no
tag of its own.** That is the same argument this section already made when it dropped
the `effect` enum from `client_rule`: the namespace carries the meaning, so
`omniwm.window_rules.<name>` already says what the value is, and a tag repeating it is
a second place for the two to disagree. Every value in an `if` map is a `string`
holding a pattern, and every value in a `then` map is whatever type that field is
(`windows.md` §9.3). The cost of no dedicated tag is that nothing structural stops
`if` from holding a non-string or `then` from holding a read-only field, so a rule's
validation is semantic rather than framing-level, and it lands in the bucket §14
already defers. A dedicated tag would make the core recognise a rule without a
schema, at the price of a tag per block-shaped type for as long as that holds.

This is a design-stage decision and cheap to revisit, which is the reason to make it
now rather than defer it: the compositor does not exist, so there is no
compatibility cost to changing the tag or promoting the rule to one of its own before
any code is written, and §14's list is where that would be recorded if it happens.

`effect` is gone too, and the namespace carries it instead: a set under
`omniwm.clients` arranges a surface and a set under `omniwm.snaps` names a
destination, so which of the two a rule belongs to is what its effect is, and an enum
repeating it would let the two disagree. `by` is signed while `layoutlanguage.md`
§11.6's `by` is not, because an `offset` may be negative and an `inset` may not; that
is the one place where the program layer's argument and the client layer's argument
share a name and differ in range, and the record is where it is visible.

`client_rule` is still not `rule` with a flag set. `rule` matches a key and replaces
a value; these match nothing and solve a set of soft equations, so it remains a
different record rather than a variant of one.

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

- 256 fixed request slots of 256 bytes. The name is an arena frame referenced by
  `name_ref` rather than an inline field, which is what makes the slot 256 bytes
  instead of `0x1100` and the queue 64KB instead of 1.1MB. The frame belongs to
  the slot from submission until release or reclaim and is freed to the §5 arena
  free list then, so it is readable for the whole drain and reclaimed exactly
  once. The requester copies the name out of the arena before releasing the
  slot.
- The slot count is sized for the queue's real load rather than the region count. The queue carries CREATE_ENTRY and CREATE_REGION alike while the catalog holds 16,384 entries, and nothing in the protocol bounds a client's in-flight requests, so a batching client that submits a 200-key theme is legal and would exhaust a 64-slot queue. Each slot contains `{epoch, status: FREE/PENDING/DONE/ERROR, ticket, requester_pid, requester_start_id, type, target, target_generation, name_ref, params, response, terminal_at_ms}`; `params`, `response` and `name_ref` are u32 arena frame offsets, so no field is large enough to set the slot size, and a name is bounded by the frame allocator rather than by the slot.
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
L2  section   failure: drop that section; refusing the block or not is
               decided by PRESENT and by commit_state, not by the check
L3  slot      failure: skip that slot, continue with the others
L4  value     failure: skip that entry, continue with the others
```

All four tiers are structural. Every check in them answers one question: can
these bytes be read as what they claim to be? None of them asks whether the value
is *sensible*, because that is not a question about bytes and the answer belongs
to whoever uses the value.

- **L1 header.** `magic`, `format_version`, `header_size`, section constants,
  `state` in {CREATING, READY}, and `commit_state` readable. A CREATING block
  is `OMNI_ERR_NOT_READY`; a BROKEN block is `OMNI_ERR_STORE_BROKEN`. Both are
  refusals, not corruption, and neither is logged per entry because there is no
  consumer state to log into yet.
- **L2 section.** Each section row has `id == row`, a 16-aligned `offset`, a
  `size` that does not wrap, and a range contained in `[0, block_size)`. A
  section outside the mapping is dropped, never clamped to it. What dropping a
  section means is read off two facts the block already carries, so the tier does
  not need a required/optional list of its own:
  - `flags & PRESENT` clear means the section is **absent by design**. The reader
    skips it, reports nothing, and continues; there is no failure here at all,
    because a section nobody promised cannot be a broken promise.
  - `PRESENT` set means the block promised that section, and a promised section
    whose framing is invalid, or whose version word this build does not
    implement, is `OMNI_ERR_BLOCK_UNSUPPORTED` and refuses the block. Creation
    sets `PRESENT` on every section (`configstorelayout.md` §4, `id == row`,
    eight rows), so a PRESENT section that cannot be read is a block breaking its
    own stated shape, and there is nothing to degrade to.
  - The one PRESENT section a reader may legitimately drop is one whose range
    extends past `block_size` **while a growth commit is in flight**, which is
    `OMNI_ERR_BLOCK_TRUNCATED` and is a temporary skip, §12.5. The commit state
    is what tells the two apart, and that is why a short block at rest refuses
    while a short block mid-growth does not.
- **L3 slot.** Slot index inside the section's declared capacity, the slot's
  identity fields non-zero where required, generation matching for a live
  reference, and 16-byte alignment before any dereference.
- **L4 value.** The tag decision, flag state, and length rules below.

The failure action names the scale of the failure, which is what lets a reader
decide whether to keep reading. A failure at L1 is about the block and is
`OMNI_ERR_NOT_READY`, `OMNI_ERR_STORE_BROKEN` or `OMNI_ERR_BLOCK_UNSUPPORTED`
according to which L1 check it was. At L2 the scale is decided by PRESENT as
above: a promised section that cannot be read is `OMNI_ERR_BLOCK_UNSUPPORTED`
and refuses, because a build that cannot find its own catalog index or journal
has nothing to offer a client. A failure at L3 or L4 is about one reference
inside a block this build understands, so it is `OMNI_ERR_VALUE_UNREADABLE`, or
`OMNI_ERR_ENTRY_FREE` / `OMNI_ERR_GENERATION_MISMATCH` when a held reference is
what led there. A reader that hits the second kind still has a usable block and
should keep using it; one that hits the first does not, and collapsing the two
would make a client rediscover a whole instance over one stale key.

Replay is a fifth tier, applied at its own site rather than in the read path:
apply only entries whose `commit_id` has its `COMMIT_END` in the ring, only
entries whose `entry_generation` matches the live catalog entry, and only after
the §5.1 gap check. Failure is skip-and-continue or a policy-driven resync, never
a partial application.

Semantic validation is not a tier here, and its absence is the decision rather
than an omission. Numeric ranges, enum name recognition, action-name
registration, and sanity relative to a key's meaning are all checks on meaning,
and the store has no standing to make them: it holds a value of whatever tag names
it under the open-catalog rule, so a `u8` of 200 is a perfectly well-formed value
of an unknown key and a `string` of `"purple"` is a well-formed value of a colour
key whose consumer is entitled to its own opinion. Putting these checks in the
store would also put them in the wrong place to be useful, because the store
answers one question per read while meaning is per consumer and often per use.

So the boundary is this: the store guarantees a reader can *decode* a value or
gets a named failure saying it cannot, and every consumer independently decides
whether to *accept* one. A consumer that rejects a value does not corrupt anything
by refusing it, because nothing in the block asserted that the value was
acceptable in the first place. This is also why `OMNI_SOCK_ERR_BAD_VALUE` and
`OMNI_SOCK_ERR_BAD_TYPE` are socket-only codes (`ipc.md` §2): they report what a
client sent, never what a block contains, so a direct reader can neither produce
nor need them.

### 12.1 Tag decision

The tag test is a three-way branch, not "is this tag known". This is what lets
the core carry extension values it cannot interpret while still refusing its own
malformed values.

```
0x01..0x34        known:      require the tag's exact encoded length and its
                              representation rule (inline or framed)
0x8000..0xFFFF     extension:  require VALUE_FRAMED, a verified frame, and a
                              length the core never interprets
0x00, 0x35..0x7FFF unassigned: malformed, refused by the core
```

`0x00` is unassigned in the catalog and is used only by journal entries that
carry no value. An unassigned low tag is a hole in the core's own table, not an
extension, so it is refused. Save preserves entries on the extension branch and
skips entries that fail any structural tier.

The upper end of the known range is a moving target: `0x32` and `0x33` were added
for the layout engine and `0x34` for the window-rule block, and the hole simply
moved from `0x32..0x7FFF` to `0x35..0x7FFF`. `OMNI_TAG_MAX_KNOWN` and
`OMNI_TAG_UNASSIGNED_LO` are asserted to be adjacent in `omni_layout.h` so the two
cannot drift apart, which is the only thing that makes a hole detectable at all.

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
         declared arena window and block_size, and the frame is allocated
region   length == 8, body_ref < OMNI_REGION_DESC_COUNT, generation non-zero
```

An over-length inline value is malformed. A framed value whose declared length
disagrees with its frame header is malformed. Neither is repaired.

The "allocated" test in the `framed` line is the free list's discriminator, and
it is the one thing that makes a recycled frame safe to hand out. A free frame
stores its successor at `OMNI_FRAME_OFF_NEXT_FREE`, which is the second word of
its 16-byte frame header, ahead of the payload that starts at
`OMNI_FRAME_OFF_PAYLOAD` (`configstorelayout.md` §5), and a live frame stores
zero there, so
the two cannot be confused: `next_free == 0` is allocated, and any non-zero
`next_free` is on the free list. The free-list head itself
(`OMNI_HDR_OFF_ARENA_FREE_HEAD`) and a free list's tail both use
`OMNI_REF_NONE`, so the end of a list is not a usable offset either. A
`body_ref` that resolves to a free frame is therefore a guard failure and
nothing else: `OMNI_ERR_VALUE_UNREADABLE`, or
`OMNI_ERR_GENERATION_MISMATCH` when a held reference is what led here. It is never
a read of recycled bytes as a value. The check is two loads and a compare, and it
must be applied on every `body_ref` dereference, not only on the path that wrote
it, because the free list is what makes offsets reusable in the first place.

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
past `block_size` is dropped, never clamped to it, and whether that is a
temporary skip or a refusal is the L2 rule above: a drop during a growth commit
is a temporary skip, and the reader can come back to the section after the
commit without having lost anything, while a drop of a PRESENT section on a
block at rest is a block that broke its stated shape. Truncating a live block
therefore either degrades the reader to fewer sections for the duration of one
commit or refuses it outright; it never produces a header claiming more than the
mapping holds.

The two degradation paths are different codes and stay different codes.
`BLOCK_TRUNCATED` is a size fact: the block is a prefix of the layout, so the
section is genuinely not written yet, and the reader can come back to it after a
growth commit without having lost anything. `BLOCK_UNSUPPORTED` is a shape fact:
the bytes are there and the build cannot interpret them, either a version it does
not implement or a mandatory section that is absent or malformed. A promised
section lands in `BLOCK_UNSUPPORTED` rather than in the skip-and-continue path
above precisely because there is no honest degradation available: the catalog
index in particular has no fallback, since scanning the catalog is the O(n) cost it
exists to remove, and a reader that degraded to a scan would turn a configuration
file's key count into every socket client's latency (`configstorelayout.md` §6.1).

### 12.6 Diagnostic policy

One policy, one counter. A per-consumer dedupe keyed by
`(tier, section, slot index)` logs a given corrupt slot once instead of once
per frame, while a different corrupt slot is still reported. The key is reset
when the slot's `entry_generation` changes, so a slot that becomes valid and
corrupt again is reported again. Diagnostics never change control flow: a
logged entry and a silently skipped entry are the same outcome.

Every read treats the whole block as hostile input, because every mapping process can write every byte of it; page-level write protection cannot work inside one RW mapping.

Free win: because bad data is a designed-for state, hardening is fuzzable. Garbage the block (mutate random bytes, truncate, clobber lengths/refs), run the WM, assert it never crashes and only skips-and-continues. This is a first-class test target. Because each invariant in `configstorelayout.md` §12 names exactly one tier, the corpus is organised per tier: a mutation that should trip L3 is asserted to skip one slot and leave every other slot readable, and a mutation that should trip L1 is asserted to refuse the block rather than degrade any single entry.

## 13. Startup, config-as-saved-state, and save-file semantics

- Startup: create block, initialise the header, seed core `wm.*` keys, set
  `OMNI_INSTANCE_SIGNATURE`, serve. The header initial values are not left to
  "whatever the allocator handed back", because a field whose initial value is
  implied rather than written is a field whose meaning differs between the creator
  and a reader that never checked; §13.1 is the table and it is derived from
  `configstorelayout.md` §3 and the recovery rules in §10.

### 13.1 Block creation, field by field

A creator has exactly two jobs that can be got wrong: the block must be
*unambiguously* a fresh block, and it must be *incomplete* in a way readers
already know how to refuse. Both come from the same decision, which is that a
block in this state is not an error condition and does not need a separate
"creating" flag beyond the one the header already has.

The sequence is: `ftruncate` the file to `OMNI_BLOCK_INITIAL_SIZE`, `mmap` it
`PROT_READ|PROT_WRITE` with `MAP_SHARED`, and **zero the whole mapping** before
writing any field. Zeroing first is what makes the whole table below correct at
once: a reserved field is zero because nothing wrote it, a pointer is `NULL`
because nothing wrote it, and a sentinel field that has to be non-zero for an
empty structure is the only kind of field needing an explicit write. It also
removes a class of bug where a field is correct in a fresh block and garbage in
a recycled file, which is otherwise indistinguishable until it matters.

| field | initial value | why that value |
|---|---|---|
| `magic` | `OMNI_MAGIC` | written first, before anything else is meaningful |
| `format_version` | `OMNI_FORMAT_VERSION` | 1; the field exists so a later build can refuse this one |
| `block_size` | `OMNI_BLOCK_INITIAL_SIZE` | matches the `ftruncate`, so §12.5's containment test passes |
| `header_size` | `OMNI_HEADER_SIZE` | ditto, for the header row itself |
| `futex` | zero | an unlocked futex is 0; the creator never holds it here |
| `state` | `OMNI_STATE_CREATING` | **not** READY. A reader that mapped the file before this line ran sees CREATING and gets `NOT_READY`, which is a refusal it already knows how to make |
| `commit_id` | 1 | the seeding commit is the first published state, so the first `commit_id` a reader can see is 1 and never 0 |
| `epoch` | fresh, never reused | one per compositor instance; zero is not a legal epoch because a zeroed block must never look like a live one |
| `boot_time_ns` | `CLOCK_MONOTONIC` at creation | a witness of when the creator started, so a reader can tell a block it is looking at was written by this compositor instance or by a previous one. Both sides read the same clock, so the comparison is a subtraction and not a conversion, and `CLOCK_MONOTONIC` is the clock the rest of the design already uses for a stored timestamp (`configstorelayout.md` §9's `terminal_at_ms`, `ipc.md` §4's entry `time`), so this field is not the one exception that has to remember which of two clocks it meant |
| `capabilities` | `OMNI_CAP_DEFAULT` | all nine bits; the sections are all created empty rather than absent, so presence is stated once and up front. `OMNI_CAP_DEFAULT` is `OMNI_CAP_ALL_BITS` in the header, so the ninth bit is the catalog index and a block that has one is visible as having one |
| `wm_pid` | creator's pid | recovery evidence, cleared on clean shutdown |
| `pool_base`, `pool_size` | `OMNI_POOL_OFF`, `OMNI_INITIAL_POOL` | from §2's derivation; asserted against `OMNI_POOL_OFF` at build time |
| `arena_end` | `pool_base` | an empty arena. The end is the bump cursor, so an empty arena is where they meet |
| `arena_free_head` | `OMNI_REF_NONE` | an empty free list terminates on the sentinel rather than on 0, because 0 is not a valid frame offset: the pool starts at `OMNI_POOL_OFF` |
| `region_head` | `OMNI_REF_NONE` | same reason, same sentinel |
| `catalog_free_head` | the first unseeded slot, or `OMNI_REF_NONE` only if seeding consumed every slot | a fresh block is not short of free slots, it is *entirely* free slots, so the freelist is a chain over the unseeded tail of the array rather than the sentinel. The creator seeds a low run of slots for the core `wm.*` keys and links the whole remainder, which is why the head is usually a real index and the sentinel is the rare case it was designed for |
| `catalog_free_count` | `OMNI_CATALOG_SLOT_COUNT` minus the number of slots seeding consumed | the honest count for the same reason. Zero is correct only for a block whose catalog is full, and a fresh block is the opposite of full, so a creator that wrote zero here would have its first `omni_set` walk an empty freelist while sixteen thousand FREE slots sat in the array looking unavailable |
| `request_ticket_next` | 1 | 0 is reserved as "no ticket ever issued", so the first real ticket is 1. A u32 wraps in practice, so §0.1's no-wrap rule holds within an epoch and a wrapped ticket is rejected against the live one rather than trusted |
| `catalog` slots, the 16,384 not handed to a seeded entry | `entry_generation` = 0, `FREE` set, `DESTROYED` clear, `name_ref` = the next unseeded slot, `0xFFFFFFFF` on the last one | these are the empty slots, and generation 0 is what distinguishes "never allocated" from "allocated and later freed" (`configstorelayout.md` §6 gives 1 to the first allocation of a slot). `name_ref` carries the freelist link that §6 reserves for a FREE entry, which is what makes them allocatable rather than merely present. `DESTROYED` is deliberately clear, and it stays clear on a chain of never-allocated slots rather than being set to match the generic FREE rule in `configstorelayout.md` §6: that rule describes a slot freed from a live entry, where `DESTROYED` is a statement about a name that existed and was deleted, and a slot that was never named has nothing to have destroyed |
| `catalog` slots handed to a seeded entry | `entry_generation` = 1, both lifecycle bits clear | the first allocation of a slot is generation 1. The creator takes these from the low end of the array directly rather than from the freelist, so seeding never has to distinguish a fresh slot from a recycled one, and the chain of unseeded slots above stays intact for the first real `omni_set` |
| `region_desc_count` | 0 | no descriptors; §7 allocates them |
| `section_table_offset`, `section_table_size`, `first_section_offset` | the §2 constants | these three are geometry, not state, and a reader checks them rather than trusting them |
| `region_desc_base` | `OMNI_REGION_DESC_OFF` | geometry, same reasoning |
| `writer_pid`, `writer_token` | 0 | the creator is not holding the futex |
| `commit_state` | `OMNI_COMMIT_IDLE` | a reader must not be made to retry by the act of creating the block |
| `active_commit_id` | 0 | no commit in flight; the field is meaningful only while `commit_state != IDLE` |
| `ready` | `OMNI_READY_NOT_READY` | `state` is CREATING and `ready` is NOT_READY at the same time on purpose: they answer different questions and a fresh block is legitimately both |
| reserved tail, offsets 168..255 | zero | and now zero *because* of the `memset`, not by omission |

Two of these are worth defending, because they are the ones a "just memset it and
fill in the obvious fields" implementation gets wrong.

`state` starts at CREATING rather than READY even though the creator is about to
publish. The window between the `ftruncate` and the seeding commit is real: a
client polling for an instance can find the file in that window, and CREATING is
what tells it to come back rather than to treat an empty catalog as a
misconfiguration. The alternative, publishing READY with an empty catalog, is a
block that a reader will accept and conclude from.

`catalog_free_head` does **not** start at `OMNI_REF_NONE`, and this is the one
row in the table where the obvious answer is wrong. An empty free list is the
shape that makes the sentinel easy to read, and `arena_free_head` and
`region_head` both get it, which is exactly why the catalog looks like it should
too. But a fresh catalog is not an empty freelist, it is a full one: 16,384 slots
of which seeding has taken a low run, and every one of the rest is FREE and
waiting. Writing the sentinel would claim the block had no free slots, and
`catalog_free_count` of zero would agree with that claim, so the first
`omni_set` after startup would find no slot to allocate and fail on a block with
sixteen thousand of them sitting in the array. Linking the unseeded tail through
each slot's `name_ref` is the same mechanism `configstorelayout.md` §6 defines for
a freed entry, which is the point: a slot that was never allocated and a slot that
was freed are in the same state and belong on the same list, and the only
difference between them is the retained generation, which is why that field is
0 here and not 1.

`arena_free_head` and `region_head` keep their sentinels, and the asymmetry is
not an inconsistency. Both of those are genuinely empty at creation: there is an
arena with no frames in it and a region table with no descriptors, and nothing
exists to put on either list. The catalog is the one list in the block that is
non-empty before a single user operation, because the slots are allocated by the
array existing rather than by anything being stored in them.

Two details in that table are about the *sections* rather than the header, and both
exist so that a reader of an empty block gets a refusal rather than a plausible
wrong answer. An empty catalog index would have `live_count` and `used_count` both
zero, which a binary search handles correctly and which tells a reader nothing
about whether the section is populated; so the creator writes
`OMNI_CATALOG_INDEX_VERSION` at the index header's version word from
`configstorelayout.md` §6.1, and a reader that finds it absent or different
refuses with `OMNI_ERR_BLOCK_UNSUPPORTED` instead. An empty request ring and an
empty journal ring are both genuinely empty and need no such word: their slot
counts are zero, their heads are `OMNI_REF_NONE`, and there is nothing for a
reader to misinterpret.

Growth reuses §9 exactly: a block that grows is updated in place by the existing
growth path, and every field in this table is already correct, so growth touches
only `block_size` and `pool_size` plus the journal event. A recreate is the only
path that reruns creation, and it is a new `epoch` on a new file, never an
in-place reset of the old one.
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
- `save` takes an optional key-path pattern that narrows the scope, and with no argument it writes the whole block under that same test, so the argument is purely additive. The pattern is a dotted prefix with a single trailing `*` meaning the subtree below it, which is what makes a layout slot extractable: a layout authored over IPC under `omniwm.layouts.delta` is written by `save omniwm.layouts.delta.*` and nothing else is in the file. A layout is four keys, so the trailing wildcard is load-bearing here rather than a convenience: without it `delta.rules` would be written and `delta.spaces` would not. The exclusion test is identical inside a narrowed scope, so narrowing cannot leak window-dependent or ephemeral state, and it is the same test the soft reset uses, so `save`, a narrowed `save` and a soft reset can never disagree about which entries are configuration. A general glob is deliberately not supported; one trailing wildcard is enough to name a namespace, and a full pattern language would make the interaction between a pattern and the exclusion test harder to reason about than the feature is worth.
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
- Mixed-endianness hosts (native, tied to `format_version`).
- Extension-specific semantic validation beyond structural framing checks.
- Compile-time ABI tests asserting the header's values against this document.

### 14.1 Three operations the store owes tags, and they are not one operation

These are not on the list above and are recorded separately because they are
**required** rather than deferred, and because they are the only reason the store
needs a subtree concept at all. `tags.md` §5 makes a tag a catalog entry with
children, and that one decision obliges the store for three distinct operations.
They are listed separately rather than as "add subtree support" because an
implementation that treats the third as a special case of the first will find the
problem too late.

- **Subtree delete.** Destroying a client removes its membership children from
  every tag naming it. §8's `save` and soft-reset classification sidesteps this
  entirely by flag, but teardown cannot: it is a delete of everything beneath a
  prefix. Without it, a destroyed client leaves membership children naming a
  recycled `entry_id`, and the next client to land in that slot appears on the old
  client's tags.
- **Subtree exchange.** `ipc.md` §4's `swap_tags` exchanges two tags' contents in
  one grouped commit. This is **not** a special case of the delete, and the reason
  is allocation rather than iteration: a delete may free each frame as it goes,
  because nothing else references it, while an exchange frees nothing at all,
  because every child of one tag is still live in the other. It has to move values
  between two existing subtrees with no intermediate state in which either is short
  a member, and that is a different code path rather than a flag.
- **Generation-correct deletion.** Applies to both of the above: a membership
  operation must compare the `entry_id` *and* `entry_generation` it was given
  against the live slot, or it will cheerfully delete a new client's membership
  after a slot is recycled. This is §0.1's rule applied to operations that do not
  exist yet, and it is why the membership child stores an `entry_ref` rather than a
  bare `entry_id`. For the exchange the same care lands in a different place: a
  child naming a destroyed-and-recycled client must not carry the stale half
  across, so the exchange validates both sides or refuses, and refusing is correct
  because a swap with a dead member in it is already a bad request.

