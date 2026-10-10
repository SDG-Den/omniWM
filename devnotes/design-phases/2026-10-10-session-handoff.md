# omniWM phase-02 session handoff (registry + namespace + phase closeout)

*Preserved 2026-10-10 from `/tmp/nix-shell.83nT4O/opencode/omniwm-registry-brainstorm.md`
so it survives the shell being cleared. It is a work log and is not reconciled
against later decisions; the raw answers are in `decisions-2026-10-10.md`.*

Scratch handoff written 2026-10-10 before compaction. Source of truth is
`devnotes/`; this note holds the current state, the decided API surface, and the
remaining phase-02 work. Repo `/home/den/repos/omniWM`.

## Objective

Design phase 02 (stage 3, the substrate) and implement the deterministic items.
The active remaining design writing is the `component-registry.h` function scope
and the in-doc worked component. Then close out phase 02.

## Done this session (already written to docs)

- Render stack decided: wlroots renderer anchor + libplacebo external layer.
  Recorded `build.md` §3/§4.1, `02-substrate.md` B3.
- B4 Q32.32 in int64_t: `build.md` §5. B6 core scope + registry machinery:
  `build.md` §6. B7 licence/provenance: `LICENSE`, `PROVENANCE.md`,
  `devnotes/licence.md`. D7 module prefix: `helpers.md` §7.6.
- **Namespace rule (new, decided)**: omniWM owns one root, `omniwm.*`. Core and
  every internal component write only there. Any other root is a foreign writer's
  own: carried in SHM/IPC/TOML but never registered, reserved, validated, or
  interpreted. `ext.<name>.*` is no longer a project concept. Convention, not
  enforcement (block cannot identify writers).
- Rename APPLIED: `\bwm\.` -> `omniwm.` across 15 authoritative docs (61 keys),
  plus `include/shared/omni_layout.h:26,733`. Convention statements rewritten in
  `configstorage.md:146`, `configstorelayout.md:505`, `helpers.md:482`.
  `ipc.md:583` custom category `ext.foo.signal` -> `foo.signal`.
- **Left untouched on purpose** (project rule: work logs are not reconciled):
  `audits/audit-2-findings.md`, `design-phases/00-reconciliation.md`,
  `summaries/2026-10-03-omniwm-design-snapshot.md`. They still contain `wm.`/`ext.`.
- **Ephemeral corrections (new, decided)**:
  - TOML keys are ordinary (non-ephemeral), despite being set by the config
    facade, because `save` must write them back. Facade-set does NOT imply
    `EPHEMERAL`. `EPHEMERAL` is narrow: process bookkeeping or a temporary change
    over IPC, and most setters are higher-level functions not yet designed.
    Recorded `configstorage.md:148` (new bullet); softened
    `configstorelayout.md:504` "set" -> "may set".
  - Foreign-writer keys are ordinary by default, same classification as a
    component option key through TOML. Soft reset clears them, so a program
    driving the WM entirely through SHM must repopulate from its own
    `omniwm.reset` handler. Rewrote `ipc.md:967-973`.
- **External programs: edge is SHM/IPC only.** No external registration path.
  `helpers.md:645` (6.3) already states this: an external program is not a
  component, is not registered, writes the block and observes the journal.

## Remaining to close phase 02

1. **B3 design half** (the only formally-open item, `scaffolding.md:259`):
   decompose `build.md` §4's 14-item toolkit list into primitives, define the
   preset-to-primitive mapping, mark where libplacebo is reached.
2. **Worked component doc + `component-registry.h` scope** (Done-when item 4,
   D1/B6). Decided as a shape; content is the writing. Must resolve:
   - pre-parse boot slot for built-in layout registration (see below),
   - decoration register vs effect presets,
   - gesture register vs action+trigger,
   - `on_change` sugar vs strict declaration/reaction split,
   - handle-vs-static split,
   - the register-function set below.
3. **IPC method registration section in `ipc.md`** (agreed). Internal-only:
   a component registers a handler extending the socket command set; external
   programs never register. `omni_ipc_method_register(namespace, method, handler,
   schema)`, namespaced under `omniwm.*`, reusing the action schema, dispatched
   after the publishing commit, readiness-gated.
4. **Build tree**: add libplacebo to `meson.build` `dependency()` and
   `nix/common.nix` `buildInputs`.
5. `flake build` on a clean machine (B5) cannot be verified here (no compiler on
   PATH).

## Register-function set (proposed, needs dev refinement while writing)

One component may make multiple register calls. Handles (releasable at suspend):
actions, triggers. Static (process-lifetime): everything else, because components
do not unload.

- Component entry: `omni_register_<component>(void)` (collect, appends descriptor,
  never runs init) - `helpers.md` §2, `server.c` table (`server.md` §9.1).
- Options: `omni_option_register(key, type_tag, default)` - `helpers.md` §3.1.
- Actions: `omni_action_register(name, handler, context, schema) -> handle`.
- Triggers (event AND SHM key): `omni_trigger_register(pattern, gap_policy, cb)
  -> handle`, `KEY_SET` pattern, journal is the only event source (`helpers.md` §5).
