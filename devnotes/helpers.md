# Shared helpers (core library) design

This is the design for `core/`, the component substrate that makes adding new
features to OmniWM cheap. It pairs with `configstorelayout.md` (the block
store) and `ipc.md` (the socket facade). Code split is intentionally finer
than mango: one concern per file, all under `core/`.

## 0. Goals

- New feature = one component: write its file(s), add one register function to
  the table in `server.c`, done. No edits scattered around a monolith.
- Everything a component exposes must reach its three config universals:
  options (config keys), actions (controls), triggers (events). No side door.
- Feature surface is runtime-config-driven, not `#ifdef`-driven (unlike
  dwl/mango): any component can be toggled off and on immediately through its
  config key.

## 1. Core files

```
core/
  registry.{c,h}   global registry, register API, omni_boot sort+activate
  component.{c,h}  the record type, option/action/trigger arrays, toggle engine
  events.{c,h}     journal-backed subscription: pattern -> callback
  actions.{c,h}    namespaced control/action registry + dispatch
  log.{c,h}        logging (mango port) + component-name prefixes
  util.{c,h}       container kit (mango port + additions)
  server.{c,h}     server struct, wl_display lifecycle, register-table, omni_boot
```

Umbrella header `include/commonheaders.h` includes every public core header;
component files include it and nothing else.

## 2. Registry (core/registry.{c,h})

Two-phase activation, per decision.

Phase 1, collect: each component `.c` ships exactly one public function,
placed next to the rest of that component's code:

```
void omni_register_tags(void);   /* example */
```

It appends descriptors to the global registry via an append API. Registration
never runs `init` and never creates an instance; it only records what the
component is. No linking magic: one fixed table of register functions lives in
`core/server.c`, known at compile time (static registry, v1).

Phase 2, activate: `omni_boot()` is the only entry point that turns descriptors
into live instances:

1. stable-sort the registry by `priority`, then by registration order
2. for each descriptor in order: create an instance in state `SUSPENDED`, then
   seed options, subscribe triggers, register actions, call `init`
3. `init` returning failure moves that instance to `FAILED` and records
   `init_errno`; boot continues regardless, because the descriptor still exists
   and the failure is reportable by name
4. after the pass, set the header `ready` field per the rules below

`omni_boot()` runs once, after the block store and socket exist.

### 2.1 Boot outcome and the `critical` flag

A component that fails `init` does not stop the boot, because the alternative is
that one bad extension leaves the user with no WM at all and no diagnosis. What
`critical` decides is the resulting readiness, per `ipc.md` §5.1:

| condition | `ready` | meaning |
|---|---|---|
| every `critical` component activated | `READY` | fully serviceable |
| a non-critical component failed | `DEGRADED` | serving; something is absent |
| a `critical` component failed | `FAILED` | block readable, WM inert |

`FAILED` is deliberately not `BROKEN`. The block is intact and its data is
readable, so a client that discards it on a component-init failure would throw
away a recoverable configuration over an unrelated problem. The WM stays up and
keeps answering reads, which is what makes the condition diagnosable: a client
can read `ready == FAILED`, enumerate the failed components by name, and report
which one to fix.

A `FAILED` instance is terminal for that activation. It is not retried, not
resumed, and its `destroy` is not called, since `init` never completed.
Suspension and resume are transitions out of `ACTIVE` only.

Stack safety: no dynamic allocation at sort time beyond one working array of
descriptor pointers, and none permitted inside the collect phase (descriptors
are filed as `const` data whenever possible).

## 3. Descriptor and instance (core/component.{c,h})

Registration exists so that adding a feature means writing functions and
appending a record, rather than editing code at several points to teach the
machinery about the new feature. Everything after the `register` call is
generic, and nothing in this section is a user-facing concept: a user never
reasons about descriptors, instances, or priority.

### 3.1 Descriptor

`struct omni_component` is the immutable registration record. It is written once
at registration and never modified, and it may be `const` throughout:

