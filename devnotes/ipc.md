# Socket facade (IPC) protocol design

The socket is the thin JSON facade over the store primitives defined in
`configstorage.md` §11 and laid out byte-exactly in `configstorelayout.md`.
Every verb is a wrapper around one of: the catalog read path, the commit
protocol (§12 of configstorelayout.md), the request queue, or the action
registry. There is no key-specific handling anywhere: unknown keys are served
verbatim.

The socket implementation is a client of the block, never a peer of it
(`configstorage.md` §11). Events pushed on `watch` are copies of journal
entries; nothing is invented for the socket.

## 1. Transport

- `AF_UNIX`, `SOCK_STREAM` + `SOCK_CLOEXEC`, non-blocking, served from the WM
  event loop (mango pattern: mango-dev/src/ipc/ipc.c).
- Path: `<block-path>.sock`, beside the SHM file in the same runtime dir
  (discovery §10 of configstorage.md). Unlinked on WM exit / restart. The
  fallback when that path does not fit is in §1.5.
- Permissions 0600, same-user only. The block itself remains authoritative;
  socket access grants no additional trust (`configstorage.md` §10).
- Backlog 16. New connection = new watcher, no handshake.

Framing: newline-delimited JSON (NDJSON), one object per message. One request
in, one response out, same line discipline. Watch pushes are lines in the same
connection after the subscribe acknowledgement (sway/mango precedent).

Constants (single definition point, `include/shared/omni_layout.h`):

```
OMNI_SOCK_MAX_LINE     = 1 MiB     per-message limit
OMNI_SOCK_BACKLOG      = 16
OMNI_SOCK_SEND_BUF     = 64 KiB    write batching threshold, not a limit
OMNI_SOCK_STALL_MS     = 2000      no forward progress, either direction
OMNI_SOCK_DISCONNECT_MS = 60000    still stalled after the partial teardown
OMNI_SOCK_SUN_PATH_MAX = 107       bytes in sockaddr_un.sun_path, NUL included
```

A line longer than the limit yields an error response, then the server closes
that connection. A partial request on EOF is discarded.

`OMNI_SOCK_SEND_BUF` is a threshold, not a cap. It is the point at which the
write path stops issuing small writes and switches to streaming a larger buffer
directly. A response may exceed it, an outbound queue may exceed it, and a queue
may grow without bound; see §3.4 for why that is deliberate and what is guarded
instead.

The stall constants implement the liveness rule in §3.4 and apply in both
directions; §1.1 and §1.2 restate the rationale where it is relevant.

### 1.1 Partial reads

A read returning fewer bytes than requested is neither an error nor a message
boundary. Each connection has its own accumulator holding a partially received
line, and the three states that matter are: no newline seen yet, newline seen so
the message is complete, or the accumulated bytes have passed
`OMNI_SOCK_MAX_LINE`. Nothing else terminates a message, and a client is free to
write a 400 KiB line in as many 4 KiB writes as it likes.

The accumulator is per-connection and bounded only by the line cap, so 16
connections can each hold up to 1 MiB of partial line. That is a passive memory
case, the same class as the stalled writer, so it gets the same treatment rather
than a separate rule: a connection that stops sending mid-line is a stall, and
`OMNI_SOCK_STALL_MS` applies from the last received byte. A client that is
mid-line and paused under a debugger is torn down at 2 s and disconnected at
60 s, exactly like one that stopped reading.

The line-cap check happens as bytes accumulate, not after a newline is found, so
an over-long line is refused before it has been fully buffered. That is the only
reason the cap is a memory bound rather than merely a validity check.

### 1.2 Partial writes and per-client queues

Each connection owns one outbound FIFO. Responses and push lines for that
connection are appended to it in order, so ordering within a connection is
preserved by construction and a response can never overtake a push that was
queued before it.

The queue is unbounded, per §3.4, and there is no drop policy because there is no
point at which the compositor chooses to discard data. Growth comes from a client
either requesting a large value or failing to consume what it already asked for,
and the first is deliberate while the second is caught by the stall timer. A
queue that has grown is released when the connection closes.

Writing is driven by readiness, not by the event loop pushing. A connection with
queued output is written when its socket becomes writable, and while it is not
writable the compositor does nothing on its behalf. This is what makes the
compositor's progress independent of client speed:

- a client that reads promptly is written promptly, in one or a few `write()`
  calls depending on how much is queued;
- a client that stops reading is simply never ready, so the compositor spends no
  time on it and its queue grows unattended until the stall rule fires;
- a `write()` returning zero or a short count means the socket buffer is full,
  not that the peer is dead, so the remainder is retried on the next writable
  event and never treated as a liveness signal. Only the timer decides that.

**Fairness needs no explicit policy.** The event loop services whichever sockets
report ready, so no client can starve another by being slow, because a slow
client is not ready and is therefore not competing for loop time. A round-robin
queue or a per-client byte budget would add machinery to solve a problem the
readiness model does not have.

A stream larger than `OMNI_SOCK_SEND_BUF` is not special-cased. The write path
issues a larger `write()` directly and, if the kernel accepts only part of it,
resumes from the offset the kernel reported. Correctness rests on tracking that
offset, not on the buffer size.

### 1.3 Queue ownership across a stall

The §3.4 teardown is partial, and the partial state has to be precisely what it
claims to be, since "partial" is exactly the kind of word that hides a mistake.

At `OMNI_SOCK_STALL_MS` the connection drops its queued output and stops writing
but stays connected, and its subscriptions remain live. Dropping the queue is
the deliberate half: a queue that has not moved for 2 s is output the peer has
not picked up, and re-sending it on resume would either duplicate lines the peer
did receive or require per-message acknowledgement the protocol does not have.
A client that resumes therefore finds its connection still up, still subscribed,
and with a gap in what it received, and it recovers the gap the way it recovers
any other gap, by re-attaching from a cursor. That is why the `sub` identity and
the cursor design from step 11 matter here and not only for the journal ring.

