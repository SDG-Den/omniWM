# OmniWM Architecture Audit

Date: 2026-09-25
Status: closed, in scope resolved, out of scope deferred
Scope: OmniWM design documents and source-tree structure compared with selected mango-dev reference files

> **Disposition, added 2026-09-26.** The findings below are preserved as written
> and are a historical record. Scope was the config store and IPC surface
> (`README.md` roadmap stages 1 and 2) plus the sections of `helpers.md` that
> existed at review time. Everything within that scope is resolved; see
> §8. Findings outside that scope are recorded as deferred, not as outstanding
> work, and §4 and Gate 4 in particular are not gaps to be closed before
> implementation begins.

## Executive assessment

OmniWM is currently a design and scaffolding repository, not an implementation. The C and header files are empty, the build system is absent, and the central compositor documents for layouts, server behavior, TOML parsing, and appearance are empty. The most detailed material concerns a shared-memory configuration store, its byte-level layout, and a socket facade.

The design has three independent implementation blockers:

1. The shared-memory store contains internal contradictions in its journal, catalog, region, and header layouts. Several safety and recovery invariants are also missing.
2. The socket protocol has ambiguous cursor, subscription, type-encoding, readiness, and error contracts.
3. The component model has no complete runtime-state, dependency, failure, shutdown, or plugin ABI contract.

The repository also lacks design for the features that define a window manager: constraint layouts, scene ownership, rendering, compositor infrastructure, input integration, and external protocol boundaries.

The findings below separate confirmed contradictions from inferred risks. A confirmed contradiction means two inspected documents or a document and its own tables disagree. An inferred risk means the intended behavior is not defined well enough to implement safely.

## Repository state

At the time of review:

- All 59 `.c` files and all 59 `.h` files under `src/` and `include/` were empty.
- The non-empty design material consists of:
  - `README.md`
  - `devnotes/configstorage.md`
  - `devnotes/configstorelayout.md`
  - `devnotes/helpers.md`
  - `devnotes/ipc.md`
  - `devnotes/filestructure.md`
- `devnotes/layoutsystem.md`, `devnotes/looks.md`, `devnotes/server.md`, and `devnotes/tomlparser.md` are empty.
- `include/shared/omni_layout.h`, referenced as the future location of shared constants, does not exist.
- There is no Makefile, Meson build, CMake build, package manifest, lockfile, Nix flake, test directory, CI configuration, protocol source, or license file.
- The README roadmap has all stages unchecked (`README.md:38-51`).
- The documented fuzz target in `devnotes/configstorage.md:218-227` has no corresponding test implementation.
- The working tree at the beginning of the review contained the following pre-existing state:

  ```text
   D include/core/event.h
   D src/core/event.c
  ?? include/core/events.h
  ?? src/core/events.c
  ```

No implementation changes were made during this audit.

## 1. Shared-memory storage design

### 1.1 Confirmed layout contradictions

#### C1. Journal entries have no epoch field

The design requires every journal entry to carry an `epoch` so consumers can detect a compositor restart. The concrete slot is exactly 40 bytes and its fields consume all 40 bytes without an epoch field.

Evidence:

- `devnotes/configstorage.md:149-155`
- `devnotes/configstorelayout.md:47`
- `devnotes/configstorelayout.md:256-268`
- `devnotes/configstorelayout.md:276-278`
- `devnotes/configstorelayout.md:370`

The header epoch is not a substitute. A header epoch identifies the current instance, while a per-entry epoch is needed to interpret a journal slot after restart or slot reuse.

#### C2. Scalar values have no defined catalog location

The prose says small values are stored inline, and the layout describes scalar inline fields. The 16-byte catalog entry has no inline payload field. The concrete example instead reuses an arena frame for a four-byte value.

Evidence:

- `devnotes/configstorage.md:73-79`
- `devnotes/configstorage.md:149-150`
- `devnotes/configstorelayout.md:178-194`
- `devnotes/configstorelayout.md:203-205`
- `devnotes/configstorelayout.md:379-381`

This affects the meaning of `body_ref`, arena growth, value validation, and the journal inline rule.

#### C3. Region descriptors alias arena allocations

Region descriptors are described as a fixed array at the pool start. Arena allocations also bump upward from `pool_base`, which is the pool start. No reserved descriptor range is defined.

Evidence:

- `devnotes/configstorelayout.md:102-105`
- `devnotes/configstorelayout.md:140-161`
- `devnotes/configstorelayout.md:320-321`
- `devnotes/configstorelayout.md:350`

The descriptor array can therefore be overwritten by normal catalog or arena allocation.

#### C4. The alignment rule contradicts the layout tables

The layout states that every multi-byte field must be 16-byte aligned, but its own tables place fields at 2-byte, 4-byte, and 8-byte offsets. Journal slot pitch is 40 bytes, which is not a multiple of 16.

Evidence:

- `devnotes/configstorelayout.md:9-10`
- `devnotes/configstorelayout.md:20`
- `devnotes/configstorelayout.md:56`
- `devnotes/configstorelayout.md:96-97`
- `devnotes/configstorelayout.md:191-192`
- `devnotes/configstorelayout.md:260-263`
- `devnotes/configstorelayout.md:328-334`
- `devnotes/configstorelayout.md:360-363`