```
struct omni_component {
    const char *name;            /* "tags", "decorate.border", "input.dwm" */

    int32_t priority;            /* lower = earlier (ascending) */
    uint8_t critical;            /* 1 = boot fails if init fails */
    const char *enable_key;      /* full config key, e.g. "wm.tags.enabled";
                                    NULL = always on */

    /* options: zero or more typed keys seeded into the block on activation */
    const struct omni_option  *options;  size_t n_options;

    /* actions: named controls resolved by IPC exec and bindings */
    const struct omni_action  *actions;  size_t n_actions;

    /* triggers: journal subscription patterns for this component */
    const struct omni_trigger *triggers; size_t n_triggers;

    void (*init)(void);
    void (*destroy)(void);       /* shutdown only */
    void (*suspend)(void);       /* runtime-off: unsubscribe, unregister */
    void (*resume)(void);        /* runtime-on: re-subscribe, re-register */
};
```

The descriptor holds no mutable state at all. There is no state field, no error
field, and no userdata on it, which is the entire reason it is a separate
record: identity, options, and callback pointers cannot be observed half-updated
because they are never written after registration. A client holding a descriptor
pointer may trust it for the lifetime of the process.

`priority` is advisory and is not a dependency edge. It expresses where a
component prefers to sit in activation order and nothing more, so there is no
cycle check, no topological sort, and no failure cascade. The consequence is
stated so it is not mistaken for a guarantee: a component that activates before
something it implicitly needs is a bug in that component's chosen priority, and
it surfaces as an init failure rather than being caught by the registry. The
registry does not infer dependencies from option or trigger references.

### 3.2 Instance

`struct omni_instance` is the mutable runtime record, one per activation:

```
struct omni_instance {
    const struct omni_component *descriptor;  /* borrowed, never owned */
    enum omni_instance_state     state;
    const char                  *owner;       /* diagnostic, not enforced */
    intptr_t                     init_errno;  /* meaningful only in FAILED */
    uint8_t                      initialised; /* init() returned */
    uint8_t                      suspended;   /* suspend() returned */
};
```

```
SUSPENDED   created by omni_boot(), not yet initialised
ACTIVE      init() succeeded
FAILED      init() failed; terminal for this activation
SUSPENDED   suspend() succeeded, from ACTIVE
ACTIVE      resume() succeeded, from SUSPENDED
```

The state lives here, never on the descriptor, so reading a component's state
cannot race or observe a partially updated registration record.

Exactly one instance exists per activation, and activation means the WM starting
up. The instance is created by `omni_boot()` and lives until the WM closes out
entirely, whether that is a crash, the user quitting, or the device shutting
down. None of those are suspend/resume targets, so none of them tear down an
instance early. A toggle moves the instance between `ACTIVE` and `SUSPENDED` and
never destroys it: `init` does not re-run on resume, because the configuration
the user did not change is still the configuration the instance was built from.

`struct omni_option`:

```
const char *key;        /* full public key: "wm.gaps" */
uint16_t    type;       /* type tag from configstorelayout.md §2/§4 */
const void *value;      /* native default per type */
```

`struct omni_action`:

```
const char *name;       /* "wm.cycle_layout"; must be namespaced */
void (*handler)(const struct omni_args *args);
```

`struct omni_trigger`:

```
const char *pattern;    /* "wm.*" or a category; same grammar as IPC watch */
void (*callback)(const struct omni_event *event);
```

The option/action/trigger arrays are the component's config reference: the
keys and action names listed are exactly what users see in TOML, IPC, and
foreign-WM configs. `name` is identity only, never a key prefix.

### 3.3 Enable key convention

Full key, namespaced: `wm.<component>.enabled`. Default seeded by the option
array (`true`), so a component is on unless a config says otherwise. A
component that genuinely cannot be torn down at runtime declares
`enable_key = NULL` and is always active.

### 3.4 When a component suspends

A component suspends on exactly one trigger: a write of `false` to its
`enable_key`. Three routes reach that write and they are the same event. A
config file can set it, so a component configured off never initialises. A
`reload` can change it, so toggling is a config edit. A `set` over the socket or
the block can change it live, which is what the toggle engine exists for.

Suspension is a built-in-component convenience and nothing else. Its purpose is
narrow: release expensive resources such as surfaces, subscriptions, and threads
without restarting the WM and without losing the instance's identity or its
accumulated state.

An external program is not a component, has no `enable_key`, and cannot be
suspended by anything. It exists for the WM's lifetime or it does not, and
releasing its own resources is its own decision. Suspension is therefore not the
unloading mechanism for extensions, and §6.3 is where that decision is recorded.

