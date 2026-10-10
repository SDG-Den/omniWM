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
  registry.{c,h}   global registry, register API, omni_boot sort+activate body
  component.{c,h}  the record type, option/action/trigger arrays, toggle engine
  events.{c,h}     journal-backed subscription: pattern -> callback
  actions.{c,h}    namespaced control/action registry + dispatch
  log.{c,h}        logging (mango port) + component-name prefixes
  util.{c,h}       container kit (mango port + additions)
  server.{c,h}     server struct, wl_display lifecycle, register-table, omni_boot call
```

Umbrella header `include/commonheaders.h` includes every public core header;
component files include it and nothing else.

### 1.1 Public and private headers, and a symbol two components need (D6)

`filestructure.md` fixes the directories (`include/<dir>` mirrors `src/<dir>`);
this fixes what goes in them. It also makes "and nothing else" above precise.

- **A header is public when another component's translation unit includes it.**
  A header under `include/<dir>` is the public surface of that subsystem. A
  declaration only its own file uses does not get a header at all; it is
  `static` in the `.c` file. A declaration shared by the several `.c` files of
  one component goes in that component's `include/<dir>` header, because those
  files include it by the same rule anyone else does and there is no third place.
- **A header under `src/<dir>` is private to that directory and is never
  included across a boundary.** The mirror gives a private header an obvious home
  instead of forcing it into a `.c` file when it grows.
- **A symbol two components need is a core symbol.** It moves down into
  `src/core` and is declared in the `include/core` header that owns its concern,
  which is why `commonheaders.h` includes every public core header and why that
  umbrella exists at all. The rule is ownership, not convenience: `core` is the
  ground layer that owns the lifecycle every component runs within, so a shared
  need either moves down or is two implementations that are genuinely not shared.
  No feature component becomes a dependency of `core`, and two feature components
  never include each other's headers.

So "and nothing else" above means: a component file includes
`include/commonheaders.h` plus its own component's `include/<dir>` headers, and
nothing from another subsystem's surface. A symbol another component must include
has, by the rule, already moved to `core`.

## 2. Registry (core/registry.{c,h})

Two-phase activation, per decision.

`registry.{c,h}` is the registry itself: the append API, the descriptor array,
and the sort-and-activate body. `component-registry.h` is a thin header, not a
second registry: it includes every component's public header, so each
component's register function is declared in one place and the fixed table in
`core/server.c` can name the whole set without a per-file edit to the server. A
component's own code still lives entirely in its own files; the thin header is
only what makes the set visible.

Phase 1, collect: each component `.c` ships exactly one public function,
placed next to the rest of that component's code:

```
void omni_register_tags(void);   /* example */
```

It appends descriptors to the global registry via an append API. Registration
never runs `init` and never creates an instance; it only records what the
component is. No linking magic: one fixed table of register functions lives in
`core/server.c`, known at compile time (static registry, v1).

`omni_boot()` has one home. The sort-and-activate sequence below is its body and
lives in `core/registry.c`, because activating the registry is the registry's
own work; `core/server.c` owns the fixed table of register functions and calls
`omni_boot()` as `server.md` §4 step 4. An earlier draft of this section
assigned `omni_boot` to both files, which is the same two-homes problem
`server.md` §1 names for the server struct.

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

### 2.2 The register-function set

The collect phase is where every registration call runs, and the set below is
the machinery a component uses to make itself known. A component may make
several of these calls, and a registration made in one component's file is
still the component's own: nothing here is a global side door.

The partition that matters is which calls return a handle. **Only actions and
triggers return a releasable handle**, because they are the two registrations a
suspended component releases. Everything else is process-lifetime static,
because components do not unload (`§3.4`, `§6.3`).

| call | purpose | lifetime |
|---|---|---|
| `omni_register_<component>(void)` | the collect entry: appends the descriptor, never runs `init` | static |
| `omni_option_register(key, type_tag, default)` | one component option, added to the descriptor's array | static |
| `omni_action_register(name, handler, context, schema) -> handle` | a named control (`§6`) | handle |
| `omni_trigger_register(pattern, gap_policy, callback) -> handle` | a journal subscription (`§5`) | handle |
| `omni_register_init(fn, priority)` | a startup step that runs at `init` time, distinct from the descriptor's own `init` | static |
| `omni_rule_field_register(name, type_tag, read_fn)` | a window-rule match field (`windows.md` §9) | static |
| `omni_rule_property_register(name, type_tag, apply_fn)` | a window-rule target property | static |
| `omni_layout_register(name, program)` | a layout the engine may select (`layoutengine.md` §3.4) | static |
| `omni_node_type_register(type_id, properties, draw_fn, pass_fn)` | a scenefx node type and its pass | static |
| `omni_decoration_register(name, ...)` and its lower-level variants | decoration pieces registered within scenefx and omniWM's own extensions | static |
| `omni_effect_register(name, primitive_ids[], params_schema)` | a preset mapping primitives onto one config key | static |
| `omni_shader_register(name, glsl_source, kind)` | a GLSL source a preset may name | static |
| `omni_animation_register(name, property, curve_or_timeline, property_source_fn)` | an animatable property binding | static |
| `omni_render_layer_register(layer, ...)` | an above-background or above-windows render buffer | static |
| `omni_protocol_register(name, interface, bind_fn)` | a Wayland protocol the server instantiates (`server.md` §9) | static |
| `omni_ipc_method_register(name, handler, schema)` | a socket command verb (`ipc.md` §4) | static |

Two entries are deliberately absent. **There is no register for a window-rule
operator**, because the operator set is fixed and the field and property
registrations are the extension point. **There is no gesture register**: a
gesture collapses into a trigger plus an action for now, so the input layer
recognises it and dispatches an action it names, and the trigger carries the
event. A dedicated gesture kind is not added.

**Decoration is exposed low-level.** `omni_decoration_register` and its
lower-level variants register pieces within scenefx and within omniWM's own
extensions to it, so a component can add a decoration the built-in presets do
not cover. The higher-level preset form is built on those primitives rather than
replacing them, which is the same toolkit shape `build.md` §4 states for the
render layer as a whole.

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
    const char *enable_key;      /* full config key, e.g. "omniwm.tags.enabled";
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
const char *key;        /* full public key: "omniwm.gaps" */
uint16_t    type;       /* type tag from configstorelayout.md §2/§4 */
const void *value;      /* native default per type */
```