Because a partial teardown loses queued output, the client's own view of
`journal_seq` may be behind what the compositor sent. This is not an error
condition and is not reported as one; the client detects the gap from the
sequence numbers of the next line it receives.

### 1.4 Ordering guarantee

Within one connection the order is: responses and pushes in the order the server
produced them, with responses to earlier requests always before later ones. A
watch push is produced at commit publication time, so a push and the response to
the command that caused the commit are ordered deterministically by production,
not by arrival.

Between connections there is no ordering guarantee and none is intended. Two
clients watching the same key see the same entries in the same order, because
both read the same journal sequence, but a client that issues `set` and another
that watches the result may observe the pair in either order relative to each
other. A client that needs a read-after-write guarantee uses the `commit_id` in
a response, which is ordered, rather than assuming its own socket timing.

### 1.5 Socket path length and fallback

`sockaddr_un.sun_path` is 108 bytes including the terminating NUL, so
`OMNI_SOCK_SUN_PATH_MAX` is 107 usable bytes. The preferred path is
`<block-path>.sock` beside the shared-memory file, which keeps the socket
discoverable by anyone who found the block, and that is used whenever the
resulting path fits.

When it does not fit, the socket is created at a fallback in `/tmp` whose name is
derived from the block's identity rather than its path, and the real path is
written into the block so discovery still works. Deriving from identity rather
than hashing the path is what makes the fallback stable across restarts: the
same block is reached at the same socket path, so a client that recorded it is
not left holding a stale name after a restart. Two instances never collide,
because the identity input differs per instance.

The advertised path lives in the block's discovery section, so the socket
remains discoverable by reading the block even when it is not next to it. This
is the same discovery mechanism §10 of `configstorage.md` already defines, with
a second possible location rather than a new mechanism.

If even the fallback cannot be created, the WM logs the errno and the exact
attempted path and continues without a socket facade. The block, the request
queue, and every in-process facade are unaffected, so a missing socket degrades
the project to its lower-skill-floor surface without making it unusable. This is
the only place a transport failure is survivable, and it is survivable because
the block is authoritative.

## 2. Envelope

Request:

```
{ "cmd": "get|set|watch|unwatch|delete|exec|save|reload|reset", "id": <any>, ... }
```

`id` is echoed verbatim in the response for correlation; optional, may be a
string or number. Responses are exactly one per request, in order:

```
ok      : { "ok": true,  "result": <type-specific>, "epoch": <u64>, "commit_id": <u64>, "id": <any> }
failure : { "ok": false, "error": { "code": "...", "message": "..." },
            "epoch": <u64|null>, "commit_id": <u64|null>, "id": <any> }
```

`epoch` identifies the compositor instance. `commit_id` identifies the
committed store state on which a read or write landed. Commands that expose a
journal position also return `journal_seq`; these identities are not
interchangeable.

`epoch` and `commit_id` are present on every response, including failures, and
are the instance and store state as of the response. They are `null` in exactly
one case: the block was not readable, so the server has no state to report.
That single rule is what lets `STORE_BROKEN` and `WATCH_EPOCH_CHANGED` name an
epoch in their own error object and still have a consistent envelope, and it
means a client never has to branch on which error it received to learn whether
it is talking to a live instance.

Malformed envelopes:

```
bad JSON                INVALID_JSON, id null, connection stays open
not a JSON object       INVALID_JSON, id null, connection stays open
missing or non-string
  cmd                   UNKNOWN_CMD, id null, connection stays open
line exceeds
  OMNI_SOCK_MAX_LINE    INVALID_JSON, then the connection is closed
```

A malformed message is never a reason to close a connection on its own, because
the cause is a client bug and the client usually wants to keep going. Framing
resynchronises on the next newline, so one bad line does not poison the rest of
the stream. The one exception is an over-long line, which has already
desynchronised the framing and cannot be resynchronised without a rescan, so
the server closes that connection.

`id` cannot be recovered from a message that did not parse, so it is `null` in
every malformed case. A client that needs correlation across a malformed
message must treat `id: null` as a permanent failure of that message.

Error codes (fixed set):

```
INVALID_JSON    UNKNOWN_CMD     PARAM_INVALID   NAME_TOO_LONG
KEY_NOT_FOUND   BAD_TYPE        BAD_VALUE       CATALOG_FULL
BLOCK_EXHAUSTED ACTION_NOT_FOUND ARGS_INVALID  ACTION_FAILED
WATCH_INVALID NOT_READY       STORE_BROKEN    WATCH_GAP
WATCH_EPOCH_CHANGED           REQUEST_EXPIRED CONFIG_TOO_LARGE
```

The set is closed. Every failure this protocol can report a client maps to one
of these, and a failure with no code is a bug rather than a new condition. Two
classes deliberately have no code because they are not reportable to a client:

- a connection closed by the stall rule, by shutdown, or by a framing error is
  closed, not answered, so there is nobody to send a code to. The client
  observes EOF and re-discovers.
- an internal I/O failure while writing a response, such as the peer vanishing
  mid-write, is likewise a close. Reporting an error into a socket that has
  already failed is meaningless.

`NOT_READY` means the header `ready` field is not `READY`: either the
compositor is still activating components or activation failed. Which one is
distinguishable from the same field, and the per-command table is in §5.2.
`STORE_BROKEN` means the block reached the terminal `BROKEN` state after
an interrupted commit; the request is refused without inspecting the block, and
the client must rediscover the instance after the compositor recreates it. The
two are not interchangeable: `NOT_READY` is a valid block with an unready
compositor, `STORE_BROKEN` is an invalid block.
`WATCH_GAP` and `WATCH_EPOCH_CHANGED` are watch-specific discontinuities and are
never collapsed into a generic failure; see §watch. `REQUEST_EXPIRED` means the
compositor reclaimed the request slot before the client read its result, so no
result is available and the request must be re-issued. It is not a failure of
the request itself and is never reported in place of another request's result.