## 4. Toggle engine (core/component.{c,h})

Immediate runtime toggling, no restart. A toggle changes the state of the one
long-lived instance; it never creates or destroys one, and never re-runs `init`.

- After activation, the toggle engine subscribes to the component's
  `enable_key` (and to `wm.*` at most briefly, only for components with
  dependent keys).
- On an `enable_key` commit to `false`, from `ACTIVE`: call `suspend()`; the
  component unsubscribes its triggers, unregisters its actions, tears down
  transient state. Record state `SUSPENDED`.
- On `true`, from `SUSPENDED`: call `resume()`; the component re-registers
  actions, re-subscribes triggers. Record state `ACTIVE`.
- A toggle arriving for an instance in `SUSPENDED` (setting `false` again) or
  `FAILED` is a no-op and does not call either callback. A toggle setting
  `resume` on a `FAILED` instance is refused rather than attempted, since
  `init` never completed and there is nothing to resume from.
- Idempotence follows from the state checks above: `suspend` runs at most once
  per activation and `resume` at most once per suspension, so a repeated commit
  with the same value cannot double-subscribe or double-unregister.
- `destroy()` runs only at shutdown, in reverse priority order, and only for
  instances that reached `ACTIVE`.
- The engine is driven synchronously from the commit dispatch (single
  thread), so toggles land on the next relevant commit, never mid-frame.

### 4.1 Dispatch is single-threaded, so destruction needs no barrier

Every dispatch is a call on the compositor's own event loop. A shutdown request
is therefore queued like any other work and runs at a point where no callback is
in flight, because nothing else is executing concurrently. There is no
callback refcount, no quiescence protocol, and no atomic state machine, and
`destroy` cannot observe a half-torn-down peer.

The one rule this places on components is that a callback must not block waiting
for another thread. A callback that waits for a thread which in turn waits for
the event loop deadlocks the compositor, and no amount of lifecycle
documentation prevents it.

`exec` handing off to an external program is not an exception to this. The
spawned process is outside the compositor's ownership: the contract is that it
either outlives the WM or is killed explicitly during shutdown, and the
compositor does not track its callbacks. A component that starts a thread for
its own work is responsible for joining it inside `destroy` or `suspend`.

## 5. Events (core/events.{c,h})

One vocabulary, one source of truth. The block journal is the only event
source; `omni_subscribe()` is an internal watch over the same commit path that
IPC `watch` serializes (ipc.md §4). Triggers listed in a record are
subscribed here at activation.

- `pattern` grammar and matching are identical to IPC `watch` (single `*`
  wildcard); a trigger is the in-process twin of a watch.
- `omni_subscribe()` takes the same `gap_policy` argument as IPC `watch`
  (`configstorage.md` §5.2). A trigger is not exempt from gap detection just
  because it runs in-process and therefore never pays socket latency; a
  subscriber that blocks, allocates, or throws away events still falls behind.
- Event struct `omni_event` carries the journal identity fields: `epoch`,
  `commit_id`, `journal_seq`, `entry_id`, `entry_generation`, kind, `time_ns`,
  key or pattern matched, and the typed value as read from the entry.
- Dispatch is synchronous, subscribers in registration order, each guarded
  against reentrancy (a callback committing changes re-enters dispatch one
  nesting level deep, then unwinds).
- `omni_subscribe()` returns an `omni_subscription` handle, and
  `omni_unsubscribe(omni_subscription)` is the only removal path. Unsubscribing
  by pattern is not offered: it has no defined referent when a component
  subscribes twice to the same pattern or subscribes the same pattern another
  component also uses. The pattern is what dispatch matches on; the handle is
  what lifetime is tracked by; they are separate structures for that reason.
- Duplicate patterns are allowed and each subscription receives the event. This
  is the same rule as IPC `watch` (ipc.md §4): a subscription's output depends
  only on its own pattern, never on what else is subscribed, so a component
  cannot have its stream altered by an unrelated component registering the same
  pattern.
- Replay rules apply to in-process subscribers exactly as to socket clients: no
  partial group is dispatched without its `COMMIT_END`, and an entry whose
  `entry_generation` no longer matches the live entry is not dispatched.
