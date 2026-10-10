# Server (devnotes/server.md)

The authority for the compositor server: what owns what, the order things come
up and go down, and where each contract is defined. Component and action
contracts stay in `helpers.md`; this document does not restate them, it points at
them. Anything about the server lifecycle stated in two places is a bug.

## 1. Ownership

```
struct omni_server {
    struct wl_display     *display;
    struct wl_event_loop  *loop;
    struct omni_store     *store;      /* the SHM block client */
    int                    sock_fd;    /* socket facade, optional */
    struct omni_registry  *registry;   /* descriptors and instances */
};
```

One owner, typed borrows. The block belongs to the server for the process
lifetime; components borrow it and never own block state. A component that wants
a region goes through the request queue (`configstorage.md` §8) rather than
touching pool memory, so no component can invalidate another component's view of
the pool. The socket is the server's own client of the block, and its absence
degrades the project to its lower-skill-floor surface without making it
unusable, because the block is authoritative (`ipc.md` §1.5).

Handlers receive a `struct omni_action_context` carrying the server and the
owning component's descriptor and instance, never a reachable global
(`helpers.md` §6). Nothing reaches into server state by reaching for a global.

## 2. Boot order

Boot is one entry point, `omni_boot()`, and it runs once. The order is not
interchangeable, because each step depends on the previous one existing.

1. create the store, seed the core `omniwm.*` keys, and register the built-in
   layouts: reserve each built-in name and write its program through the same
   store calls the TOML parser makes, before any user config is replayed so a
   user layout of the same name overrides the built-in
2. start the socket facade, if it can be created
3. run the register-function table from `core/server.c`, appending descriptors
4. `omni_boot()`: create instances in priority order, seed options, subscribe
   triggers, register actions, call `init`
5. set the header `ready` field per §4
6. enter the display event loop

The socket starts at step 2, before components exist, which is why readiness is
a readable header field rather than a handshake: a client that connects early
can see what the compositor is doing instead of inferring it from failed
commands (`ipc.md` §5.1).

Registration at step 3 never runs `init` and never creates an instance. It
records what a component is, and activation is a separate pass, so a component
that fails to start is still registered and can still be named in a readiness
report.

The built-in layouts are registered in step 1 rather than during activation
because the config facade replays the config file in step 4 and the ordering has
to favour the user: the built-in names are occupied and their programs written
first, so a user layout of the same name lands on top (`layoutengine.md` §3.4).
The built-in registration functions run before the register-function table at
step 3 and before the config parse at step 4, so by the time any user can
register a layout the built-in names are already taken. A soft reset re-runs the
same booted sequence, so the built-in is restored and an in-place edit to it does
not survive, which is the reset edge `layoutengine.md` §3.4 already states.

## 3. Activation

Descriptors are sorted by `priority` ascending with registration order as the
stable tiebreak (`helpers.md` §9), then each is activated in turn. Priority is
advisory and is not a dependency edge; a component that activates before
something it implicitly needs surfaces as an init failure rather than as a
detected violation, because the registry does not infer dependencies.

One instance exists per activation, where activation means the server starting
up. The instance lives until shutdown. A toggle moves it between `ACTIVE` and
`SUSPENDED` and never re-runs `init` (`helpers.md` §3.2).

## 4. Readiness

The header carries a `ready` u32 separate from `state`
(`configstorelayout.md` §3). The two axes are distinct on purpose: `state` is
whether the block is structurally trustworthy, `ready` is whether the compositor
can service requests against it. Conflating them would make "still starting"
and "store corrupt" indistinguishable, and their client responses are opposite.

| outcome | `ready` |
|---|---|
| all critical components activated | `READY` |
| a non-critical component failed | `DEGRADED` |
| a critical component failed | `FAILED` |

A non-critical failure does not stop the boot, because the alternative is that
one bad component leaves the user with no WM and no diagnosis. `FAILED` is not
`BROKEN`: the block is intact and readable, so a client must not discard a
recoverable configuration because a component failed to start. Reads are still
served in every one of these states, and writes are refused with `NOT_READY`
until `ready` is `READY` (`ipc.md` §5.2).

`ready` is written only in a commit, so a client that observes the `omniwm.ready`
journal event has necessarily already observed the new field value.

## 5. Dispatch

Every dispatch is a call on the event loop. There is no second thread of
control, and that single fact is what removes a large amount of machinery: a
shutdown request is queued like other work and runs when no callback is in
flight, so destruction needs no callback refcount and no quiescence protocol
(`helpers.md` §4.1).