- Startup: `omni_register_init(fn, priority)` (user addition, distinct from
  descriptor `init()`).
- Rules: `omni_rule_field_register(name, type_tag, read_fn)`;
  `omni_rule_property_register(name, type_tag, apply_fn)`. No operator register.
- Layouts: `omni_layout_register(name, program)` (see storage below).
- Rendering: `omni_node_type_register(type_id, properties, draw_fn, pass_fn)`
  (scenefx new node type + pass, `generaldesign.md` §3);
  `omni_effect_register(name, primitive_ids[], params_schema)`;
  `omni_shader_register(name, glsl_source, kind)`;
  `omni_animation_register(name, property, curve_or_timeline, property_source_fn)`;
  `omni_render_layer_register(layer, ...)`; `omni_decoration_register` only if
  decoration is its own kind, else built from effect presets (OPEN).
- Server: `omni_protocol_register(name, interface, bind_fn)`;
  `omni_ipc_method_register(...)` (internal).
- Input misc: `omni_gesture_register(...)` likely collapses into actions+triggers.

## Option behavior: how an option's function attaches (shadows)

`omni_option_register` is declaration only `{key, type, default}` (helpers §3.1).
Behavior attaches through existing machinery, never a field on the option:

- If the key is the component's `enable_key`, the toggle engine (`helpers.md` §4)
  drives `init`/`suspend`/`resume`.
- Every other key is acted on by a trigger subscription; callback re-reads and
  applies (`helpers.md` §5). No per-option callback, because that would be a
  second dispatch path and §5 forbids synthetic in-process events.

Example enable key is `omniwm.shadows.enabled` (NOT `omniwm.shadows.enable`).

## Layout storage + built-in registration

- No preset object. A built-in is a compiled-in program in the same shape TOML
  produces, handed at startup to the same store-write functions the parser uses;
  "registered" and "configured" are one state (`layoutengine.md` §3.4, lines
  802-808). The value lands in the block; "no stored definition" (line 1235)
  means the source is compiled in.
- Reserved set = whatever is registered (lines 1180-1183, 1255-1260);
  `OMNI_LAYOUT_BUILTIN_NAMES` in the header is only a v1 seed and the guard does
  not read the string.
- **User refinement**: built-in registration runs on init via a register function
  that makes the same calls the TOML parser makes, before the parser parses.
  `omni_register_layouts()` (collect, reserves names, no writes); per-layout
  `init_layout_<name>()` (e.g. `init_layout_dwindle`) writes the shm keys.
- **Ordering constraint to record**: built-ins must be written before the config
  facade parses user config (so user config overrides), and reset must re-run
  built-ins then re-apply user config (`layoutengine.md` 843-846). That makes it a
  pre-parse boot step in `server.md` §2, not a normal component init at its
  priority relative to the facade. Pin down the slot.

## IPC method registration (internal-only)

`omni_ipc_method_register(namespace, method, handler, schema)`; namespace under
`omniwm.*`; reuse `omni_action_schema` (`helpers.md` §6.1); dispatch on the event
loop after the publishing commit (`ipc.md` §2, `server.md` §9.2); readiness-gated
(`ipc.md` §5.2). Needs an explicit home in `ipc.md`, which currently fixes a core
command set. Keep the internal/external split explicit when writing it.

## Key file references

- `devnotes/design-phases/02-substrate.md`: B/D items, Done-when (item 4 = worked
  component).
- `devnotes/design-phases/scaffolding.md:211-272`: phase-02 append; only B3 open.
- `devnotes/build.md` §3/§4/§4.1/§5/§6: build half.
- `devnotes/helpers.md` §1.1, §2, §3, §3.1, §3.3/§3.4, §3.5, §4, §5, §5.1, §6,
  §6.1, §6.3, §7.5, §7.6.
- `devnotes/layoutengine.md` §3.4/§4; `devnotes/windows.md` §9;
  `devnotes/generaldesign.md` §3/§7/§9/§10/§11; `devnotes/input.md` §4/§5/§7.
- `devnotes/configstorage.md` §3/§12.2/§14.1.1; `devnotes/configstorelayout.md`;
  `devnotes/ipc.md` §2/§4/§5.2/§13; `devnotes/tomlparser.md` §3/§9;
  `devnotes/server.md` §2/§9; `devnotes/testing.md` §2.
- `include/shared/omni_layout.h`; `meson.build`; `nix/common.nix`; `flake.nix`.
- Plan file: `/home/den/repos/nixos-configs/notes/AI-output/2026-10-10_omniWM-phase-02-substrate-plan.md`.

## Working rules in force

- No compiler on PATH; use `nix run nixpkgs#gcc -- ...` for syntax checks. Never
  read/search `/nix`; never `rm` without explicit permission; never string-kill.
- Do not make dev decisions; refill the register set and open questions with the
  dev. Confirm the prefix `modes: build. intent: ...` each turn.