`struct omni_action`:

```
const char *name;       /* "omniwm.cycle_layout"; must be namespaced */
void (*handler)(const struct omni_args *args);
```

`struct omni_trigger`:

```
const char *pattern;    /* "omniwm.*" or a category; same grammar as IPC watch */
void (*callback)(const struct omni_event *event);
```

The option/action/trigger arrays are the component's config reference: the
keys and action names listed are exactly what users see in TOML, IPC, and
foreign-WM configs. `name` is identity only, never a key prefix.

### 3.3 Enable key convention

Full key, namespaced: `omniwm.<component>.enabled`. Default seeded by the option
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

### 3.5 Where a component's fields live (D1, D2)

Every component answers the same question about every field it holds, and the
answer has to be the same each time or `WINDOW_DEPENDENT` scope and the `save`
exclusion test start disagreeing with each other. The rule has three homes, and a
field is in exactly one of them.

- **The descriptor** holds what is immutable and known at registration: identity,
  the option, action and trigger arrays, and the callback pointers (§3.1). It
  holds no mutable state, so a field that changes after registration is not a
  descriptor field even when it looks like configuration.
- **The instance** holds mutable, process-local runtime state: the activation
  state, whether `init` returned, whether `suspend` returned, and the error code
  from a failed init (§3.2). This is memory the process owns and the block never
  sees. A private derived cache (§5.1) is process memory too, for the same reason:
  it is a performance structure, not configuration.
- **The block** holds a field only when it is either user configuration or state
  that must be observed by someone outside the process. The vocabulary is the
  store's, not the component's: an ordinary entry is configuration, `EPHEMERAL`
  marks bookkeeping that `save` and reset both skip, and `WINDOW_DEPENDENT` marks
  per-window state that is observable but never saved (`configstorage.md` §3 and
  §12.2, whose scope bits apply only to a live entry).

The dividing question is one sentence: **is this field part of the user's
configuration, or is it the component's own working memory?** Configuration and
the state a user can observe go in the block, marked for scope if they are not
ordinary; everything else is process memory and never touches the block. A
component that wants its transient state observable publishes it as a value on
purpose; a component that has not is not, by default, writing it anywhere the
block can see.

Two consequences follow, and they are the point of writing the rule down:

- **A field does not move between homes when it becomes convenient.** A value
  that is configuration is in the block from the start, even before anything
  reads it live, and a value that is runtime state is in the instance even when a
  test would find it easier in the block.
- **`WINDOW_DEPENDENT` is a claim, not a tag of convenience.** Setting it is what
  tells `save` to skip a value, so a component that sets it to avoid writing a
  field it should have kept has a `save` output that is silently incomplete. The
  exclusion test is `WINDOW_DEPENDENT clear AND EPHEMERAL clear`
  (`configstorage.md` §13), and it is the same test a soft reset uses, so a wrong
  scope bit is wrong in two operations at once.

`configstorage.md`'s scope taxonomy is the vocabulary; this is how a component
uses it, and the identity fields (`configstorage.md` §0.1) are what a component
stores instead of a pointer when it needs to name another entry.