## 3. JSON value encoding (type tags from configstorage.md §4)

Decoder input rules for `set` / `exec` args; encoder output rules for
`get` / `watch`. Both directions are defined per tag group. `set` must carry
`type` explicitly whenever the JSON value is ambiguous.

| tag | JSON in | JSON out |
|---|---|---|
| bool | `true`/`false` | `true`/`false` |
| i8..i32, u8..u32, f32, f64 | number | number |
| i64, u64 | object `{"$u64":"<decimal>"}` / `{"$i64":"<decimal>"}` | same |
| duration | object `{"$u64":"<ns>"}` | same; nanoseconds, not ms |
| datetime | RFC 3339 string, offset required | same, normalised to UTC `Z` |
| char | string (1 char) | string (1 char) |
| string, font_desc, path, exec, state | string | string |
| blob | base64 string | base64 string |
| keysym | string or number | string (XKB name) |
| keycode | number | number |
| modmask | string `"ctrl+alt"` or array | string `"ctrl+alt"` |
| point, offset, vec2i, vec2u, size, rect | object `{"x","y"(,"w","h")}` | same |
| vec2, vec3, vec4 | object `{"x","y"("z","w")}` | same |
| ratio | object `{"num": number, "den": number}` | same |
| percent | object `{"value": number}` in [0,1] | same |
| rgba8, argb8, hsva8 | `"#rrggbbaa"` / `"#aarrggbb"` / `"#hhssvvaa"` | same |
| rgba32f | object `{"r","g","b","a"}` | same |
| region_ref | object `{"epoch": wide, "id": number, "generation": wide}` | same |
| entry_ref | object `{"epoch": wide, "id": number, "generation": wide}` | same |
| shader_ref, cursor_ref | number | number |
| enum | string | string |
| option | `{"type": <tag>, "value": <typed> or null}` | same |
| array | `{"type":"array", "elem_type": <tag>, "value": [...]}` | same, `elem_type` retained |
| tuple | `{"type":"tuple", "field_types": [<tag>...], "value": [...]}` | same |
| binding | object `{"mods","key","cmd","args"?}` | same; encoding settled in `helpers.md` §6.2 |
| rule | object `{"match":{},"replace":{}}` | same; schema in `helpers.md` §6.2 |
| constraint | object `{"records":[...]}` | same; records in `configstorage.md` §4 |
| client_rule | object `{"match":{},"effect","target","scope"}` | same |
| gradient | `{"type":"gradient", "stops":[{"offset","color"}]}` | same |

Every type named in the storage table of `configstorage.md` §4 has a row here.
`ratio`, `percent`, `vec2u`, and `hsva8` were previously missing; the two
culled-as-redundant candidates are retained deliberately. `hsva8` stays because
HSV is a legitimate authoring input, and the encoder normalises it at the
boundary rather than storing HSV. `percent` stays as a distinct tag because a
fraction in [0,1] and a number in [0,100] are genuinely different stored
intents, and the explicit object makes the caller state which.

### 3.1 Wide integers

A JSON number is a double in every mainstream parser, so any integer past
2^53 is silently rounded. Nanosecond timestamps reach 2^53 in about 104 days,
which is not a theoretical concern for a long-running compositor.

Every value that is definitionally 64-bit is therefore carried as a single-key
wrapper object holding a decimal string: `{"$u64":"18446744073709551615"}` for
unsigned, `{"$i64":"-9223372036854775808"}` for signed. The wrapper is applied
to `i64`, `u64`, `duration`, and the `epoch` and `generation` members of
`region_ref` and `entry_ref`, and to `epoch`, `commit_id`, and `journal_seq` in
every response and event.

The wrapper rather than a bare string, because a bare string is
indistinguishable from a `string`-tagged value: a client reading
`{"$u64":"7"}` cannot otherwise tell a count from a name, and the same field
slot in a different message would be a string. One shape per field, tagged, is
cheaper for a client to implement than guessing which of two shapes a field
takes.

Decoding is strict. A bare JSON number where a wide integer is expected is
`BAD_TYPE`, not a coercion, because silently truncating an id or a sequence is
the failure this encoding exists to prevent. A string that is not a valid
decimal integer in range for the target width is `BAD_VALUE`. `f64` and `u32`
stay plain numbers and are exactly representable.

### 3.2 Composite discriminators

A composite's element or field types are part of its value, not a property the
client infers from the JSON shape. Three rules follow from that.

`option` always carries an explicit `type`, even when `value` is `null`, so a
null option of `u32` is distinguishable from an option of something the client
did not name. Encoding always emits `type` for the same reason.

`array` always carries `elem_type`, on input and on output. The earlier rule
that said the element type is "required on set" in prose but had no field to
carry it is the exact shape of a client having to guess, so the field is now
explicit. An empty array is representable because `elem_type` is present even
when `value` is `[]`.

`tuple` always carries `field_types`, which is authoritative and length-checked
against `value`. A zero-field tuple is representable, and a `value` whose length
differs from `field_types` is `BAD_VALUE` rather than a silent truncation.

`binding` and `rule` are settled in the action ownership contract, `helpers.md`
§6.2, which owns both encodings. A `binding` names its action on the wire and
stores the resolved handle, so a binding whose action is not registered is
`ACTION_NOT_FOUND` at `set` time rather than a dangling handle stored for later.
Its `args` is an optional positional string array, validated against the action's
schema at decode and again at dispatch, so `wm.snap left` is one action rather
than four. A `rule` matches by key pattern and optionally type, and its `replace`
is a full typed value whose type must match the match constraint.