A reader applying the stated rule literally would reject valid entries.

#### C5. Header and section-table placement are ambiguous

`header_size` is documented as 256 bytes while the first section begins at `0x1000`. The section table is placed inside the header region, but no header field clearly defines its offset and size. A client deriving section locations from `header_size` can land inside the section table.

Evidence:

- `devnotes/configstorelayout.md:39`
- `devnotes/configstorelayout.md:74-79`
- `devnotes/configstorelayout.md:92`
- `devnotes/configstorelayout.md:123`
- `devnotes/configstorage.md:40-43`
- `devnotes/configstorage.md:62`

#### C6. The mandatory read guard is not a testable predicate

The guard bundle includes a malformed condition for mutable flags and compares a value length against an offset range. The same text is described as an invariant asserted by fuzzing, although the condition cannot be evaluated as written.

Evidence:

- `devnotes/configstorelayout.md:202-206`
- `devnotes/configstorelayout.md:347-363`

#### C7. Known-tag validation rejects extension values

The read guard requires known type tags, while the type system reserves `0x8000` and above for extensions and save is required to preserve semantically unknown extension keys.

Evidence:

- `devnotes/configstorelayout.md:191`
- `devnotes/configstorelayout.md:203`
- `devnotes/configstorelayout.md:360-363`
- `devnotes/configstorage.md:89`
- `devnotes/configstorage.md:235`

The core reader must be able to carry unknown extension values without interpreting them, while still refusing malformed core values.

#### C8. Persistence classification has no representation

Save must exclude window-dependent state, but catalog flags contain only mutable, destroyed, and free state. No scope, persistence, owner, or window-specific bit exists.

Evidence:

- `devnotes/configstorage.md:234-235`
- `devnotes/configstorelayout.md:192`
- `devnotes/configstorelayout.md:347-358`

#### C9. Delete and arena reclamation disagree

The prose says destruction returns the slab to a freelist and permits reuse. The layout says arena frames remain allocated after deletion and reclaim only trailing space.

Evidence:

- `devnotes/configstorage.md:187`
- `devnotes/configstorelayout.md:162-163`
- `devnotes/configstorelayout.md:213-215`

A destroyed catalog entry also has no stable tombstone if its slot is immediately recycled.

#### C10. Generation and journal sequence have incompatible meanings

The header generation is described as the newest journal sequence, but growth commits and header-only commits increment generation without appending a journal entry. Grouped commits append multiple entries under one generation.

Evidence:

- `devnotes/configstorelayout.md:96`
- `devnotes/configstorelayout.md:225-226`
- `devnotes/configstorelayout.md:370`
- `devnotes/configstorage.md:161-165`
- `devnotes/configstorage.md:24-25`

The protocol must define whether `seq` is a generation, a journal sequence, or a separate commit identifier.

#### C11. The journal size arithmetic is wrong

The documented formula `8 + 4096 * 40` equals `163848` bytes (`0x28008`), not the stated `167936` bytes (`0x29000`).

Evidence:

- `devnotes/configstorelayout.md:47`
- `devnotes/configstorelayout.md:64`

The offsets remain numerically safe in places, but clients deriving extents from the stated size will reject or misread valid data.

#### C12. String-valued tags are not consistently classified

The inline rule names strings, blobs, arrays, tuples, and gradients as framed values, but several string-valued tags are not explicitly classified. A string-valued enum or other short string has no unambiguous representation.

Evidence:

- `devnotes/configstorage.md:128-140`
- `devnotes/configstorelayout.md:270-274`

#### C13. Header field accounting is incomplete

The header table ends before the documented used range, and the request ticket counter has no named home in the header layout.

Evidence:

- `devnotes/configstorelayout.md:39`
- `devnotes/configstorelayout.md:85-109`
- `devnotes/configstorelayout.md:288`

#### C14. Region capacity is defined for one slot but validation permits two

The region constant is described as a one-frame maximum, while payload allocation multiplies by `slot_count` and creation validation checks only `stride * height`.

Evidence:

- `devnotes/configstorelayout.md:59`
- `devnotes/configstorelayout.md:326`
- `devnotes/configstorelayout.md:337-339`

A two-slot region can exceed the stated per-frame limit.

#### C15. Region publication parity is inconsistent

The painter writes the slot selected by the old sequence, then publishes `seq + 1`. The reader derives the slot from the observed, incremented sequence. The first publication and subsequent publications therefore select different slots unless an unstated offset convention is added.

Evidence:

- `devnotes/configstorage.md:171-176`
- `devnotes/configstorelayout.md:341-344`

#### C16. Journal resynchronization has no starting cursor

The consumer is told to discard an invalid cursor and then follow the ring from where it left off, but the cursor has just been discarded. A full snapshot is mentioned, but the first live journal position is not defined.

Evidence:

- `devnotes/configstorage.md:155`
- `devnotes/configstorelayout.md:276-278`

#### C17. Request state and addressing differ between documents

The prose lists `FREE`, `PENDING`, and `DONE`, while the layout adds `ERROR` and defines destroy requests by target ID. The lifecycle of an error slot and the addressing of create versus destroy requests are not equivalent between the two descriptions.