- Epoch resync (configstorage.md §5.3) applies: a subscriber invalidated by
  epoch change receives a reset notification and re-baselines, and this
  terminal condition is reported even when `gap_policy` is `resync`.
- A `resync` subscriber re-baselines from the snapshot and its callback must
  discard state derived from the stream; the helper performs the reset before
  re-dispatch, so a callback never observes a partially reset state.
- Reentrancy is safe because a callback receives values already resolved from
  the journal entry, never a pointer into live store state. An inner commit
  therefore cannot invalidate an outer callback's inputs, since those inputs
  were resolved before the outer callback was entered. The nesting guard is the
  second half of the rule, catching a callback that re-enters itself by
  committing. A callback that resolves an argument lazily, by reading the store
  during its own execution, has opted out of this guarantee and may observe its
  own commit's effect.

## 6. Actions (core/actions.{c,h})

Namespaced handler registry feeding IPC `exec` (ipc.md §4) and keybindings.

- `omni_action_register(name, handler, context)` during activation returns an
  `omni_action_handle`, and `omni_action_release(handle)` during suspend is the
  only removal path. Unregistering by name is not offered, for the same reason
  `omni_unsubscribe` takes a handle: a name does not identify a registration
  uniquely once two components can register the same name or a component has
  already suspended. The name is dispatch metadata; the handle is lifetime
  identity.
- `context` is a `struct omni_action_context` carrying the server and the owning
  component's descriptor and instance, passed to the handler on every call.
  Handlers do not reach for server globals, so a handler's dependencies are
  visible at its definition and a descriptor cannot be used after its component
  is gone without that being evident.
- Names use the key namespace discipline: `wm.*` core, `ext.<name>.*`
  extensions. The first registration of a name wins; a later one is refused and
  logged, and returns a null handle. The component learns at registration that
  its action is unreachable, rather than at first dispatch.
- Dispatch validates the handler exists and the arg shape (typed arg array),
  then calls it. Unknown/arg-mismatch maps to the IPC error codes
  `ACTION_NOT_FOUND` / `ARGS_INVALID`.
- An action that fails during execution reports `ACTION_FAILED`, distinct from
  `ACTION_NOT_FOUND` (no such action) and `ARGS_INVALID` (the arguments did not
  match the schema). The distinction is what lets a client tell "you asked for
  something that does not exist" from "the thing you asked for went wrong", and
  collapsing them would make a broken action indistinguishable from a typo.

### 6.1 Argument schema and result

An action declares its arguments as typed tags, and the declaration is part of
the registration rather than documentation a caller has to read. The schema is
positional: argument 0 has the tag the action declared for position 0.

```
struct omni_action_schema {
    uint16_t           n_args;
    const uint16_t    *arg_tags;    /* one type tag per position */
    const char        *result_tag;  /* type tag, or 0 for "no typed result" */
};
```

`omni_args` is a resolved argument array: each element carries the declared tag
and a value read from the request, and the array is fully decoded and type-checked
before the handler is entered. This is the concrete form of the reentrancy rule
in §5, and it is also what makes argument validation a dispatch-time concern
rather than a handler concern: a handler never inspects a type tag, because by
the time it runs, every argument already has the type it declared.

Validation rules, all mapped to `ARGS_INVALID`:

- wrong `n_args` for the action;
- an argument whose JSON type cannot decode to the declared tag;
- an argument outside a declared range, such as a `percent` above 1;
- a malformed composite, such as a `tuple` whose `value` length differs from its
  `field_types` (ipc.md §3.2).

A result is typed by `result_tag`. A `0` result tag means the action has no
result and the response carries `result: null`; there is no "arbitrary JSON"
case, because a result with no declared type cannot be checked and a client
would have to guess. An action with a result tag that fails to produce one is
`ACTION_FAILED`, not a silent `null`, so a broken action is distinguishable from
one that has no result.

`result: null` and `ok: true` are therefore different messages: the first means
this action returns nothing, the second is not a state that exists. Every
successful response still carries `epoch` and `commit_id` per ipc.md §2, and an
action's side effects are ordered by the commit that dispatched it, so a client
that changes the store and then execs an action sees the action run after the
change is published.