`constraint` and `client_rule` are the two tags added for the layout engine, and
both decode to objects rather than to flat tuples, because a `tuple`'s fields are
positional and these records are read by a human writing a layout file. A
`constraint` carries an array of 16-byte records and is refused with `BAD_VALUE`
if the record count is not integral, a `kind` is outside
`OMNI_CONSTRAINT_KIND_COUNT`, or a `role` is a walk index past the end of the
walked set. A `client_rule` carries a `match`, an `effect`, a
`target` naming a group or layout, and a `scope` of `map_only` or `always`,
which is the pair of timings that would otherwise be indistinguishable.

### 3.3 Value limits

`OMNI_SOCK_MAX_LINE` bounds bytes between newlines and bounds nothing else. A
768 KiB blob is a legal 1 MiB line of base64 and 400 000 `[0,0,0,0]` elements
fit in one line, so a framing limit is not an input limit. Every decoded
quantity has its own bound, checked before any arena allocation is attempted.

The numbers below are derived from the frozen layout, not chosen. The initial
arena is `OMNI_POOL_OFF` to the 4 MiB block end, which is `0x225000`, or
2 248 704 bytes, and every framed allocation costs a 16-byte frame header on top
of its payload.

| limit | value | basis |
|---|---|---|
| `OMNI_VALUE_MAX_NAME` | 4095 bytes | `OMNI_REQUEST_NAME_MAX`, the request slot name field |
| `OMNI_VALUE_MAX_STRING` | 1 MiB | one line's worth of decoded text |
| `OMNI_VALUE_MAX_BLOB` | 768 KiB | the largest blob a 1 MiB base64 line can encode |
| `OMNI_VALUE_MAX_FRAMED` | 1 MiB | largest single arena frame at the initial block size |
| `OMNI_VALUE_MAX_ARRAY_ELEMS` | 65536 | 65536 x 16-byte frame headers = 1 MiB of header worst case |
| `OMNI_VALUE_MAX_TUPLE_FIELDS` | 64 | one cache line per field tag list |
| `OMNI_VALUE_MAX_GROUPED_KEYS` | 4096 | one journal ring of entries per grouped commit |
| `OMNI_VALUE_MAX_NESTING` | 16 | recursion bound for composite decode |

A request that exceeds any of these is refused with `PARAM_INVALID` during
decoding, before the store is touched. The store is therefore never asked to
hold a frame larger than the arena, and the client gets a parse-time error
rather than `BLOCK_EXHAUSTED` after an allocation attempt.

`OMNI_VALUE_MAX_ARRAY_ELEMS` is the bound that has to account for per-element
frame cost rather than a flat byte total, because an array of 65 536 framed
elements carries 1 MiB of frame headers before any payload. The limit is
element count for that reason, and the decoder computes
`elem_count * (payload_min + 16)` against `OMNI_VALUE_MAX_FRAMED` before it
allocates.

### 3.4 No response limit, and stalled peers

There is no response size limit. The socket is meant to be as valid a surface
as the block, and a `get` of a value that exists in the arena must not fail
because the client reached it over a socket. A large read is a deliberate act by
someone who understands their own configuration, and the cost of it landing on
compositor memory is documented behaviour, not something to guard against.

What is guarded is the passive case, because it is the one nobody acts on. A
client that requests a small value and then stops reading has done nothing
deliberately; it may be a bug, a suspended process, or a debugger. That is the
liveness problem, and it is a liveness rule rather than a resource rule: nothing
is refused and no client is asked to behave.

| constant | value | meaning |
|---|---|---|
| `OMNI_SOCK_STALL_MS` | 2000 | no forward progress on an in-flight stream |
| `OMNI_SOCK_DISCONNECT_MS` | 60000 | still stalled after the partial teardown |

The stall timer resets on each successful socket-level write and is only armed
while a stream is in flight. A client that has subscribed and is then idle is
never touched, because it is not mid-transfer. A `write()` returning zero on a
non-blocking socket means the buffer is full, not that the peer is dead, and
must not be treated as death; only the timer decides that.

Staging is deliberate. At `OMNI_SOCK_STALL_MS` the connection drops its queued
output and stops writing, but stays connected with its subscriptions intact,
because a peer that resumes reading a moment later should not lose its
subscriptions, and the cursor design means it can recover without losing its
place. At `OMNI_SOCK_DISCONNECT_MS` of continued no-progress the connection is
closed outright, releasing a genuinely wedged peer. Sixty seconds rather than
thirty because a client blocked on a slow read, or stopped under a debugger,
resolves itself and should not be punished for it; the cost of waiting is
bounded and only applies while nothing at all is moving.

Unbounded queue growth is a documented, deliberate consequence, not an
accident, and `ipc.md` says so in those terms so that a later reader cannot
mistake it for an oversight.

## 4. Commands

### get

Read the current value of one key from the catalog.

```
{ "cmd": "get", "key": "wm.focus.color", "id": 1 }
-> { "ok": true, "result": { "key": "wm.focus.color", "type": "rgba8",
     "value": "#3d6bff", "length": 4 }, "epoch": 7, "commit_id": 43, "id": 1 }
```

Unknown key: `KEY_NOT_FOUND`. `get` uses the open-catalog rule; extension keys
are returned without interpretation beyond typing. A structurally invalid
entry (configstorelayout.md §12 guard bundle) is reported as `BAD_VALUE`.

### set

Single-key commit or grouped commit in one message.

```
{ "cmd": "set", "keys": [ { "key": "wm.gaps", "type": "u32", "value": 8 },
                          { "key": "wm.gaps_enabled", "type": "bool", "value": true } ], "id": 2 }
```

