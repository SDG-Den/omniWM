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

It appends priority-tagged records to the global registry via an append API.
No linking magic: one fixed table of register functions lives in `core/server.c`,
known at compile time (static registry, v1).

Phase 2, activate: `omni_boot()` is the only entry point that turns records
into live components:

1. stable-sort the registry by `priority`, then by registration order
2. for each record in order: seed options, subscribe triggers, register
   actions, call `init`
3. set `state = ACTIVE` per record; mark block `state = READY` afterwards

`omni_boot()` runs once, after the block store and socket exist.

Stack safety: no dynamic allocation at sort time beyond one working array of
record pointers, and none permitted inside the collect phase (records are
filed as `const` data whenever possible).

## 3. Component record (core/component.{c,h})

```
struct omni_component {
    const char *name;            /* "tags", "decorate.border", "input.dwm" */

    int32_t priority;            /* lower = earlier (ascending) */
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

### Enable key convention

Full key, namespaced: `wm.<component>.enabled`. Default seeded by the option
array (`true`), so a component is on unless a config says otherwise. A
component that genuinely cannot be torn down at runtime declares
`enable_key = NULL` and is always active.

## 4. Toggle engine (core/component.{c,h})

Immediate runtime toggling, no restart:

- After activation, the toggle engine subscribes to the component's
  `enable_key` (and to `wm.*` at most briefly, only for components with
  dependent keys).
- On an `enable_key` commit to `false`: call `suspend()`; the component
  unsubscribes its triggers, unregisters its actions, tears down transient
  state. Record state `SUSPENDED`.
- On `true`: call `resume()`; the component re-registers actions,
  re-subscribes triggers. State `ACTIVE`.
- `destroy()` runs only at shutdown, in reverse priority order.
- The engine is driven synchronously from the commit dispatch (single
  thread), so toggles land on the next relevant commit, never mid-frame.

## 5. Events (core/events.{c,h})

One vocabulary, one source of truth. The block journal is the only event
source; `omni_subscribe()` is an internal watch over the same commit path that
IPC `watch` serializes (ipc.md §4). Triggers listed in a record are
subscribed here at activation.

- `pattern` grammar and matching are identical to IPC `watch` (single `*`
  wildcard); a trigger is the in-process twin of a watch.
- Event struct `omni_event` carries the journal fields: kind, seq, time_ns,
  key/pattern matched, typed value, as-read from the entry.
- Dispatch is synchronous, subscribers in registration order, each guarded
  against reentrancy (a callback committing changes re-enters dispatch one
  nesting level deep, then unwinds).
- Epoch resync (configstorelayout.md §5) applies: a subscriber invalidated by
  epoch change receives a reset notification and re-baselines.

## 6. Actions (core/actions.{c,h})

Namespaced handler registry feeding IPC `exec` (ipc.md §4) and keybindings.

- `omni_action_register(name, handler)` during activation;
  `omni_action_unregister(name)` during suspend.
- Names use the key namespace discipline: `wm.*` core, `ext.<name>.*`
  extensions; conflicts at registration are logged and refuse the later one.
- Dispatch validates the handler exists and the arg shape (typed arg array),
  then calls it. Unknown/arg-mismatch maps to the IPC error codes
  `ACTION_NOT_FOUND` / `ARGS_INVALID`.

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
table, `omni_boot()`, mark block READY, enter the display event loop. Shutdown:
close socket, unlink block, destroy components in reverse priority order.

## 9. Priority convention (v1)

```
0    core (server, store, registry)
10   config facade (TOML, IPC bootstrap)
100  input
200  tags, windows
300  decorate, draw
400  ext-protocol, misc
```

Same priority records activate in registration order (stable sort). Priorities
are advisory; a component sets its own to express its dependency position.

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
- The action arg array and its typed encoding reuse the tag system but the
  exact `omni_args` layout is unresolved; it settles when binds (input) are
  designed.
- Component dependency edges beyond plain priority (a hard "requires X")
  do not exist at v1; priority ordering is the mechanism until a real
  dependency failure appears.