Evidence:

- `devnotes/configstorage.md:182-186`
- `devnotes/configstorelayout.md:280-307`

#### C18. Name and value limits differ by path

The catalog allows names up to 4095 bytes, but the request slot has a 64-byte name field and a 63-byte maximum. Larger values are expected to be written after creating an entry, but the create request name itself has no alternate representation. Per-entry request limits are also both described as fixed and deferred.

Evidence:

- `devnotes/configstorage.md:81`
- `devnotes/configstorage.md:167`
- `devnotes/configstorage.md:210-214`
- `devnotes/configstorage.md:251`
- `devnotes/configstorelayout.md:44`
- `devnotes/configstorelayout.md:297-300`
- `devnotes/configstorelayout.md:312`
- `devnotes/configstorelayout.md:389-390`

#### C19. Optional socket capability semantics are unclear

The capability bits include a socket-presence bit, but the store is designed to operate without the optional facade. The lifecycle of the bit and behavior of clients that discover only the block are not specified.

Evidence:

- `devnotes/configstorelayout.md:111-117`
- `devnotes/configstorage.md:204-206`

#### C20. Region stride is described inconsistently

The prose defines stride as exactly width times bytes per pixel, while the layout permits any stride greater than or equal to that value.

Evidence:

- `devnotes/configstorage.md:171`
- `devnotes/configstorelayout.md:337-339`

### 1.2 Missing concurrency and recovery invariants

#### Futex ownership and recovery

The futex is a small shared word with no holder identity, owner PID, or recovery rule. A dead writer can leave the block permanently locked. Because every mapping process can write every byte, the WM cannot treat the futex as trusted input.

Evidence:

- `devnotes/configstorage.md:225`
- `devnotes/configstorage.md:29`
- `devnotes/configstorelayout.md:93`
- `devnotes/configstorelayout.md:357`

A safe design needs a defined owner and liveness/recovery rule. A bounded wait alone does not define how ownership is recovered.

#### Journal gap detection

The ring drops old entries, but the ring word contains only head and count. There is no oldest retained sequence or explicit gap marker. A watcher can silently miss events if it falls behind.

Evidence:

- `devnotes/configstorage.md:152-155`
- `devnotes/configstorelayout.md:238-254`
- `devnotes/configstorelayout.md:354`

A consumer needs a way to distinguish a valid next entry from a dropped range.

#### Journal slot and key-reference lifetime

Recycled journal slots are not individually versioned, and `key_ref` is a recyclable catalog slot index. An event can therefore refer to a different key after deletion and reuse.

Evidence:

- `devnotes/configstorage.md:150-151`
- `devnotes/configstorelayout.md:199-200`
- `devnotes/configstorelayout.md:214-215`
- `devnotes/configstorelayout.md:263`

A stable key identity or per-event generation is required.

#### Memory ordering

The documents mention atomic builtins but do not define release/acquire relationships between payload writes, journal slot writes, the ring word, and the generation store.

Evidence:

- `devnotes/configstorelayout.md:17-20`
- `devnotes/configstorelayout.md:245-246`
- `devnotes/configstorelayout.md:371-375`
- `devnotes/configstorage.md:30`

This is a correctness issue on weakly ordered CPUs, not only a portability concern.

#### Writer death during a commit

A writer can die after partially mutating a catalog entry, arena frame, region descriptor, request slot, or freelist. The current design does not define an aborted-commit marker or guarantee that every intermediate state is rejected by the read guard.

Evidence:

- `devnotes/configstorage.md:23`
- `devnotes/configstorage.md:30`
- `devnotes/configstorelayout.md:163-167`
- `devnotes/configstorelayout.md:374-375`

#### Region validation on every read

Region fields are writable by every mapper, but the detailed read guard is primarily written for catalog entries. Pixel access must independently validate descriptor offsets, lengths, dimensions, stride, format, slot count, and integer-overflow conditions before uploading.

Evidence:

- `devnotes/configstorage.md:222-225`
- `devnotes/configstorelayout.md:337-339`
- `devnotes/configstorelayout.md:360-363`

#### Region descriptor discovery

A painter holding a catalog mapping needs a defined way to derive the descriptor base from the block itself. The current region ID points to a descriptor, but the descriptor base is described only in prose.

Evidence:

- `devnotes/configstorelayout.md:136-138`
- `devnotes/configstorelayout.md:194`
- `devnotes/configstorelayout.md:320-321`

#### Region sequence reuse

A recreated region can reuse a descriptor with a sequence equal to the WM's cached sequence. The cache may then skip the upload. No global monotonic region sequence or descriptor generation is defined.

Evidence:

- `devnotes/configstorage.md:174-175`
- `devnotes/configstorelayout.md:332-334`
- `devnotes/configstorelayout.md:339`

#### Request handoff and PID reclaim

Completion and reclamation are not ordered against consumer reads. PID liveness checks are vulnerable to PID reuse and can reclaim a `DONE` slot before the requester consumes it.

Evidence:

- `devnotes/configstorage.md:183-186`
- `devnotes/configstorelayout.md:302-307`

#### Freelist traversal and arena exhaustion