- Without `keys` array, one shot: `{"cmd":"set","key":"...","type":"...","value":...}`.
- A key registered by a component carries a declared type in its descriptor
  (`helpers.md` §3.1). When `type` is omitted and the key is registered, the
  declared type is used and a value that does not fit it is `BAD_TYPE`. When
  `type` is given explicitly it must match the declared type for a registered
  key, which is `BAD_TYPE` on mismatch rather than a silent retyping, because a
  component that declared `u32` cannot be relied on if the block holds `i64`.
  An unregistered key has no declared type, so `type` is then required and is
  the client's own choice.
- This is what makes a bare integer unambiguous: the value's type comes from the
  key's registration, not from the literal's magnitude, so `wm.gaps = 8` keeps
  the type its component declared regardless of whether the number later grows.
- Mapped onto commit protocol §12 of configstorelayout.md: single-key = one
  `commit_id` and one `journal_seq`; grouped = one `commit_id` and N distinct
  `journal_seq` values.
- Unknown keys accepted verbatim, per the open-catalog rule.
- Overwrite-fits rule and arena framing apply (§6 configstorelayout.md).
- The response returns the new `epoch` and `commit_id`. A grouped response
  also returns `journal_seq_start` and `journal_seq_end` so a watcher can
  position itself without guessing the group size.

### watch

Subscribe to a journal-forward stream of events for a key glob. The field is
`pattern`; the term `play` used in older notes is not accepted.

```
{ "cmd": "watch", "pattern": "wm.*", "since": { "epoch": 7, "journal_seq": 43 },
  "gap_policy": "fail", "id": 3 }
-> { "ok": true,
     "result": { "sub": 1, "pattern": "wm.*",
                 "cursor": { "epoch": 7, "journal_seq": 100 } },
     "epoch": 7, "commit_id": 99, "id": 3 }
```

`sub` is assigned by the server, monotonically per connection starting at 1, and
is the identity of this subscription. It is never chosen by the client, so
library code that merges two connections' bookkeeping cannot produce a
collision, and a client that reconnects can never mistake an old `sub` for a new
one. Every push line on the connection carries the `sub` that produced it.

Subsequent lines on the connection are forward in `journal_seq` order from
`since.journal_seq` (or from the current tail if omitted):

```
{ "sub": 1, "event": "key_set", "key": "wm.gaps", "type": "u32", "value": 8,
  "epoch": 7, "commit_id": 44, "journal_seq": 44, "entry_id": 12,
  "entry_generation": 3, "time": 12345 }
{ "sub": 1, "event": "key_delete", "key": "wm.gaps", "epoch": 7, "commit_id": 45,
  "journal_seq": 45, "entry_id": 12, "entry_generation": 3, "time": 12346 }
{ "sub": 2, "event": "custom", "category": "ext.foo.signal", "value": "hello",
  "epoch": 7, "commit_id": 46, "journal_seq": 46, "time": 12347 }
```

- `since` is a complete `(epoch, journal_seq)` cursor. Its `journal_seq` is the
  last sequence already consumed. Replay includes entries with a greater
  `journal_seq` in the same epoch, then the subscription follows live. The
  acknowledgement cursor is the next position the subscription will use.
- `gap_policy` is `"fail"` (default) or `"resync"`; an unknown value is
  `PARAM_INVALID`. It is per subscription and is fixed for the life of that
  subscription. It changes only by re-attaching.
- The block holds no policy field, so this request field is the whole of the
  contract; both values are implemented against the same gap detection.
- A cursor is always `(epoch, journal_seq)`. An epoch mismatch requires a
  commit-id-stable catalog snapshot and a new cursor; it never silently mixes
  entries from two instances.
- `pattern` is the pattern field name. The `play` spelling from older notes is
  not accepted by the parser and yields `UNKNOWN_CMD` or `PARAM_INVALID`.
- `unwatch` takes the `sub` returned by `watch` and closes that subscription
  only; other subscriptions on the same connection are untouched. An unknown or
  already-closed `sub` is `PARAM_INVALID`. Closing the connection closes every
  subscription on it, so an abandoned connection leaks nothing.
- Subscriptions on one connection do not interact. A single journal entry is
  delivered once per matching subscription, each line carrying its own `sub`.
  There is no deduplication across subscriptions, ever, and a subscription's
  output depends only on its own `pattern`, `since`, and `gap_policy`, never on
  what else shares the connection. A client may therefore route two overlapping
  watches into two independent parts of its logic and both see exactly the
  stream they would see alone. The cost is duplicated lines, which is cheaper
  than a client having to reason about when two watches collapse into one.
- A `watch` that matches nothing is not an error; a subscription that never
  fires is a valid subscription.

#### Pattern syntax

A pattern is matched against a key name and supports exactly one metacharacter,
`*`, meaning "any run of characters including none". There is no `?`, no `[...]`
class, and no anchoring syntax, because a key name has no separate beginning or
end to anchor to: a pattern is a prefix-and-suffix test, not a regular language.

Key names may legally contain `*`, so an escape is required for a client to
watch such a key literally. The escape is a backslash, and it applies only to
`*` and to itself:

```
wm.*        prefix match on "wm."
wm.\*       the exact key "wm.*"
\\          a literal backslash
\*          a literal asterisk
w\*x        the exact key "w*x"
```

Any other `\x` sequence is a literal backslash followed by `x`, which keeps the
rule to one case instead of defining a general escaping grammar. A pattern
containing a backslash as its final character is `WATCH_INVALID`, since there is
nothing for it to escape.