### 3.6 How an option's behavior attaches

An option is declaration only: `{key, type, default}` (`§3.1`). It carries no
callback, and there is no `on_change` field, because a per-option callback would
be a second dispatch path and `§5` forbids a synthetic in-process event. Behavior
attaches through the existing machinery, never through a field on the option:

- If the key is the component's `enable_key`, the toggle engine (`§4`) drives
  `init`, `suspend` and `resume` from it.
- Every other key is acted on by a trigger subscription. The callback re-reads
  the key and applies it, which is the ordinary read-and-react pattern of `§5.1`
  rather than a special option path.

That is why the option array is enough on its own: registration declares *what*
the key is, and a subscription decides what happens when it changes.

## 4. Toggle engine (core/component.{c,h})

Immediate runtime toggling, no restart. A toggle changes the state of the one
long-lived instance; it never creates or destroys one, and never re-runs `init`.

- After activation, the toggle engine subscribes to the component's
  `enable_key` (and to `omniwm.*` at most briefly, only for components with
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
  key or pattern matched, and the typed value as read from the entry. **The field
  set is final.** It was an open item whether to extend it for richer in-process
  payloads, and the decision is not to: the struct mirrors one journal slot, and
  anything a journal slot cannot express has no business arriving through a
  trigger, because a trigger is a view of the journal rather than a second event
  channel. A component that needs metadata the journal does not carry gets it from
  the block, which is where the same data already lives and where reading it costs
  a guarded lookup instead of a struct that has to stay in step with the slot
  layout. The one thing this rules out is a synthetic in-process event, and that
  is the intended outcome: a dispatch that could deliver something no commit
  produced would break the guarantee below that no partial group is ever
  delivered.

  The two journal concepts with no field in the struct are handled by not being
  delivered through it rather than by widening it. A `COMMIT_END` is a group
  boundary the dispatcher enforces before it calls anything, so a subscriber never
  needs to see one, and a custom `EVENT` entry arrives as a key/pattern match on
  its category with the typed value attached, which is the same shape as any other
  match.
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

### 5.1 The read and subscription pattern every component copies (D3, D4)

Two halves, written together here because every component does both and no
document stated the composition. §5 above is the subscription mechanism and
`configstorage.md` §3.1 is the read mechanism; what was missing was the pattern
that puts them together, and it has to be stated here before a component can cite
it.

```
resolve once:    name -> hash -> catalog index -> (entry_id, entry_generation)
cache privately: keep the resolved value in the component's own process memory
per frame:       if the private copy's commit_id is unchanged, use it
on a change:     invalidate on the journal event for that entry, re-resolve
```

Four properties are load-bearing, and each is a decision rather than a style:

- **Resolution goes through the catalog name index, never a catalog walk.**
  `configstorage.md` §3.1 removed the walk behind `get` by design, so a component
  reads through the index or through its own private-copy refresh, and neither
  degrades to a scan whose cost scales with how many keys the user has configured.
- **The cache is process-private, and it is refreshed rather than polled.**
  `configstorage.md` §3.1 states the cost it removes: forty options read every
  frame become forty `u64` compares and zero hash lookups.
- **A frame offset is never cached.** An arena frame can be recycled; the private
  copy holds a copied value or a resolved `(entry_id, entry_generation)`, both of
  which survive a commit, and a raw offset does not.
- **Invalidation is a journal event, and it is the same event for every
  component.** An ordinary cached value invalidates on `KEY_SET` for its
  `entry_id`. A derived structure invalidates on `BINDS_UPDATED`
  (`configstorage.md` §3.1), because it is invalidated by the set changing rather
  than by any single entry. The subscription is §5's `omni_subscribe`, and the
  callback is what refreshes the private copy.

The worked example this pattern was waiting for is `input`, which is the
component with the most reads: `generaldesign.md` §14.4's binding index is a
derived structure refreshed on `BINDS_UPDATED`, and it is the first real instance
of the pattern rather than a citation of it. Writing that instance is a change to
`input.md` in `03-input.md`, not a second statement of the pattern, which is why
the pattern is stated here first.

**D4's remaining half is deliberately small.** A component that wants more than a
journal entry reads the block, because there is no richer in-process payload and
no synthetic event (§5). Subscription is a pattern and not a schema, and this is
the consequence of `omni_event`'s field set being final.

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
- Names use the key namespace discipline: an action name is an `omniwm.*` key.
  The first registration of a name wins; a later one is refused and
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
                                "action": "omniwm.cycle_layout",
                                "args": ["master_stack"] } }
