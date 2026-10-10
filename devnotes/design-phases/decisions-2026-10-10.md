# Decisions recorded 2026-10-10

These are the phase-02 close-out answers given 2026-10-10, preceded by the
decisions from the earlier 2026-10-10 session that were written into the
documents without a raw record of their own. This file is the raw record; the
owning documents are authoritative where they restate a decision.

## Part A. Decisions already written into the documents

**The namespace rule.** omniWM owns one root, `omniwm.*`. The core and every
internal component write only there. Any other root is a foreign writer's own:
carried in SHM, IPC, and TOML, but never registered, reserved, validated, or
interpreted. `ext.<name>.*` is no longer a project concept. It is convention,
not enforcement, because the block cannot identify writers. Recorded
`configstorage.md` §3 and `configstorelayout.md` §6; the discipline is applied
in `helpers.md` §6.

**The EPHEMERAL correction.** A TOML key is ordinary configuration even though
the config facade sets it, because `save` must write it back. Facade-set does
not imply `EPHEMERAL`. `EPHEMERAL` is narrow: process bookkeeping, or a
temporary change over IPC. Recorded `configstorage.md` §3.

**Foreign-writer keys.** Ordinary configuration by default, the same
classification a component's option key gets through TOML. A soft reset clears
them, so a program driving the WM entirely through SHM must repopulate them from
its own `omniwm.reset` handler. Recorded `ipc.md` §reset.

**IPC method registration.** A component may register a socket command verb. It
is internal-only, namespaced under `omniwm.*`, reuses `omni_action_schema`,
dispatches on the event loop after the publishing commit, and is readiness-gated.
An external program has no registration path and reaches a method by sending the
command. Recorded `ipc.md` §4.

**The render stack.** The anchor is wlroots' own renderer; the external
cross-GPU layer is libplacebo, reached only by an effect that needs a GLSL or
compute pass. Recorded `build.md` §4.1.

**The solver arithmetic width.** Q32.32 in `int64_t`: signed 64-bit, 32
fractional bits, with the TOML parser absorbing the rounding. Recorded
`build.md` §5.

**The file-local symbol prefix.** A file-local `static` symbol carries its
module prefix (the file name without extension), not `omni_`. `omni_` is for
external linkage only. Recorded `helpers.md` §7.6.

## Part B. The phase-02 register and registry questions

### 1. Handle versus static split

Handles only for actions and triggers, which are the two registrations a
suspended component releases. Everything else is process-lifetime static,
because components do not unload. The handle is the lifetime identity and the
name is dispatch metadata, exactly as `helpers.md` §6 already states for
actions.

### 2. The register-function set

Keep the proposed set as listed, and add more later only if a component needs
one. A component may make several register calls. The set is written into
`helpers.md` §2.2: component entry (`omni_register_<component>`), options,
actions, triggers, init, rule fields and properties, layouts, node types,
low-level decoration registers, effects, shaders, animations, render layers,
protocols, and the IPC method.

### 3. Decoration register functions

Decoration is not only a set of effect presets. Lower-level decoration register
functions are exposed, so a component can register pieces within scenefx and
within omniWM's own extensions to it. The higher-level preset form is built on
those primitives rather than replacing them.

### 4. Gesture

A gesture has no register function. It collapses into a trigger plus an action
for now: the input layer recognises the gesture and dispatches the action, and
the trigger carries the event. A dedicated gesture kind is not added.

### 5. Option behavior attachment

An option stays declaration-only: `{key, type, default}`. Behavior attaches
through a trigger subscription that re-reads the key and applies it
(`helpers.md` §5). There is no `on_change` field on the option, because that
would be a second dispatch path and §5 forbids a synthetic in-process event.

### 6. Layout registration ordering

The built-in layout registration functions run before the user can register any
layout, so the built-in names are already occupied by the time user
registration runs. Built-ins are written before the config file is replayed so a
user layout of the same name overrides the built-in (`server.md` §2,
`layoutengine.md` §3.4).

### 7. The worked component

A small synthetic component: a per-window display name, settable through a
window rule and a dispatcher action. It is written in `helpers.md` §12 as the
concrete example every later phase cites, with mock functions and a generic
method, rather than in the tree.

### 8 to 10. The render toolkit (B3)

Resolved the same session, written into `build.md` §4.2 and §4.3.

Nine primitives: Target, Source, Transform, Geometry, Shader pass, Blend, Pass
chain, Driver, Node hook. A preset is a chain bound to one config key. The
libplacebo boundary is per pass: a pass the wlroots renderer can express runs
there, and only a pass that needs arbitrary GLSL or a compute blur reaches
libplacebo.

Five primitives are named beyond §4's original list: mask, subtree capture,
camera and projection, persistent target, and feedback blend. A sixth capability,
the delayed property sample, sits on the Driver.

The built-in trail and afterimage mechanism is feedback: a persistent two-slot
target plus a decay blend. The geometry trail is enabled as a preset by the
delayed sample. Blur and gradient ship as shared chains. The node and pass model
is written in `build.md` §4.3 rather than deferred to phase 08, because phases 09
and 10 consume it.

### 11. Registry naming

`registry.{c,h}` holds the actual registry components. `component-registry.h` is
a thin header that includes every component file so their register functions run
and their things get registered. The two names are both correct and name
different things.

### 12. `flake build` verification

Not useful before the design is done. The `flake build` Done-when item is
recorded as deferred in `build.md` and `02-substrate.md`, and becomes a check
for the substrate implementation rather than for this design phase.

### 13. The summary snapshot

Keep `devnotes/summaries/2026-10-03-omniwm-design-snapshot.md` and commit it. It
is a dated work log and is not reconciled against later decisions.

### 14. This decisions record

Create `decisions-2026-10-10.md`, which is this file.
