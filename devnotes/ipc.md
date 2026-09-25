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
  (discovery §10 of configstorage.md). Unlinked on WM exit / restart.
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
OMNI_SOCK_SEND_BUF     = 64 KiB    per-client send buffer before write pressure
```

A line longer than the limit yields an error response, then the server closes
that connection. A partial request on EOF is discarded.

## 2. Envelope

Request:

```
{ "cmd": "get|set|watch|unwatch|exec|save|reload|reset", "id": <any>, ... }
```

`id` is echoed verbatim in the response for correlation; optional, may be a
string or number. Responses are exactly one per request, in order:

```
ok      : { "ok": true,  "result": <type-specific>, "seq": <u64>, "id": <any> }
failure : { "ok": false, "error": { "code": "...", "message": "..." }, "id": <any> }
```

`seq` is the generation the read/write landed on, so a client can position a
subsequent `watch`.

Error codes (fixed set):

```
INVALID_JSON    UNKNOWN_CMD     PARAM_INVALID   KEY_NOT_FOUND
BAD_TYPE        BAD_VALUE       CATALOG_FULL    BLOCK_EXHAUSTED
ACTION_NOT_FOUND ARGS_INVALID   WATCH_INVALID   NOT_READY
```

## 3. JSON value encoding (type tags from configstorage.md §4)

Decoder input rules for `set` / `exec` args; encoder output rules for
`get` / `watch`. Both directions are defined per tag group. `set` must carry
`type` explicitly whenever the JSON value is ambiguous.

| tag | JSON in | JSON out |
|---|---|---|
| bool | `true`/`false` | `true`/`false` |
| i8..u64, f32, f64 | number | number |
| duration | number (ms) | number (ms); stored as ns |
| char | string (1 char) | string (1 char) |
| string, font_desc, path, exec, state | string | string |
| blob | base64 string | base64 string |
| keysym | string or number | string (XKB name) |
| keycode | number | number |
| modmask | string `"ctrl+alt"` or array | string `"ctrl+alt"` |
| point, offset, vec2i, size, rect | object `{"x","y"(,w,h)}` | same |
| vec2, vec3, vec4 | object `{"x","y"("z","w")}` | same |
| rgba8, argb8 | `"#rrggbbaa"` or number | `"#rrggbbaa"` |
| rgba32f | object `{"r","g","b","a"}` | same |
| region_ref, entry_ref, shader_ref, cursor_ref | number | number |
| enum | string | string |
| option | null or typed value | null or typed value |
| array | JSON array (element type required on set) | array |
| tuple | JSON array (fixed order) | array |
| binding | object `{"mods","key","cmd"}` | same |
| rule | object `{"match":{},"replace":{}}` | same |
| gradient | array of `{"offset", color}` objects | same |

## 4. Commands

### get

Read the current value of one key from the catalog.

```
{ "cmd": "get", "key": "wm.focus.color", "id": 1 }
-> { "ok": true, "result": { "key": "wm.focus.color", "type": "rgba8",
     "value": "#3d6bff", "length": 4 }, "seq": 43, "id": 1 }
```

Unknown key: `KEY_NOT_FOUND`. `get` uses the open-catalog rule; extension keys
are returned without interpretation beyond typing. A structurally invalid
entry (configstorelayout.md §11 guard bundle) is reported as `BAD_VALUE`.

### set

Single-key commit or grouped commit in one message.

```
{ "cmd": "set", "keys": [ { "key": "wm.gaps", "type": "u32", "value": 8 },
                          { "key": "wm.gaps_enabled", "type": "bool", "value": true } ], "id": 2 }