`ACTION_FAILED` is a runtime failure of the action's own logic. It is not a
store error, so it does not imply `STORE_BROKEN`, and it does not make the block
unreadable. An action that fails must leave the store as it found it; an action
that needs a multi-step change to be atomic must use a grouped `set` rather than
committing twice, since a single action call is not a transaction.

### 6.2 Binding and rule encodings

These two wire forms were left open by ipc.md §3.2 because they depend on the
action contract, and they are settled here.

A `binding` is a 24-byte header plus an argument array in the block
(`configstorage.md` §4), and the wire form carries the action by name rather than
by handle:

```
{ "type": "binding", "value": { "mods": "ctrl+alt",
                                "key": "Shift+Return",
                                "action": "wm.cycle_layout",
                                "args": ["alpha"] } }
```

`args` is optional and positional, and when it is absent the binding has none:
`args_ref` is `OMNI_REF_NONE` and the stored payload is the header alone. An
absent `args` and an empty `args` are the same value, because a binding whose
action takes no argument and one whose action was given nothing are not
distinguishable at dispatch.

`action_ref` in the block is the u32 handle, and it is resolved through the same
registry lookup on the IPC and in-process paths. A stored handle whose
registration is gone is reported as `ACTION_NOT_FOUND` on use, never dispatched
to nothing.

The argument array is where `wm.cycle_layout` finally gets its layout name, and
it removes the reason §6.1's positional schema was previously unusable from a
keybind. A snap can be one action taking a direction
(`wm.snap left`) rather than four actions, and a layout can be selected by
binding rather than only by a cycle command, so the parameter count stops being a
function of the number of directions. Arguments are validated against the
action's schema from §6.1 at decode time and at dispatch time, because the
schema is a property of the action and an action can be replaced by an
extension between the two.

`rule` is a match-and-replace record, and both halves are typed key patterns
rather than arbitrary values:

```
{ "type": "rule", "value": { "match":   { "key": "wm.*", "type": "u32" },
                             "replace": { "type": "u32", "value": 8 } } }
```

`match` selects keys by pattern and optionally by type, both optional. `replace`
is a full typed value, not a patch, so a rule cannot produce a value whose type
differs from the one it matched. A `replace` whose type differs from the
`match` type constraint is `BAD_TYPE`; a rule that matches nothing is stored and
simply never fires, because rules are authored against keys that may not exist
yet.

`rule` does not cover the layout set's client rules, and this is deliberate
rather than an omission. `rule` matches a key and replaces a value; moving a
client into a group, relocating it, or clustering it on attach all match a
*client* and change *membership*, so they are `client_rule` records at `0x33`
(`configstorage.md` §4):

```
{ "type": "client_rule",
  "value": { "match": { "appid": "org.mozilla.firefox" },
             "effect": "join_group", "target": "layout.browser", "scope": "always" } }
```

One wire form per tag, because one tag carrying two schemas means a reader
dispatches on a field it previously assumed was fixed.

### 6.3 Extension boundary

There is no in-process extension interface. An external program is not a
component, is not registered, and cannot be suspended or unloaded; it writes to
the block and observes the journal like any other client. See §8.1 for the
decision and the reasoning.

## 7. Log (core/log) and util (core/util)

Ports per the port rule, fresh prefixes.

- `log`: mango `log.c` conventions (WLR levels, file/line prefix on error)
  plus a component-name tag: `[tags] [input] [decorate]`. Level set by a
  block key `wm.log.level` at boot.
- `util`: mango `util.c` patterns (string_printf, monotonic clock) extended
  with the container kit components reach for: dynamic array, hash map,
  linked list, string builder, ring buffer. No component writes its own
  container.

## 8. Server (core/server.{c,h})

Owns lifecycle and produces the ordering the registry relies on:

```
struct omni_server {
    struct wl_display *display;
    struct wl_event_loop *loop;
    void *block;                /* config store instance */
    int sock_fd;                /* socket facade fd */
    struct omni_registry *registry;
};
```

Boot order: create store (seed `wm.*`), start socket, run the register-function
table, `omni_boot()`, set the header `ready` field per §2.1, enter the display
event loop. Shutdown: close socket, unlink block, then `destroy` components in
reverse priority order, calling it only for instances that reached `ACTIVE`.
The instance records are released last, after every `destroy` has returned, so
no callback can observe a freed instance.