The writer follows a mutable freelist chain while writer-side checks are explicitly not enforcement. Arena leaks from overwrite, delete, and growth paths can permanently exhaust the block.

Evidence:

- `devnotes/configstorage.md:167`
- `devnotes/configstorage.md:225`
- `devnotes/configstorelayout.md:196-215`
- `devnotes/configstorelayout.md:233-234`
- `devnotes/configstorelayout.md:353`

#### Header self-consistency

`pool_base`, `block_size`, `pool_size`, `arena_end`, and `region_head` are mutable inputs used as bounds. They need cross-validation before any arena or region access.

Evidence:

- `devnotes/configstorelayout.md:102-105`
- `devnotes/configstorelayout.md:350-363`

#### Mapper staleness

Startup unlinks and recreates the block, but an existing mapper has no fully defined stale-mapping test and recovery action for every generation state.

Evidence:

- `devnotes/configstorage.md:191-193`
- `devnotes/configstorage.md:155`
- `devnotes/configstorelayout.md:98-100`

#### Grouped-commit boundaries

Grouped commits append several entries with one generation, but the journal has no explicit group-complete marker. Consumers cannot tell whether all entries belonging to a grouped operation have been observed.

Evidence:

- `devnotes/configstorage.md:33`
- `devnotes/configstorage.md:163-165`
- `devnotes/configstorelayout.md:264`
- `devnotes/configstorelayout.md:370`

#### Request dimension truncation

Region dimensions are stored as `u16`, while request fields are `u32`. The protocol does not reject values that cannot be represented in the descriptor.

Evidence:

- `devnotes/configstorelayout.md:293-294`
- `devnotes/configstorelayout.md:328-329`
- `devnotes/configstorelayout.md:337-339`

## 2. Socket facade and IPC design

### 2.1 Confirmed contract problems

#### Watch request field

The command is `watch`, but the example uses `"play"` and the surrounding text calls the value a glob. The accepted field name is not consistently defined.

Evidence:

- `devnotes/ipc.md:41-45`
- `devnotes/ipc.md:128-147`
- `devnotes/configstorage.md:208-214`

#### Watch ordering

The protocol says `"newest-first-forward"` while also requiring forward replay from `since` and live forwarding. The ordering is therefore contradictory or undefined.

Evidence:

- `devnotes/ipc.md:137-151`
- `devnotes/configstorage.md:147-155`

#### Sequence identity

The success envelope defines `seq` as the generation on which a read or write landed. `watch.since` is a journal sequence. The design does not state that they are equal, and grouped commits create multiple journal entries under one generation.

Evidence:

- `devnotes/ipc.md:47-55`
- `devnotes/ipc.md:121-126`
- `devnotes/ipc.md:148-151`
- `devnotes/configstorage.md:24-25`
- `devnotes/configstorage.md:149-153`

#### Missing acknowledgement cursor

Successful responses require `seq`, but the watch acknowledgement contains only `ok`, `result`, and `id`.

Evidence:

- `devnotes/ipc.md:47-55`
- `devnotes/ipc.md:132-135`

#### Unknown-key semantics

The open-catalog rule says unknown keys are accepted and served verbatim, but `get` returns `KEY_NOT_FOUND`. The protocol needs to distinguish an absent key from an existing extension key that the core does not understand.

Evidence:

- `devnotes/ipc.md:7-8`
- `devnotes/ipc.md:108-125`
- `devnotes/configstorage.md:20-24`
- `devnotes/configstorage.md:214`

#### Undefined unwatch operation

`unwatch` appears in the command set and prose, but no request shape, subscription ID, response, or multi-subscription rule is defined. A connection may have any number of watches.

Evidence:

- `devnotes/ipc.md:41-45`
- `devnotes/ipc.md:146-153`
- `devnotes/ipc.md:198-201`
- `devnotes/configstorage.md:208-214`

#### Incomplete JSON type mapping

The storage type list contains types without a corresponding JSON row, including `ratio`, `percent`, `vec2u`, and `hsva8`. Extension tags and `action_ref` also lack a complete wire representation.

Evidence:

- `devnotes/ipc.md:66-95`
- `devnotes/configstorage.md:89-141`

#### Composite values lack discriminators

`option` needs a nested type tag. `array` requires an element type on set but has no request field for it. `tuple` needs its field tags. `binding` stores an action reference but the wire form uses a command string. `rule`, `gradient`, and other composite records have incomplete schemas.

Evidence:

- `devnotes/ipc.md:89-94`
- `devnotes/configstorage.md:129-137`

#### Action argument and result contracts

The JSON section applies typed decoding to `exec` arguments, but the command section says the action contract defines their meaning. Successful results may be arbitrary JSON or null, while every successful response requires `seq`. Action execution failure has no error code.

Evidence:

- `devnotes/ipc.md:66-70`
- `devnotes/ipc.md:47-55`
- `devnotes/ipc.md:155-167`
- `devnotes/helpers.md:97-101`
- `devnotes/helpers.md:224-226`

#### Save response semantics

Save is assigned per-line `constructions` and `failed` counts, but it serializes current state rather than parsing input lines. The meaning of those counts and the returned sequence are undefined.

Evidence:

- `devnotes/ipc.md:169-183`
- `devnotes/configstorage.md:231-235`