An empty pattern is `WATCH_INVALID`. It is not a wildcard and not a match-nothing
subscription: a client sending one has a bug, and a subscription that silently
never fires is a far worse outcome than a rejected request, because the client
will wait for events that can never arrive. A pattern that is well-formed but
matches no existing key is still accepted, since keys are open-ended and the
pattern may simply be for a key set later.

Key name length is bounded by `OMNI_VALUE_MAX_NAME` from §3.3, and a pattern
longer than that is `WATCH_INVALID` rather than a pattern that can never match.
Characters are otherwise unrestricted: the store accepts any UTF-8 key name, so
restricting pattern characters would restrict which keys are watchable and
nothing else.
- Region lifecycle events expose both identities: `entry_ref` contains
  `epoch`, `entry_id`, and `entry_generation`; `region_ref` contains `epoch`,
  descriptor `id`, and `region_generation`. A request-completion event exposes
  `epoch` and `request_ticket`.
- `time` is the entry's CLOCK_MONOTONIC ns and is not a cursor.
- A grouped commit's entries are delivered in order and are applied by the
  client only as a unit; the socket does not emit a separate marker for
  `COMMIT_END`, and a truncated group at the end of the retained ring is
  reported through the gap machinery rather than delivered partially.

#### Discontinuities

The journal ring is drop-oldest with no backpressure, so a consumer that does
not read at commit rate falls behind. Two discontinuities are reported, and
they are not interchangeable.

`gap_policy = "fail"`, consumer fell behind within the current epoch:

```
{ "ok": false, "error": { "code": "WATCH_GAP",
  "message": "consumer fell behind the journal ring",
  "epoch": 7, "lost_from_seq": 812, "lost_to_seq": 5199 },
  "epoch": 7, "commit_id": 5199, "id": 3 }
```

`lost_from_seq` is the first sequence no longer retained; `lost_to_seq` is the
oldest still available. The subscription ends; re-attach with a cursor to get a
fresh snapshot.

`gap_policy = "resync"`, same condition, reported as data instead:

```
{ "sub": 1, "event": "watch_resync", "epoch": 7,
  "cursor": { "epoch": 7, "journal_seq": 5199 },
  "lost_from_seq": 812, "lost_to_seq": 5199 }
```

The subscription continues from `cursor`, which is the boundary of a
commit-id-stable snapshot rather than the oldest retained sequence. The client
must discard all state derived from the stream and reload from that snapshot,
because the snapshot already contains every entry up to the boundary. This
event is never appended to the journal ring.

An instance restart is always terminal and is never governed by `gap_policy`:

```
{ "ok": false, "error": { "code": "WATCH_EPOCH_CHANGED",
  "cursor_epoch": 6, "current_epoch": 7 },
  "epoch": 7, "commit_id": null, "id": 3 }
```

A gap present at attach time, where `since` is already below the oldest
retained sequence, produces the same `WATCH_GAP` or `watch_resync` result as a
mid-stream gap.

### exec

Invoke a registered action by namespace-qualified name. Bindings reference
actions as strings, resolved via the registry at dispatch time.

```
{ "cmd": "exec", "action": "wm.cycle_layout", "args": ["master-stack"], "id": 4 }
```

- `args` is a JSON array, validated positionally against the action's declared
  schema before the handler runs: wrong count, an undecodable type, an
  out-of-range value, or a malformed composite is `ARGS_INVALID`. The schema
  lives with the action (`helpers.md` §6.1), not here.
- Unknown action: `ACTION_NOT_FOUND`. Wrong arg shape: `ARGS_INVALID`. The action
  ran and failed: `ACTION_FAILED`, which is distinct from both, since otherwise
  a broken action is indistinguishable from a typo.
- `result` is typed by the action's declared result tag. A `0` result tag means
  the action returns nothing and `result` is `null`; there is no untyped result
  case, because a result with no declared type cannot be checked. An action
  that declares a result tag and fails to produce one is `ACTION_FAILED`, not a
  silent `null`.
- An action is not a transaction. An action needing a multi-step atomic change
  uses a grouped `set`; one that commits twice is two observable commits.

### save

Serialise the current configuration. A read-only operation, but still a
snapshot: `save` must observe one commit-id-stable state, not a mixture of
several.

```
{ "cmd": "save", "path": "/tmp/omni.toml", "id": 5 }
-> { "ok": true, "result": { "path": "/tmp/omni.toml", "entries": 214,
                             "skipped": 2 }, "epoch": 7, "commit_id": 99, "id": 5 }
```

- `path` optional; defaults to the config file the WM booted from, or the
  standard path if none.
- The snapshot is taken under the read futex, validated by the L1-L4 guard
  tiers, and the file is then written from that snapshot outside the lock. A
  `commit_id` changing mid-save does not restart or corrupt the save; the
  snapshot already fixed what is being written.
- `entries` is how many were written. `skipped` is how many live entries failed a
  structural tier and were therefore deliberately dropped, per the export policy
  in `configstorage.md` §13. `skipped` is the field that distinguishes a clean
  save from one that quietly lost a key, and it is the reason the response
  exists; the earlier `constructions` and `failed` names described line counts,
  which is not what save does.
- The file is written to a temporary path and renamed into place, so a failed or
  interrupted save cannot leave a truncated config where a working one was.
- Window-dependent and `EPHEMERAL` entries are excluded, per §13 and the
  `save` classification.

### reload

Replay a config file. This is the transaction boundary, and the guarantee is
precise about what it covers.

```
{ "cmd": "reload", "path": "/etc/omni/config.toml", "id": 6 }
-> { "ok": true, "result": { "path": "/etc/omni/config.toml",
                             "applied": 180, "failed": 3 },
     "epoch": 7, "commit_id": 140, "id": 6 }
```

- The file is parsed into a private staging area first, and every staged value
  is validated before anything is published. A file that fails to parse never
  begins a commit, so the live block is untouched by a failed reload.