```

`args` is optional and positional, and when it is absent the binding has none:
`args_ref` is `OMNI_REF_NONE` and the stored payload is the header alone. An
absent `args` and an empty `args` are the same value, because a binding whose
action takes no argument and one whose action was given nothing are not
distinguishable at dispatch.

`action_ref` in the block is a **frame offset holding the action name**, not a
registry handle. This document used to say it was the `omni_action_handle`, and
that was wrong on the one fact that matters most: a binding lives in the block,
the block is the user's editable configuration, and a user editing a binding has
to be editing a name they can read. A handle is an index into a process-local
registry, so a stored handle is meaningless to anyone reading the file, is
silently invalidated by a restart, and cannot survive a `save` and reload at all.

So resolution happens on use, not at set time. A binding stores the name; the
registry looks it up when the key fires; a name that is not registered is
`ACTION_NOT_FOUND` on use. This reverses the earlier rule in this section that an
unregistered action is refused when the binding is written, and it is the weaker
of the two checks deliberately: a store that can be
edited by hand cannot promise that every name in it resolves, and refusing the
write would mean refusing to store a binding for a component that has not
activated yet, which is the normal state during boot.

The argument array is where `omniwm.cycle_layout` finally gets its layout name, and
it removes the reason §6.1's positional schema was previously unusable from a
keybind. A snap can be one action taking a direction
(`omniwm.snap left`) rather than four actions, and a layout can be selected by
binding rather than only by a cycle command, so the parameter count stops being a
function of the number of directions. Arguments are validated against the
action's schema from §6.1 at decode time and at dispatch time, because the
schema is a property of the action and an action can be replaced by an
extension between the two.

`rule` is a match-and-replace record, and both halves are typed key patterns
rather than arbitrary values:

```
{ "type": "rule", "value": { "match":   { "key": "omniwm.*", "type": "u32" },
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
  "value": { "rule": "left", "against": "master", "axis": "x", "edge": "start",
             "region": "content", "width": 1, "height": 1, "by": 1 } }
```

There is no `match`, no `target` and no `scope` here, and an earlier draft of
this section carried all three. A `0x33` record constrains a window the solver
is already at and says nothing about which window that is: the rule set it
belongs to carries the matchers, and the two namespaces a set is under carry
the effect (`configstorage.md` §4, `layoutengine.md` §7, `ipc.md` §3). A client
that needs to be matched on `appid` or `title` writes a window rule at
`omniwm.window_rules.<name>` instead, which is a `map` and not this record.

One wire form per tag, because one tag carrying two schemas means a reader
dispatches on a field it previously assumed was fixed.

A kind field inside one tag is not the thing this warns about, and the
distinction is worth keeping sharp. Two schemas means a reader cannot tell which
fields are present, so it reads a field that may be argument padding. A kind
field with a fixed header and a fixed set of trigger slots means the reader always
reads the same offsets and only interprets the trigger slots differently, which is
the same situation as a tagged union in C: one layout, one offset table, a
discriminant. `binding` is now the second kind, because `generaldesign.md` §14.1
puts Mango's five binding structs behind one tag, and the thing the warning is
about, a reader assuming `modmask` is at offset 0 because the record is a
keyboard binding, cannot happen once the offsets are declared per kind in
`omni_layout.h`.

### 6.3 Extension boundary

There is no in-process extension interface. An external program is not a
component, is not registered, and cannot be suspended or unloaded; it writes to
the block and observes the journal like any other client. See §8.1 for the
decision and the reasoning.

## 7. Log (core/log) and util (core/util)

Ports per the port rule, fresh prefixes. This section is the contract and not
the intent: it names every function, so a stage-3 implementer writes from it
rather than from a paraphrase of it.

### 7.1 log

Mango's `log.c` is 37 lines wrapping `wlr_log_init` and `_wlr_vlog`, and it
ports as-is with one addition. Its two paths are kept because they do different
jobs: `through_wlr = true` goes through wlroots with a `[file:line]` prefix, and
`through_wlr = false` prints straight to stderr with ANSI colour, which is what
reaches the user when the message is more important than wlroots' own
verbosity.

```c
enum omni_log_level {             /* mirrors wlr_log_importance exactly */
    OMNI_LOG_SILENT     = 0,
    OMNI_LOG_ERROR      = 1,
    OMNI_LOG_INFO       = 2,
    OMNI_LOG_IMPORTANCE = 3,      /* nil */
    OMNI_LOG_DEBUG      = 4,
};

void omni_log_init(enum omni_log_level level);   /* before wlr_log_init */
void omni_log_set_level(enum omni_log_level level);
void omni_log_impl(bool through_wlr, enum omni_log_level level,
                   const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

#define omni_log_at(through_wlr, level, ...) \
    omni_log_impl((through_wlr), (level), __FILE__, __LINE__, __VA_ARGS__)

#define omni_error(...) omni_log_at(true,  OMNI_LOG_ERROR, __VA_ARGS__)
#define omni_warn(...)  omni_log_at(true,  OMNI_LOG_INFO,  __VA_ARGS__)
#define omni_info(...)  omni_log_at(false, OMNI_LOG_INFO,  __VA_ARGS__)
#define omni_debug(...) omni_log_at(false, OMNI_LOG_DEBUG, __VA_ARGS__)
```

Four levels and no more, because the set is wlroots' and wlroots' version is
not pinned yet (`build.md` does not exist). A fifth level invented here would
mean maintaining a translation table the day wlroots' enum changes.

`omniwm.log.level` is a `omniwm.*` key, so it is saved and restored like any other. It
is read in `omni_boot()` step 4, before any component's `init` runs, because a
component that logs from `init` and is silenced because nothing read the level
first is a confusing first impression. `ERROR` and above go to stderr
regardless of the level for the same reason §4 gives for refusing writes before
`READY`: a compositor that can only be reached through a log it has silenced is
worse than a noisy one.

The component tag is applied by the caller, not by a per-component logger
object, and it lives in a thread-local slot rather than in a field the call site
has to pass:

```c
void omni_log_tag_bind(const char *component);  /* once, from init() */
```

A call site reads the same at every position, and the tag cannot be forgotten at
the point of the call, which is the failure a per-call-prefix argument invites.
`[tags]`, `[input]`, `[decorate]`. A component that logs before binding logs
untagged, which is correct rather than a bug.

There is no file sink and no ring sink at v1. A ring sink is a real want later
for a crash report, and the way to add one is the `wlr_log_func_t` callback
`wlr_log_init` already takes, so the log does not have to know in advance that
one will be wanted.

### 7.2 util

Mango's `util.h` is 39 lines and ports whole, with the prefix rule and one
change per row below.

| function | from | note |
|---|---|---|
| `void omni_die(const char *fmt, ...)` | `die` | logs at `ERROR`, then `exit(1)` |
| `void *omni_ecalloc(size_t n, size_t size)` | `ecalloc` | zeroed or die; no component checks for null |
| `int omni_fd_set_nonblock(int fd)` | `fd_set_nonblock` | as-is |
| `bool omni_regex_match(const char *pattern, const char *string)` | `regex_match` | **PCRE2**, so rule filters, `watch` patterns and future gesture patterns share one engine |
| `void omni_list_append(struct wl_list *, struct wl_list *)` | `wl_list_append` | as-is |
| `void omni_list_swap(struct wl_list *, struct wl_list *)` | `wl_list_swap` | as-is |
| `void omni_list_safe_reinsert_prev(struct wl_list *, struct wl_list *)` | same | as-is |
| `void omni_list_safe_reinsert_next(struct wl_list *, struct wl_list *)` | same | as-is |
| `uint64_t omni_now_ms(void)` | `get_now_in_ms` | **u64, not u32**; see below |
| `uint64_t omni_timespec_to_ms(const struct timespec *)` | `timespec_to_ms` | as-is |
| `char *omni_string_printf(const char *fmt, ...)` | `string_printf` | heap, caller frees |
| `char *omni_join_strings(char *const *arr, const char *sep)` | `join_strings` | heap, caller frees |
| `char *omni_join_strings_with_suffix(char *const *arr, const char *suffix, const char *sep)` | same | heap, caller frees |

`omni_now_ms` is u64 rather than Mango's u32, which wraps in 49.7 days. A
compositor's animation clock, its idle timer and its region buffer all
subtract timestamps, and a wrapped `uint32_t` makes that subtraction a very large
number rather than a small one, so the bug is a compositor that suddenly animates
as fast as the wrap. A u64 cost nothing and the bug cannot be written.

`omni_regex_match` is the row that decides something outside this section.
`windows.md` §9 settled PCRE2 as the sole filter operator, and this is where
that engine lives, so a window rule, a `watch` pattern check, and a gesture
pattern in `input.md` all reach for one call. That is the argument for
putting it in `util` rather than in whichever component needs a filter first: a
second engine would mean a rule written in one dialect failing in another.

The macros port whole with the prefix rule and no changes: `OMNI_MAX`,
`OMNI_MIN`, `OMNI_GEZERO`, `OMNI_LENGTH`, `OMNI_END`, `OMNI_LISTEN`,
`OMNI_LISTEN_STATIC`. They are dwm's, carry no design weight, and renaming them
is the whole of the port.

### 7.3 The container kit

**This is the part that has to be right, because these utilities have two jobs
that are easy to confuse.** The first is ordinary: a component needs somewhere
to put a growable list. The second is the extensibility surface, where adding a
layout, an action or an option should be a matter of adding a descriptor and
nothing else.

Mango reaches for the second job with a 40-field `ConfigWinRule` struct and an
`APPLY_INT_PROP` macro per field, where a new property is a struct field, a
macro invocation, and a place for the compiler to catch you. `windows.md` §5
replaced the struct with `if`/`then` blocks and a `map` in the block, so the
question this section answers is whether the utilities make the second job as
cheap as the first.

| container | shape | why |
|---|---|---|
| dynamic array | `struct omni_dynarr`, contiguous, doubles on growth | the workhorse: any component's list of anything, and the substrate the registry's own sorted lists are built on |
| string builder | `struct omni_strbuf`, append and format | building a key path while walking the catalog |
| hash map | open-addressed, string to `uintptr_t` | name to `entry_id` for layout, action and set names, and for the catalog's own name index (`configstorage.md` §3.1); three lookups per keypress plus a name resolution per `get`, so it has to be O(1) |
| ring buffer | `struct omni_ringbuf`, fixed capacity, no allocation | the journal is one of these in the block already; a native one serves the region buffer and the solver's iterations |
| interning | `struct omni_interns`, string to `uint32_t` | layout and action names are compared constantly and there are few enough to hold once |
| linked list | `wl_list` only | Wayland's is intrusive, allocation-free and already in every component, so a second list is a second convention |

**No component writes its own container.** One dynamic array, one hash map, one
string builder. The reason is narrower than consistency: §2's registry has to
resolve names and sort descriptors, and a component with its own hash map means
the registry's lookup is not the one everything else uses. That is exactly the
coupling which makes a new action more than a descriptor.

Interning pays for itself on the first keypress rather than later, because
`omniwm.cycle_layout`'s argument is a layout name, and resolving a name to a slot
twice per lookup is a string compare per lookup.

### 7.4 What this section deliberately does not do yet

The kit above is chosen for components that read and write values. When
registering a layout, an action or an option becomes user-facing, three more
things are needed, and **none of them is a container**:

- a **declarative** descriptor form, so a component publishes its options as
  data rather than as a C array the registry walks with a callback;
- a **validation** step, so a declared option's type tag is checked against the
  store before a value is read, which is `tomlparser.md` §3's key registration
  arriving for real and `struct omni_option` in §3.1 being its shape;
- **introspection**, so something can ask the registry what a component offers
  instead of having to know in advance.

What §7.3 commits to is the substrate all three need, and nothing in it has to
be thrown away when they land. That is the constraint to hold this section to:
utilities here are chosen so the later extensibility work builds on them, not
so it has to replace them.

### 7.5 Error and diagnostic conventions (D5)

Two questions were parked on this item and they are not the same question, so
this says which it answers first. The first is **what a caller sees when a
request fails**, and it is answered for actions in §6.1 and for the store in
`configstorage.md` §12.6; the general rule below extends it to options. The
second is **where a diagnostic goes and what it names**, which is the log policy
below. They are answered in that order.

**The return path.** Every failure a component reports has one of two shapes,
chosen by what the caller can do about it, never by where the failure was
detected:

- A **store failure** is an `OMNI_ERR_*` value from the vocabulary in
  `omni_layout.h` §12. A component does not invent a code for it and does not
  translate it; the code names a fact about the block, and the component passes it
  through (`configstorage.md` §12.6).
- A **message failure** is an `OMNI_SOCK_ERR_*` value: the request did not decode,
  the arguments did not match the schema, the action does not exist, or the action
  failed. §6.1 fixes the action cases, and the rule for the rest is that a
  component reports the same code the socket would, because a trigger-visible
  outcome and a socket client's outcome are the same fact.

A **failed option validation** is a store failure, not a third shape. A registered
option's declared type tag is checked against the stored value
(`configstorage.md` §12's L4 tier), and a value that does not satisfy it is
refused with the same `OMNI_ERR_*` the read path uses, because the option table is
registration data and a mismatch is a fact about the block and its registration
rather than about a message.

The consequence is that a component has no private error vocabulary. It either
returns a code from one of the two shared sets or it has no error to report, and
the two sets are already partitioned by what they name: `OMNI_ERR_*` is the
block, `OMNI_SOCK_ERR_*` is a message or a facade limit.

**The diagnostic path.** `configstorage.md` §12.6 owns the store's diagnostics.
The rule outside the store is the one already stated as a contract in §7.1:

- A diagnostic is logged through `omni_log_*`, tagged with the component's bound
  name, and never written into the store as a key. A component that wants its
  state observable publishes it as an ordinary or `WINDOW_DEPENDENT` value, which
  is data, and keeps its diagnostics out of the block.
- `ERROR` and above go to stderr regardless of the configured level, per §7.1. A
  failure a user has to act on is never silenceable.
- A diagnostic names the component and the failing operation, not the caller. A
  log line is for the WM's operator and the caller's error is the return path
  above; the two are not substitutes and a component does neither on behalf of the
  other.
- `ACTION_FAILED` is the one place the two paths meet, and §6.1 states the split:
  the caller gets `ACTION_FAILED` as its result and the component logs what went
  wrong. Keeping them distinct is what tells "you asked for something that does
  not exist" (`ACTION_NOT_FOUND`)   from "the thing you asked for went wrong".

### 7.6 File-local symbols carry the module prefix (D7)

`omni_` is the prefix for an exported symbol, one that is declared in a header and
reaches another translation unit. It is not the prefix for a file-local symbol.
§7.2's operations are the exported case and their `omni_` names are right; this is
the other case, and it was the last open naming item because it is the difference
between a `grep` that finds a symbol's callers and one that returns one file's
private helper alongside every `omni_` symbol in the tree.

**Decided 2026-10-10: a file-local symbol is prefixed with its module, not with
`omni_`.** The module is the file's name without extension, so a `static` helper in
`src/input/input.c` is `input_setup` and a `static` helper in
`src/input/pointer.c` is `pointer_setup`. A file and its prefix are the same string,
which is what makes the rule mechanical rather than a judgement call. The two rules
partition the namespace rather than competing for it: anything with external
linkage is `omni_`-prefixed, anything `static` carries its module prefix, and the
dwms macros in §7.2 are `OMNI_`-prefixed and unaffected.

## 8. Server (core/server.{c,h})

`server.md` is the authority for all of this and it is where `struct omni_server`
is declared, along with the boot order, the readiness mapping, the dispatch
model, and the shutdown sequence. An earlier draft of this section carried a
second copy of the struct with a `void *block` where `server.md` §1 has a
`struct omni_store *store`, and restated the boot order verbatim. Both are the
failure mode `server.md` §1 names, and they are removed here rather than
reconciled: a second copy of a struct is a second contract.

What helpers.md contributes is the one thing server.md cannot know, which is
that the registry this section is about produces the ordering server.md's boot
order consumes. So the only restatement is that direction: `omni_boot()` in
`server.md`'s step 4 is this registry's Phase 2 (§2), and it is called once,
after the register-function table has run.

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

- ~~`omni_event` field set is fixed by the journal slots (configstorelayout.md
  §8); richer in-process payloads need a follow-up design if components want
  them.~~ **Closed by decision: the field set is final and is not extended.**
  §7 now records the decision and the reasoning. A trigger is a view of the
  journal, so the struct mirrors one slot and stops there; a component needing
  more reads the block, and a synthetic event is deliberately not expressible
  because it would let a dispatch deliver something no commit produced. The
  `COMMIT_END` and custom-event cases are answered by the dispatcher enforcing
  the boundary before delivery and by the custom event arriving as an ordinary
  match, neither of which needs a new field.
- The action argument schema and result typing are fixed in §6.1: positional
  typed tags plus one declared result tag.
- ~~The `binding` key-to-action path, the `modmask` text grammar, and the keysym
  name table.~~ **Closed by decision, not by design here.**
  `generaldesign.md` §14 decides that we use Mango's code where we can and change
  what we need, and Mango's `KeyBinding` already carries `mod`, a `KeySymCode` and
  an `Arg` array, with `parse_bind_flags` and the XKB keysym table supplying the
  grammar and the names. So the key-to-action path is Mango's, the `Arg` array
  is this document's `args`, and the modmask and keysym spellings come across
  with the code rather than being specified twice. The `action_ref` question
  this bullet used to leave open is closed as well: §6.2 settles it as a frame
  offset holding the action's name, and `ipc.md` §8 has been corrected to agree.
- Component dependency edges beyond plain priority (a hard "requires X")
  do not exist at v1, and are not planned; priority ordering is the mechanism
  permanently unless a real dependency failure appears. The observable
  consequence is documented in §9 so it is not read as an oversight.
- The `owner` field on an instance is diagnostic only. Nothing enforces it and
  no access check consults it; it exists so a log line can name a component
  without a registry lookup. A component that wants access control has to use
  the namespace convention in `configstorage.md` §3, which is convention too.

## 12. Worked component: a per-window display name

This is the phase-02 worked component ("Done when" item 4). It is written here
rather than in the tree, so it is a guide every later phase cites instead of a
restatement of the patterns. It is deliberately small and synthetic: a component
that gives a window a display name, chosen by a window rule or set by a
dispatcher. It exists to exercise the whole path once: descriptor, options,
actions, triggers, block-versus-instance fields, the read-and-react pattern, and
the register calls of §2.2.

The mock names below are illustrative in the same way the rest of this document
is: the signatures are the design, the bodies are sketches.

### 12.1 The component and what each field belongs to

The component is `displayname`. Its one user-visible rule is that a window may
carry a name other than its title, and the name is per-window, so it is
`WINDOW_DEPENDENT`: observable, never saved (§3.5). Applying §3.5's dividing
question to every field the component holds:

- **Descriptor** (immutable, §3.1): the option, action and trigger arrays, the
  name, priority, `enable_key`, and the callback pointers.
- **Instance** (mutable process memory, §3.2): the derived index cache (§12.4)
  and the activation and init-result fields.
- **Block** (config or observable state, §3.5): the per-window name itself, an
  ordinary or `WINDOW_DEPENDENT` value under the window's entry; and the
  component's own options, which are ordinary configuration.

### 12.2 The descriptor

```
static const struct omni_option displayname_options[] = {
    { "omniwm.displayname.enabled", OMNI_TYPE_BOOL, &(bool){true} },
    { "omniwm.displayname.default", OMNI_TYPE_STRING, "" },
};

static const struct omni_action displayname_actions[] = {
    { "omniwm.displayname.set",
      displayname_action_set, NULL,
      &(struct omni_action_schema){
          .n_args = 2, .arg_tags = (uint16_t[]){ OMNI_TYPE_STRING, OMNI_TYPE_STRING },
          .result_tag = 0 } },
};

static const struct omni_trigger displayname_triggers[] = {
    { "omniwm.client.*.display_name", OMNI_GAP_FAIL, displayname_on_name_change },
};
```

The action's two arguments are the window to target and the name to give it,
and the window is an argument rather than a binding scope, which is the
2026-09-29 decision that client targeting is the action's business.

### 12.3 How the component is registered

```
void omni_register_displayname(void)
{
    omni_option_register_many(displayname_options);
    omni_action_register_many(displayname_actions);
    omni_append_component(&(struct omni_component){
        .name       = "displayname",
        .priority   = 200,
        .critical   = 0,
        .enable_key = "omniwm.displayname.enabled",
        .init       = displayname_init,
        .suspend    = displayname_suspend,
        .resume     = displayname_resume,
    });

    omni_rule_property_register("display_name", OMNI_TYPE_STRING, displayname_apply_rule);
}
```

One collect entry, and the rule property is registered beside the component
because it is the component's own extension point. `component-registry.h`
includes this file so `core/server.c`'s table can name `omni_register_displayname`.

### 12.4 The instance and the derived index

The name is read on every draw, so it is cached and refreshed rather than
looked up per frame. This is §5.1's pattern made concrete:

```
struct displayname_instance {
    struct omni_instance base;              /* descriptor, state, init_errno */
    struct omni_map     name_by_client;     /* key: entry_id, value: copied name */
    uint64_t            indexed_commit_id;  /* commit the cache reflects */
};

static void displayname_init(void)
{
    omni_subscribe("omniwm.client.*.display_name", OMNI_GAP_FAIL,
                   displayname_on_name_change);
}

static void displayname_on_name_change(const struct omni_event *event)
{
    displayname_index_refresh();          /* re-read the block, copy values */
}
```

The cache holds a copied name, never a frame offset, because an arena frame can
be recycled and a copied value survives a commit (§5.1). Invalidation is the
journal event for the window's name entry, and the callback refreshes rather
than polls.

### 12.5 The action and the rule

```
static void displayname_action_set(const struct omni_args *args)
{
    struct omni_entry window = omni_resolve_client(args->value[0]);
    if (!window.entry_id)
        return omni_action_fail(OMNI_SOCK_ERR_ACTION_FAILED);
    omni_set_window_value(window, "display_name", args->value[1],
                          OMNI_SCOPE_WINDOW_DEPENDENT);
}

static void displayname_apply_rule(const struct omni_rule_ctx *ctx)
{
    omni_set_window_value(ctx->window, "display_name", ctx->value,
                          OMNI_SCOPE_WINDOW_DEPENDENT);
}
```

Both paths write the same block value with the same scope bit, so a name set by
a rule and a name set by the dispatcher are one kind of value. The write is what
raises the journal event the subscription refreshes on, so the action needs no
separate notification step.

### 12.6 What this example is a guide to

A later phase adding a component follows this shape: a descriptor with its
option, action and trigger arrays; an instance holding only mutable process
state; block entries only for configuration and observable state; behavior
attached through a trigger subscription rather than an option callback (§3.6);
and one `omni_register_<component>` entry that a single line in `core/server.c`
names. The parts specific to `displayname` are the two rule paths and the
per-window scope; everything else is the substrate.