#### Reset reversibility

Full reset is described as reversible through save, although save explicitly excludes window-dependent state. No pre-reset snapshot, undo token, or complete reset scope is defined.

Evidence:

- `devnotes/ipc.md:184-195`
- `devnotes/configstorage.md:231-235`

#### Readiness

The socket starts before all components finish activation, but there is no handshake, ready field, epoch field, queueing policy, or command-specific pre-ready behavior.

Evidence:

- `devnotes/ipc.md:22-26`
- `devnotes/ipc.md:191-206`
- `devnotes/configstorage.md:229-235`
- `devnotes/helpers.md:49-57`
- `devnotes/helpers.md:193-195`

#### 64-bit JSON precision

`i64`, `u64`, sequence values, IDs, and nanosecond timestamps are represented as JSON numbers without a required arbitrary-precision parser, string encoding, overflow rule, or rounding rule.

Evidence:

- `devnotes/ipc.md:47-55`
- `devnotes/ipc.md:72-80`
- `devnotes/ipc.md:141-153`
- `devnotes/configstorage.md:95-107`

#### Input and output limits

The 1 MiB line limit does not bound decoded string, blob, array, tuple, grouped-key, action-result, or response sizes. Base64 expansion and UTF-8 decoding can also make a valid line larger after decoding.

Evidence:

- `devnotes/ipc.md:30-37`
- `devnotes/ipc.md:66-95`
- `devnotes/ipc.md:112-126`
- `devnotes/configstorage.md:104-107`
- `devnotes/configstorage.md:130-143`

#### Backpressure policy

A 64 KiB per-client send buffer is mentioned, but the design does not define partial writes, queue overflow, client fairness, disconnect behavior, or whether events may be dropped. A slow client can block the compositor event loop or create an unbounded queue.

Evidence:

- `devnotes/ipc.md:198-203`

#### Literal glob names

Only `*` is supported and no escape syntax is defined. If key names can contain `*`, a client cannot reliably watch a literal key. Empty names, character restrictions, and key-length limits are also absent.

Evidence:

- `devnotes/ipc.md:128-147`
- `devnotes/configstorage.md:20-23`

#### Reconnect identity

`OMNI_INSTANCE_SIGNATURE` contains the block path, while the journal epoch is the per-instance identifier. A path reused after restart does not itself distinguish instances, and the socket has no hello containing the epoch.

Evidence:

- `devnotes/ipc.md:204-206`
- `devnotes/configstorage.md:155`
- `devnotes/configstorage.md:199-202`

### 2.2 Inferred IPC risks

- Replay-to-live handoff can race with concurrent commits because the subscription cursor and live attachment are not defined as one atomic operation (`devnotes/ipc.md:137-151`).
- Inclusive `seq >= since` can replay the cursor entry unless the boundary is explicitly exclusive (`devnotes/ipc.md:148-151`).
- Overlapping watch patterns have no subscription identity or duplicate-event policy (`devnotes/ipc.md:128-153`, `:198-201`).
- Reload is a sequence of individual set and exec operations, so external observers can see a partially reloaded configuration (`devnotes/ipc.md:179-182`; `devnotes/configstorage.md:231-235`).
- Save has no generation-stable snapshot or retry rule, so a read-only save can capture mixed state (`devnotes/ipc.md:169-182`; `devnotes/configstorage.md:24-25`).
- The socket path has no Unix-domain path-length rule or fallback (`devnotes/ipc.md:18-21`; `devnotes/configstorage.md:199-202`).
- Error codes are defined as a fixed set, but many failure cases have no assigned code, including invalid envelope shapes, unknown type names, extension values, ring loss, action runtime failure, file errors, and startup readiness (`devnotes/ipc.md:58-64`, `:202-203`).
- The documented command wrapper statement does not account for save serialization, reload replay, or full reset as distinct transaction boundaries (`devnotes/ipc.md:3-8`, `:169-195`; `devnotes/configstorage.md:229-235`).

### 2.3 Per-command disposition

| Command | Defined intent | Missing or conflicting contract |
|---|---|---|
| `get` | Read one catalog value | Extension values, absent-key semantics, type-independent `length`, races |
| `set` | Single or grouped commit | Result shape, nested type metadata, name limits, sequence identity, extension encoding |
| `watch` | Replay and follow journal events | Pattern field, ordering, acknowledgement cursor, ring loss, replay/live handoff |
| `unwatch` | Remove a subscription | Request shape, subscription identity, response, multiple-watch behavior |
| `exec` | Invoke a registered action | Argument schema, runtime failure code, result sequence, action result limits |
| `save` | Export non-window-specific keys | Snapshot boundary, response counts, file errors, large output |
| `reload` | Replay a file with partial failure | Atomicity, response sequence, file errors, action failure mapping |
| `reset` | Delete one key or restore defaults | Full-reset scope, reversibility, readiness, extension/window state handling |

## 3. Component and server design

### 3.1 Runtime state is missing

`struct omni_component` contains identity, priority, enable key, metadata arrays, and callbacks. It has no state field, instance pointer, userdata, resource handle, or owner. The lifecycle prose nevertheless refers to setting a record to `ACTIVE` or `SUSPENDED`.