- On success the whole file is published as one grouped commit with a single
  `commit_id` and a `COMMIT_END`. A reader sees the complete pre-reload state
  or the complete post-reload state, and never a partially reloaded
  configuration. This is the same prepare-then-publish rule in
  `configstorage.md` §1.1 that governs any grouped commit.
- A file exceeding `OMNI_CONFIG_MAX_OPS` is rejected whole with
  `CONFIG_TOO_LARGE`, carrying the operation count that was refused and the
  limit. It is never truncated, because a truncated config is a partially
  applied one and the whole point of the transaction is that there is no such
  state.
- Per-line failures are still tolerated: the rest of the file loads and the
  failing lines are reported as `failed` and logged. The config fails per line,
  never as a whole. The one exception is exceeding `OMNI_CONFIG_MAX_OPS`, which
  is a structural limit and aborts the file.
- **The guarantee covers the block, not the effects.** An `exec` line invokes an
  action, and an action's effects are outside the transaction. A config that runs
  a background script starts the script; a crash or abort after publication does
  not stop it, and the block's atomicity says nothing about what that script goes
  on to do. Ordering is likewise partial: the block applies in file order, and
  each `exec` runs at its line, but an action's side effects are not rolled back
  if a later line fails.
- `wm.reload` is appended as a journal event in the same commit, carrying the
  new `commit_id`, so a client watching for it can confirm the boundary rather
  than infer it.

### delete

Remove one key outright. This is the destructive single-key primitive, and it
is not a reset: `delete` unmakes an entry, while `reset` re-establishes a
configuration state.

```
{ "cmd": "delete", "key": "wm.gaps", "id": 7 }
```

- The entry goes DESTROYED + FREE (configstorelayout.md §6), `KEY_DELETE` is
  appended to the journal, and the new `commit_id` is returned.
- The name, the `entry_generation`, and the frame are gone. A later `set` of the
  same name allocates a new entry identity, so a client holding
  `(entry_id, entry_generation)` must not assume the name still refers to it.
- `WINDOW_DEPENDENT` and `EPHEMERAL` do not protect an entry from an explicit
  `delete`; the flags govern `save` and `reset`, not this command.
- Unknown key: `KEY_NOT_FOUND`.

### reset

Re-establish a configuration state. Two modes, and the mode is always explicit
in the wire form even though `soft` is the default.

```
{ "cmd": "reset", "mode": "soft", "id": 8 }
{ "cmd": "reset", "mode": "hard", "id": 9 }
```

The WM does not know how to put an individual value back. It clears and
rebuilds, and whatever applied the configuration re-applies it. That is what
lets any language drive the WM: restoration is owned by the applier, not by the
store.

Both modes use the same classification, identical to `save`:

```
affected entries   WINDOW_DEPENDENT clear AND EPHEMERAL clear
```

`WINDOW_DEPENDENT` dominates: a per-window override of a configuration option is
never cleared, which is what makes "set generally and for one window" work
across a reset. `EPHEMERAL` entries, such as process bookkeeping, are also left
alone.

Soft reset is reversible:

1. Write the current affected entries to a new temporary file, per `save`
   semantics. The response and the journal event both name the path.
2. Clear the affected entries.
3. Re-run the config that was booted from.

To revert, `reload` that path. The snapshot is a file, not block state, because
a snapshot section would be a block-layout change and the file reuses the
`save` path unchanged.

Hard reset drops and rebuilds the entire shared-memory block from config. The
block is destroyed and recreated, so the `epoch` changes, which is exactly the
signal a process-based applier watches: it polls the `epoch`, and on change it
re-runs its own init function. Soft reset does not change the epoch, so an
applier that only watches the epoch will not notice a soft reset.

#### Scope of each mode

|  | soft | hard |
|---|---|---|
| `epoch` | unchanged | **changes** |
| block | same block survives | destroyed and recreated |
| `WINDOW_DEPENDENT` entries | kept | **gone** |
| `EPHEMERAL` entries | kept | **gone** |
| catalog | affected entries cleared | empty, then re-seeded |
| journal | continues, `wm.reset` appended | **new ring, new `epoch`** |
| every outstanding reference | stays valid | **invalid** |
| reversible | yes, via the snapshot | no |

The `epoch` column is the important one. A soft reset preserves the block, so
every `(entry_id, entry_generation)` a client holds, and every `region_ref`, and
every outstanding `watch` cursor, all remain valid. A hard reset changes the
`epoch`, which invalidates all of them, and clients detect it through the
existing `WATCH_EPOCH_CHANGED` path rather than through anything new. That is
why `epoch` is the whole reset-detection story and no new error code is needed.

Extension entries are ordinary configuration under the `EPHEMERAL` default,
which is the one genuinely sharp edge in soft reset: an extension's `ext.*`
keys are cleared and it must repopulate them from its own `wm.reset` handler.
That is correct, and it is a real obligation on extension authors rather than
something the store can infer, so it is stated rather than hidden.

To make both modes observable to any applier, and not only to the caller, both
append a journal `EVENT` with category `wm.reset`, carrying `mode`, the new
`commit_id`, and for `soft` the snapshot path. An applier watching `wm.reset`
re-runs its init. Without this, soft reset would work only for the built-in
TOML path, because that path happens to be the caller, which is the coupling
this design exists to remove.

- `mode` absent defaults to `soft`, so the destructive form is never reached by
  accident. An unrecognised value is `PARAM_INVALID`.
- A soft reset issued while a window operation is in flight is refused with
  `NOT_READY`. A hard reset is always permitted, because it does not depend on
  block state surviving.

## 5. Readiness

The socket starts serving before every component has finished activating, so
readiness has to be a first-class observable rather than something a client
discovers by watching commands fail.