`server.md` is the authority for all of the above. This section records where the
struct is declared and the shutdown note about instance release; the boot order,
readiness mapping, dispatch model, and shutdown sequence are defined there and
are not restated, so the two documents cannot drift.

The server is owned by `core/server.c` and handed to components and handlers as
a `struct omni_server *` inside their action or event context, never as a
reachable global. Ownership is a single owner with typed borrows: the store
belongs to the server for the process lifetime, and a component that wants a
region goes through the request queue (`configstorage.md` §8) rather than
mutating pool state, so no component ever owns a block resource directly.

### 8.1 No native plugin ABI, and that is a decision

There is no host API, no ABI version, no capability negotiation, no load or
unload boundary, and no third-party component that links into the compositor
process. The registration table in `core/server.c` is static and
compile-time-known, and a feature is added by writing functions and appending a
descriptor to that table.

This is not a deferral pending a plugin system. It is rejected, because the
project's central design goal is that the compositor is driven generically
through the block, and exposing a host API for code to hook into the compositor
runs counter to that goal. It would create a second, privileged way to be a
component, with a privileged way to see the server, in a system whose premise is
that there is one unprivileged way: the shared-memory store.

**How omniWM extends instead.** A program written in any language, using any
parser or library, interacts with the block and thereby drives the compositor.
That is the extension mechanism, and it needs no support from the component
model: the store already accepts unknown keys verbatim, the journal already
carries custom events, and `omni_event` already dispatches by pattern. A program
that wants to set custom keys, or to communicate with other programs that are
reading the same block, writes to the block like anything else. It gets no
in-process callback, no server pointer, and no privilege, and it does not need
them.

The trade is stated rather than hidden. A third-party program cannot be called
in-process, so it communicates by writing to the block and observing the
journal, and it cannot be suspended, unloaded, or given a lifecycle by the WM
(§3.4). In exchange, it works in any language, survives a WM restart, and needs
no build-time coupling to the compositor. The store is the stable interface; the
process boundary is the isolation.

Revisiting this would need a component whose behaviour cannot be expressed as
data and actions, which is a property nothing in the current design has. A
capability that needs in-process access is the signal to reconsider, and it
would be a new design step rather than an extension of this one.

## 9. Priority convention (v1)

```
0    core (server, store, registry)
10   config facade (TOML, IPC bootstrap)
100  input
200  tags, windows
300  decorate, draw
400  ext-protocol, misc
```

Same priority descriptors activate in registration order (stable sort).
Priorities are advisory; a component sets its own to express its dependency
position. A component activating before something it implicitly needs is a bug
in that priority choice and surfaces as an init failure, not as a detected
dependency violation, because the registry does not infer dependencies from
option or trigger references. This is deliberate: components are internal, and a
user never reasons about activation order.

## 10. Port vs fresh

Port from mango wherever goals don't differ: `log.c`, `util.c`, server
event-loop/wl_signal conventions, later input and client-management patterns.
Fresh where omniWM diverges: `registry`/`component`/`events`/`actions` (mango
has no equivalents), everything touching the store (`config/`), and any
component reading config keys instead of `#ifdef`d options.

## 11. Open items

- `omni_event` field set is fixed by the journal slots (configstorelayout.md
  §8); richer in-process payloads need a follow-up design if components want
  them (metadata beyond journal entries).
- The action argument schema and result typing are fixed in §6.1: positional
  typed tags plus one declared result tag. What is still open is the `binding`
  key-to-action binding path for the input subsystem, which consumes the
  `binding` type this step defines but is not yet designed.
- The `modmask` text grammar (`"ctrl+alt"`) and the keysym name table are still
  open, and they are what a keybinding author actually types, so they belong
  with the input design rather than here.
- Component dependency edges beyond plain priority (a hard "requires X")
  do not exist at v1, and are not planned; priority ordering is the mechanism
  permanently unless a real dependency failure appears. The observable
  consequence is documented in §9 so it is not read as an oversight.
- The `owner` field on an instance is diagnostic only. Nothing enforces it and
  no access check consults it; it exists so a log line can name a component
  without a registry lookup. A component that wants access control has to use
  the namespace convention in `configstorage.md` §3, which is convention too.