Evidence:

- `devnotes/helpers.md:63-86`
- `devnotes/helpers.md:53-57`
- `devnotes/helpers.md:129-134`
- `devnotes/helpers.md:59-61`

The immutable descriptor and mutable live instance need separate types.

### 3.2 Lifecycle transitions are incomplete

- A pre-existing false enable value has no documented initial-suspension path (`devnotes/helpers.md:53-55`, `:115-134`).
- `resume()` does not state whether `init()` runs again (`devnotes/helpers.md:129-134`).
- Equal-priority destruction order is undefined (`devnotes/helpers.md:134`, `:208-209`).
- There is no callback drain, quiescence, idempotence, or suspended-component destruction rule (`devnotes/helpers.md:129-136`).
- All registration and lifecycle callbacks return `void`, so initialization failure cannot be propagated (`devnotes/helpers.md:38-43`, `:82-86`).
- Partial activation and rollback are not defined (`devnotes/helpers.md:49-57`).

### 3.3 Priority is not a dependency graph

The design explicitly says hard dependency edges do not exist. Priorities are described as advisory even though activation and reverse destruction depend on their order.

Evidence:

- `devnotes/helpers.md:197-209`
- `devnotes/helpers.md:227-229`
- `devnotes/helpers.md:66-86`

A numerically valid order does not detect a missing prerequisite or guarantee that destruction happens in dependency-safe order.

### 3.4 Action and trigger ownership is incomplete

Action records contain only a name and handler. Trigger records contain only a pattern and callback. The event API has no subscription handle, owner, unregister operation, or duplicate-pattern policy. The action registry is global and name-based.

Evidence:

- `devnotes/helpers.md:97-113`
- `devnotes/helpers.md:138-165`

This creates ambiguity when a component is suspended, destroyed, or replaced, and when two registrations use the same name.

### 3.5 Boot readiness is not an external contract

The socket is started before registration and `omni_boot()` complete. Components are marked active before the block is marked ready. The server record has no readiness or initialization error state.

Evidence:

- `devnotes/helpers.md:49-57`
- `devnotes/helpers.md:179-195`

Clients and callbacks can observe a partially initialized system. No protocol or internal barrier defines pre-ready behavior.

### 3.6 The component model is not a plugin ABI

The registration table is static and compile-time known. A feature requires editing the table in `server.c`. There is no ABI version, host API, capability negotiation, load boundary, unload contract, or context parameter.

Evidence:

- `devnotes/helpers.md:10-16`
- `devnotes/helpers.md:38-47`
- `devnotes/helpers.md:221-226`

This supports internal modularity, but not native third-party plugins or a stable external extension interface.

### 3.7 Reentrant dispatch creates a lifetime hazard

Event dispatch is synchronous, ordered by registration, and permits a callback to commit changes reentrantly. Suspend and resume unregister callbacks and tear down state. No snapshot, in-flight callback, or lifetime barrier is defined.

Evidence:

- `devnotes/helpers.md:129-151`

A callback can therefore retain state while another path removes its subscription or destroys its component.

### 3.8 Server ownership is underspecified

The server stores the block as `void *` and the socket as a raw integer. It also stores a registry pointer while the registry is described as global. Component callbacks receive no server or component context.

Evidence:

- `devnotes/helpers.md:22-29`
- `devnotes/helpers.md:179-190`
- `devnotes/helpers.md:82-109`

The ownership and lifetime of each resource are therefore implicit.

### 3.9 Filesystem boundaries do not match the component design

The helper design places the component substrate under `core/`, but the file-structure note does not establish a corresponding core or config boundary. The store and IPC designs are referenced but their interfaces and ownership are not included in the component contract.

Evidence:

- `devnotes/helpers.md:1-6`
- `devnotes/helpers.md:20-32`
- `devnotes/filestructure.md:4-31`

## 4. Missing compositor and layout architecture

The repository's central goals are broader than configuration storage. The following contracts are absent or only named as future work:

- Constraint layout representation, evaluation, conflict handling, and persistence.
- Relationship between constraints and client, tag, monitor, floating, and workspace state.
- Per-tag layout state and layout transitions.
- Scene graph ownership across windows, drawing, decoration, and animation.
- Renderer/backend boundary and texture lifetime ownership.
- Output layout, monitor lifecycle, and virtual-monitor behavior.
- Input device, keyboard, pointer, tablet, gesture, and keybinding contracts.
- Shell surfaces, layer shell, input method, clipboard, session lock, screenshot, and screencopy integration.
- Standard and extension Wayland protocol lifecycle.
- Xwayland policy and ownership.
- Application-driven background, fake-client, and visual-effect boundaries.
- Error, shutdown, and resource-lifetime policies for all of the above.

The empty `devnotes/layoutsystem.md`, `devnotes/server.md`, `devnotes/looks.md`, and `devnotes/tomlparser.md` mean that the repository currently has no implementation-level design for these goals.

## 5. Mango reference architecture

Mango is a working reference, not a direct match for OmniWM's goals.

### State ownership

Mango uses one global `MangoServer` containing display, event-loop, scene, renderer, compositor, layout, focus, monitor, animation, and Xwayland state.