```

- Without `keys` array, one shot: `{"cmd":"set","key":"...","type":"...","value":...}`.
- Mapped onto commit protocol §12 of configstorelayout.md: single-key = one
  journal entry + one generation bump; grouped = N entries, one bump.
- Unknown keys accepted verbatim, per the open-catalog rule.
- Overwrite-fits rule and arena framing apply ($6 configstorelayout.md).
- Return echoes the new `seq` and each stored key.

### watch

Subscribe to journal-forward stream of events for a key glob.

```
{ "cmd": "watch", "play": "wm.*", "since": 43, "id": 3 }
-> { "ok": true, "result": { "watched": "wm.*" }, "id": 3 }
```

Subsequent lines on the connection, newest-first-forward from `since` (or from
the current cursor if omitted):

```
{ "event": "key_set", "key": "wm.gaps", "type": "u32", "value": 8, "seq": 44, "time": 12345 }
{ "event": "key_delete", "key": "wm.gaps", "seq": 45, "time": 12346 }
{ "event": "custom", "category": "ext.foo.signal", "value": "hello", "seq": 46, "time": 12347 }
```

- `play` is a glob: `*` matches any run of characters. No other wildcards at
  v1; `WATCH_INVALID` otherwise.
- `since` is a journal `seq`; the stream replays recorded entries with
  `seq >= since`, then follows live. Epoch mismatch (configstorage.md §5)
  resynchronizes: the client re-reads the catalog snapshot and the stream
  starts from the current cursor.
- `unwatch` closes the subscription; the connection close also unwatches.
- `time` is the entry's CLOCK_MONOTONIC ns.

### exec

Invoke a registered action by namespace-qualified name. Bindings reference
actions as strings, resolved via the registry at dispatch time.

```
{ "cmd": "exec", "action": "wm.cycle_layout", "args": ["master-stack"], "id": 4 }
```

- `args` is a JSON array; how the action interprets it is the action's
  contract (documented with the action), not the socket's.
- Unknown action: `ACTION_NOT_FOUND`. Wrong arg shape: `ARGS_INVALID`.
- `result` of a successful call is action-defined JSON or `null`.

### save / reload

Save-state semantics are `configstorage.md` §13 verbatim: `save` writes all
non-window-specific keys; window-dependent state is excluded.

```
{ "cmd": "save", "path": "/tmp/omni.toml", "id": 5 }
{ "cmd": "reload", "path": "/etc/omni/config.toml", "id": 6 }
```

- `path` optional; defaults to the config file the WM booted from (or the
  standard path if none).
- `reload` replays the file; partial-invalid config still loads per §13.
- Response reports per-line counts: `{ "constructions": n, "failed": n }`.

### reset

```
{ "cmd": "reset", "key": "wm.gaps", "id": 7 }     // delete one key
{ "cmd": "reset", "id": 8 }                       // back to seeded defaults
```

- With `key`: delete entry (DESTROYED + FREE, configstorelayout.md §6),
  journal `KEY_DELETE` appended, generation bumped.
- Without `key`: full reset allowed only while the WM is idle and reversible
  via `save`; refuse with `NOT_READY` mid-window-operation.

## 5. Session behavior

- One connection: any number of `watch` subscriptions plus ordinary commands.
- Server processes messages strictly in order per connection; responses and
  push lines share the same FIFO write queue (64 KiB buffer, then backpressure
  on the event loop).
- A message that decodes but hits a guard failure is answered with
  `BAD_VALUE`; the connection stays open.
- On WM shutdown every connected socket is closed before the socket file is
  unlinked; clients see EOF and may reconnect to the new instance (they must
  re-discover via `OMNI_INSTANCE_SIGNATURE`, configstorage.md §10).

## 6. Relationship to other facades

- The built-in TOML parser is a facade in the same sense (in-process, over the
  block, per configstorage.md §11), upstream of `set`/`exec` only. Both reuse
  the same typed-value encoder/decoder defined in §3; the TOML design
  (tomlparser.md) will bind TOML literals to the same tag groups.
- A well-written omniWM library never touches this socket: it maps the block
  directly (`configstorage.md` §0).

## 7. Open items

- `OMNI_SOCK_MAX_LINE` (1 MiB) and the send-buffer size are v1 guesses; enlarge
  if the 4K-region/state endpoints ever need more.
- The `modmask` text grammar (`"ctrl+alt"`) and keysym string names need a
  canonical table; deferred until bindings exist (layout/bindings work).
- `watch` glob is single-`*` at v1; revisit if config reload code needs richer
  matching.