### 5.1 Readiness is a header field, not a handshake

The block already carries `ready` as a u32 beside the header's `state`
(`configstorelayout.md` §3). The two are separate axes and the separation is
deliberate: `state` answers whether the block is structurally trustworthy,
`ready` answers whether the compositor can service requests against it. A client
can therefore distinguish "the WM is still starting" from "the store is
corrupt" without a round trip, and the two call for entirely different
responses, one of which is to wait.

```
0 NOT_READY   block is valid, compositor is still activating
1 READY       all components activated; requests are serviceable
2 DEGRADED    serving, but a non-critical component failed to activate
3 FAILED      activation failed; the block stays readable but inert
```

`DEGRADED` and `FAILED` are what make a partial start diagnosable rather than
merely absent. A client that only saw a boolean would have to guess whether
"not ready yet" means "wait" or "give up", and the correct action differs.
`FAILED` is also distinct from `state == BROKEN` on purpose: a `FAILED` block is
intact and readable, a `BROKEN` block must be refused outright. Conflating them
would discard a recoverable configuration because a component failed to start.

A direct mapper waits by polling `ready` with a small backoff and never opens a
connection at all, which is the cheapest possible waiter. A socket client has
the same information available without us inventing a blocking handshake, and a
handshake would in any case be inferior here, because it would make waiting
require holding a connection.

### 5.2 Pre-ready command behaviour

`ready` is read at the moment the command is dispatched, not when the connection
was accepted, so a client that connects early and issues commands later is
unaffected by having connected early.

| command | while `NOT_READY` |
|---|---|
| `get`, `watch` | **served**; the block is valid, there is nothing to wait for |
| `unwatch`, `delete` | **served** |
| `set`, `exec` | `NOT_READY`; these may depend on components that are not up |
| `save` | **served**; a read-only serialisation of what exists |
| `reload` | `NOT_READY`; a config references components by name |
| `reset` soft | `NOT_READY`, same as the in-flight window operation rule |
| `reset` hard | **served**; it does not depend on block state |

The split is the useful part. Reads are never refused, because a read cannot
depend on a component that does not exist yet, and refusing them would force a
client to poll for readiness before it could do anything useful. Writes are
refused, because `set` into a key whose owning component has not registered
would silently write a value nobody is reading.

Commands are not queued while `NOT_READY`. Queueing would mean a client cannot
tell whether its `set` was applied, and the failure mode of a silently-dropped
queued command is worse than an explicit `NOT_READY` the client can retry. A
client that wants to wait polls `ready`; a client that wants to react to the
transition can `watch` the `wm.ready` event, which is appended in the commit
that changes the field.

`ready` is written only in a commit, so it is observed atomically with respect
to the journal event announcing it, and a client that sees the event has
necessarily already seen the new field value.

## 6. Session behavior

- One connection: any number of `watch` subscriptions plus ordinary commands.
  Subscriptions are addressed by the server-assigned `sub` and never interact
  with each other.
- Server processes messages strictly in order per connection. Responses and push
  lines share that connection's per-client FIFO write queue, drained on
  writability; there is no backpressure onto the event loop, and no client can
  delay another. See §1.2.
- A message that decodes but hits a guard failure is answered with
  `BAD_VALUE`; the connection stays open.
- On WM shutdown every connected socket is closed before the socket file is
  unlinked; clients see EOF and may reconnect to the new instance (they must
  re-discover via `OMNI_INSTANCE_SIGNATURE`, configstorage.md §10).

## 7. Relationship to other facades

- The built-in TOML parser is a facade in the same sense (in-process, over the
  block, per configstorage.md §11), upstream of `set`/`exec` only. Both reuse the
  same typed-value encoder/decoder defined in §3; `tomlparser.md` binds TOML
  literals to those same tags and adds none of its own.
- A well-written omniWM library never touches this socket: it maps the block
  directly (`configstorage.md` §0).

## 8. Open items

- `OMNI_SOCK_MAX_LINE` (1 MiB) and the send-buffer size are v1 guesses; enlarge
  if the 4K-region/state endpoints ever need more.
- The `modmask` text grammar (`"ctrl+alt"`) and keysym string names need a
  canonical table; deferred until bindings exist (layout/bindings work).
- The `watch` pattern grammar is single-`*` with backslash escaping, fixed in
  §watch. Richer matching, if reload ever needs it, would be a new grammar
  rather than an extension of this one, because a client already relying on `\*`
  being a literal asterisk constrains what any extension may mean.
- The `wm.reset` journal event's category name and its field set are fixed here,
  but the temp-file naming and lifetime for a soft-reset snapshot are not; they
  belong with the transaction and readiness pass.
- `binding` and `rule` wire encodings are unfinished by design. Both depend on the
  action ownership contract, so they are settled in the action ownership pass
  (step 16) rather than guessed here. The same pass owns the `exec` argument
  schema, action result typing, and the runtime failure code for an action that
  fails during execution, none of which are defined yet.
- The value limits in §3.3 are derived from the 4 MiB initial block. A larger
  block would raise `OMNI_VALUE_MAX_FRAMED` and `OMNI_VALUE_MAX_ARRAY_ELEMS`
  proportionally; the ratio, not the absolute numbers, is the contract.
- There is no native plugin ABI and none is planned; extension happens through
  the block, and the reasoning is recorded in `helpers.md` §8.1. An external
  program is not a component, has no `enable_key`, and cannot be suspended,
  unloaded, or given a lifecycle by the WM.
- The socket-path fallback is specified behaviour, but the exact `/tmp` name
  derivation and where the advertised path lives in the discovery section are
  implementation choices left to the store pass; the constraint on them is only
  that the name is stable per block identity and that discovery still works from
  the block alone.