Evidence:

- `mango-dev/include/mango/common/server.h:41-60`
- `mango-dev/include/mango/common/server.h:136-141`
- `mango-dev/include/mango/common/server.h:177-202`
- `mango-dev/include/mango/common/server.h:236-243`

Its handles are mostly opaque typedefs whose fields are accessed directly.

Evidence:

- `mango-dev/include/mango/common/types.h:11-31`
- `mango-dev/include/mango/common/types.h:34-133`

### Layout model

Mango layouts are a fixed table of function pointers with an `arrange(Monitor *)` operation. Layout state is mutable and per-monitor/per-tag rather than constraint-based.

Evidence:

- `mango-dev/include/mango/layout/layout.h:12-18`
- `mango-dev/include/mango/layout/layout.h:22-39`
- `mango-dev/src/config/parse_config.c:4709-4718`
- `mango-dev/src/config/parse_config.c:4742-4800`

### Configuration model

Mango's config is an imperative singleton. Parsing installs defaults, parses rules, clamps values, re-applies options, re-runs rules, re-arranges clients, executes commands, and mutates environment variables.

Evidence:

- `mango-dev/src/config/parse_config.c:40-50`
- `mango-dev/src/config/parse_config.c:432-456`
- `mango-dev/src/config/parse_config.c:2700-2726`
- `mango-dev/src/config/parse_config.c:4409-4493`

### IPC model

Mango executes IPC commands inline on the compositor event loop. Its watch clients are long-lived, but ordinary commands and option changes can trigger work that blocks the same loop.

Evidence:

- `mango-dev/src/ipc/ipc.c:295-345`
- `mango-dev/src/ipc/ipc.c:736-1261`
- `mango-dev/src/ipc/ipc.c:982-1011`
- `mango-dev/src/ipc/ipc.c:1263-1424`
- `mango-dev/src/ipc/ipc.c:1602-1629`
- `mango-dev/src/ipc/ipc.c:1641-1677`

### Client, rendering, and animation coupling

Mango's client management directly creates and reparents scene nodes, applies renderer effects, handles animation geometry, and manages Xwayland state.

Evidence:

- `mango-dev/src/manage/client.c:2-45`
- `mango-dev/src/manage/client.c:1844-2155`
- `mango-dev/src/manage/client.c:2537-2570`
- `mango-dev/src/manage/client.c:3847-3995`
- `mango-dev/src/manage/client.c:406-444`
- `mango-dev/src/manage/client.c:4067-4096`
- `mango-dev/src/manage/client.c:4285-4384`

This is a useful compatibility reference but a poor boundary model for a new scene/layout abstraction unless the ownership rules are redesigned.

### Dependencies and licensing

Mango's direct dependencies include wlroots 0.20, Wayland, libinput, scenefx, pixman, cJSON, Pango/Cairo, PCRE2, and optional XCB/Xwayland.

Evidence:

- `mango-dev/meson.build:27-42`
- `mango-dev/meson.build:69-79`
- `mango-dev/meson.build:90-105`

The repository is GPL-3.0:

- `mango-dev/LICENSE:7`

Any direct code reuse requires an explicit GPL-compatible licensing decision and preservation of applicable notices.

## 6. Design gates before implementation

### Gate 1: Freeze the store ABI

Resolve the epoch field, scalar representation, header and section-table placement, alignment rule, region descriptor placement, region parity, sequence identity, journal retention, lock ownership, writer-death recovery, memory ordering, request handoff, and arena accounting. A byte-level layout should not be considered stable until these invariants can be checked independently by a mapper and the WM.

### Gate 2: Define the IPC contract

Choose one request field for watch patterns, define subscription IDs and unwatch behavior, distinguish generation from journal sequence, define ring-loss and replay behavior, specify complete type encoding, settle 64-bit JSON representation, define limits, readiness, save snapshots, reload atomicity, reset scope, action schemas, and error mapping.

### Gate 3: Separate component descriptors from runtime instances

Define instance ownership, state transitions, dependencies, initialization errors, rollback, callback quiescence, shutdown order, action and subscription ownership, and a public ABI if external plugins are intended.

### Gate 4: Define compositor subsystem boundaries

Specify the layout evaluator, scene graph, renderer, backend, output/monitor model, input model, decoration, animation, shell integration, and protocol ownership before creating the corresponding source modules. The component registry should sit above these interfaces rather than hide them behind global state.

### Gate 5: Decide the Mango reuse boundary

Choose which Mango components can be ported, which must be replaced with fresh interfaces, whether Xwayland is supported, which wlroots/scenefx versions are acceptable, and how GPL licensing is handled.

## 7. Evidence inventory

Primary OmniWM documents inspected:

- `README.md`
- `devnotes/configstorage.md`
- `devnotes/configstorelayout.md`
- `devnotes/ipc.md`
- `devnotes/helpers.md`
- `devnotes/filestructure.md`

Primary Mango reference files inspected:

- `mango-dev/include/mango/common/server.h`
- `mango-dev/include/mango/common/types.h`
- `mango-dev/include/mango/layout/layout.h`
- `mango-dev/src/config/parse_config.c`
- `mango-dev/src/ipc/ipc.c`
- `mango-dev/src/manage/client.c`
- `mango-dev/meson.build`
- `mango-dev/LICENSE`