The one obligation this places on a component is that a callback must not block
waiting for another thread, because a thread waiting back on the event loop
deadlocks the compositor. A component that starts a thread joins it inside
`destroy` or `suspend`.

Reentrancy inside a dispatch is safe because a callback receives values already
resolved from the journal entry and never a pointer into live store state, so an
inner commit cannot invalidate an outer callback's inputs (`helpers.md` §5). A
callback that commits re-enters dispatch one nesting level deep, then unwinds.

`exec` spawning an external process is not an exception. The spawned process is
outside server ownership; the contract is that it outlives the server or is
killed explicitly, and the server does not track its callbacks.

## 6. Shutdown

Reverse activation order, and the order is the contract rather than a
convenience: components are destroyed in reverse `priority` order, so a
component that depended on an earlier-activated one is torn down first.

1. close the socket, so no client observes a half-torn-down server
2. unlink the block
3. `destroy` each instance that reached `ACTIVE`, in reverse priority order
4. release the instance records last, after every `destroy` has returned, so no
   callback can observe a freed instance

An instance in `FAILED` has no `destroy` called, since its `init` never
completed and its resources were never acquired.

## 7. Extension boundary

There is no native plugin interface, and that is a decision rather than a
deferral (`helpers.md` §8.1). The server exposes no host API, no ABI version,
and no capability negotiation, and the registration table is static and
compile-time known. Extension happens through the block: a program in any
language writes to the store and observes the journal, and needs no build-time
coupling to the compositor. An external program is not a component, has no
`enable_key`, and cannot be suspended or unloaded by the server.

## 8. Document map

| contract | authority |
|---|---|
| component descriptor, instance, states | `helpers.md` §3 |
| registration and activation | `helpers.md` §2 |
| priority convention | `helpers.md` §9 |
| toggle engine and suspend | `helpers.md` §3.4, §4 |
| action and trigger handles | `helpers.md` §5, §6 |
| store layout and commit protocol | `configstorelayout.md` |
| store semantics, recovery, request queue | `configstorage.md` |
| socket protocol, readiness, limits | `ipc.md` |
| config file binding | `tomlparser.md` |
| extension boundary | `helpers.md` §8.1, and §7 here |

## 9. What the server owns, and what it defers (settled in phase 02)

The three items this section used to leave open are not the server's to design.
Each now names the phase that designs it and the surface the server exposes for
it, so the deferral lands somewhere instead of pointing at a document that does
not exist yet.

- **Display and output lifecycle.** Designed by phase 04 (`monitor.md`) and phase
  11 (`protocols.md`). The server half is the output surface: the server owns the
  output layout and the output manager, and hands each output to the monitor
  component through the action and trigger context, never as a reachable global
  (§1).
- **Input devices.** Designed by phase 03 (`input.md`). The server half is the
  seat surface: the server owns the seat, the device manager, and the pointer,
  keyboard, touch, and tablet protocol objects, and routes their events to the
  input component. `input.md` §3's file map is the decomposition of that routing.
- **The Wayland protocol set.** Designed by phase 11 (`protocols.md`). The server
  half is the registration of the protocols the compositor instantiates and the
  `wl_display` and `wl_event_loop` that carry them (§1).

What phase 02 settles is the server's own shape, below.

### 9.1 The registration table

`core/server.c` owns one fixed table of register functions, run at §2 step 3. Its
contents are the component set, and they are ordered by the priority convention
`helpers.md` §9 fixes rather than by inference. The ordering the component set
must respect is:

| order | components |
|---|---|
| first | core: the store and the registry's own activation |
| next | the config facade and the IPC bootstrap |
| then | input |
| then | tags and windows |
| then | decorate and draw |
| last | ext-protocol and the miscellaneous components |

The concrete `priority` numbers are chosen when the first components register,
because they are an implementation artifact; what is fixed is the relative order
above and the fact that the table is static and compile-time known
(`helpers.md` §8.1). A component is added by writing its register function and
adding one line here, and the band decides order without the server inferring a
dependency (`helpers.md` §9). A component registers once even when it has several
translation units.

The built-in layout registration is the one set that runs ahead of this table,
at §2 step 1, so the built-in layout names are occupied before any component or
user config can register a layout of the same name.

### 9.2 The live IPC surface

The half of the socket facade that needs a live compositor is the command
dispatch that runs an action or a `set` against the running server. `ipc.md` §5.2
fixes the per-command readiness behaviour, and the server half is that the
dispatch is a call on the event loop (§5) in the order the store publishes, so a
client that changes the store and then triggers an action observes the action
after the change. These are wlroots-harness cases (`testing.md` §2), not unit
cases.