This was a static design audit. No runtime behavior could be validated because OmniWM has no nonempty implementation or build/test configuration.

## 8. Disposition

Added 2026-09-26. One entry per finding, with the document that now owns the
answer. "Deferred" means outside the scope this audit was run at, not
unresolved; deferred work is scheduled by `README.md` roadmap stage, not by this
document.

### In scope, resolved

| finding | resolution |
|---|---|
| 1.1 layout contradictions | `configstorelayout.md` §1 to §8, field offsets, sizes, and capacities, as ABI v1; see the freeze note below |
| 1.2 concurrency and recovery | `configstorage.md` §6 to §10, commit visibility, journal replay, read guard tiers, request handoff |
| 2.1 contract problems | `ipc.md` §3 to §5, complete type encoding, subscription identity, ring loss, limits, readiness, reset |
| 2.2 inferred risks | `ipc.md` §5 to §6, envelope completeness, epoch and commit identity on every response |
| 2.3 per-command disposition | `ipc.md` §4, one subsection per command |
| 3.1 runtime state missing | `helpers.md` §3.2, `struct omni_instance` with state and init errno |
| 3.2 lifecycle transitions | `helpers.md` §3.2, §3.4, ACTIVE/SUSPENDED/FAILED and the toggle engine |
| 3.3 priority is not a dependency graph | `helpers.md` §9, advisory priority with stable registration-order tiebreak |
| 3.4 action and trigger ownership | `helpers.md` §5, §6, handle-based registration and release |
| 3.5 boot readiness | `server.md` §4, header `ready` u32 mapping to READY/DEGRADED/FAILED |
| 3.6 not a plugin ABI | `helpers.md` §8.1, recorded as a rejected design with reasons |
| 3.7 reentrant dispatch hazard | `helpers.md` §4.1, single-threaded dispatch, arguments resolved before the callback |
| 3.8 server ownership | `server.md` §1 to §6, single owner with typed borrows |
| 3.9 filesystem boundaries | `filestructure.md`, rewritten around `helpers.md` §1 and the `include/shared` ABI surface |
| empty `server.md` | `server.md`, authoritative for server lifecycle |
| empty `tomlparser.md` | `tomlparser.md`, TOML bound to the existing tags |

Two further changes were needed to close findings that were in scope. The
`datetime` tag at 0x31 filled the unassigned hole the header already reserved, so
the known-tag range became 0x01..0x31; the layout engine has since taken 0x32 and
0x33, making it 0x01..0x33 (freeze note below). Numeric typing in TOML is taken
from a key's component registration rather than the literal's magnitude, which is
what keeps a key's stored type stable and matches the socket's own rule for a
`set` that omits `type`.

### Freeze note, 2026-09-26

The freeze that closed finding 1.1 was lifted the same day it was applied,
because the deferred items in the table below are still at design stage and a
change to the tag table, a composite payload, the fixed section set, or a
capacity constant costs a paragraph now and a format version plus a
compatibility story after stage 6. Finding 1.1 itself is not reopened: the
contradictions between `configstorage.md` and `configstorelayout.md` stay
resolved, and the identity vocabulary in `configstorage.md` §0.1 is not in
question. What is reopened is the concrete surface those documents pin down.
`devnotes/layoutengine.md` §2.9 records the reopening and §2.10 lists the changes
the layout engine needs. Those have now been applied: `OMNI_TAG_CONSTRAINT` at
`0x32`, `OMNI_TAG_CLIENT_RULE` at `0x33`, a framed argument-carrying
`OMNI_TAG_BINDING`, and `OMNI_SECTION_SOLVED_LAYOUT` as fixed section 7, which
moves `OMNI_POOL_OFF` to `0x1DB000` and `OMNI_CAP_DEFAULT` to `0xFF`. The
known-tag range is now 0x01..0x33 and `OMNI_FORMAT_VERSION` is still 1, because
the first runnable build is v1 and there is no earlier format to be incompatible
with. The single definition point rule is unaffected: every constant still lives
in exactly one place, `include/shared/omni_layout.h`, and changing one is still a
one-line edit there plus the matching table in `configstorelayout.md` §2.

### Out of scope, deferred

| item | deferred to |
|---|---|
| §4 constraint layouts, tags, scene, renderer, input, protocol surface | roadmap stages 4 to 14; `devnotes/layoutsystem.md` and `devnotes/looks.md` remain empty by decision |
| Gate 4, compositor subsystem boundaries | the first stage that needs the subsystem in question, not before implementation |
| Gate 5, Mango reuse boundary and dependency versions | the stage 4 and 5 design point; GPL-3.0 is the project's chosen license |
| the keybinding path, `modmask` grammar, keysym names (`helpers.md` §11) | roadmap stage 4 |
| full log and util contracts (`helpers.md` §7) | roadmap stage 3 |
| a scene node and animatable property model | roadmap stages 5 and 9 |

Two of these are recorded as open items in the documents that own them, and that
is deliberate: `helpers.md` §11 flags the binding path and the `omni_event`
payload question rather than silently dropping them, because a later stage will
hit both immediately.

