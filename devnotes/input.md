# Input (devnotes/input.md)

The porting map for the input subsystem: which of Mango's files become which
omniWM component, the three seams the port has to get right, and what stage 4
delivers versus what stage 10 owes. It is deliberately **not** a second design
of input. `generaldesign.md` §14.1 to §14.4 already decide what a binding is,
where its fields go, and how it is looked up, and repeating that here would give
two documents one subject and a place for them to disagree.

What this document adds is the part `generaldesign.md` cannot: the file-level
correspondence, and the honest list of what is still not decided. Stage 4 in
`README.md` is "set up basic input" and stage 10 is "implement more advanced
input", so §6 below is the split between them.

## 1. How each answer here was chosen

Same ordering `windows.md` §1 uses, for the same reason.

1. **A document that already owns the subject wins.** `generaldesign.md` §14,
   `helpers.md` §6 and §7, `configstorelayout.md` §4, and `configstorage.md` §4.
2. **Otherwise Mango is the shape.** `mango-dev` at `d5a0e1e`, branch
   `config-scriptability`, which is the revision `research/mango.md` indexes. A
   working implementation of a window manager's input stack is worth more than an
   argument about what one should look like, so where the existing documents are
   silent, the shape of what Mango did is adopted and the commit is cited so the
   claim can be checked.
3. **Otherwise derive it, and say so.** Where the subject is ours and Mango has
   no equivalent at all, the shape comes from the rest of the design, and §7
   names which derivation.

## 2. The port in one paragraph

`generaldesign.md` §14 decided that input is Mango's implementation ported and
adapted, and the inversion is the reason this is cheap. Input was going to be
omniWM's own matching layer written against wlroots signals; instead it is a
port of code that already does that against the same signals. wlroots still
supplies the device plumbing, and what changes is the matching above it. The
whole port is 4,775 lines across 14 files, of which the binding-matching loops
are the only part that is replaced rather than moved, because those are the part
`generaldesign.md` §14.4 replaces with a hash index.

## 3. File map

Mango's `include/mango/input/` and `src/input/` at `d5a0e1e`, against
omniWM's `include/input/` and `src/input/`.

| Mango | omniWM | what happens |
|---|---|---|
| `device.c` (86) | `devices.c` | **port.** The `InputDevice` record, the new-device dispatch, and the destroy handler. The device-type switch becomes the set of sub-components §4 names. |
| `keyboard.c` (958) | `keyboard.c` | **port, minus one loop.** Everything except `keyboard_check_keybinding`, whose scan becomes an index probe. `find_device_rule` stays, because device rules are a Mango concept this port keeps. |
| `pointer.c` (1,497) | `mouse.c` | **port, minus two loops.** The largest file, and most of it is not matching: move/resize, constraints, drag, cursor shape, and the `is_trackpad` split. The `MouseBinding` and `AxisBinding` scans become index probes. |
| `trackpad.c` (1,046) | `trackpad.c` | **port, minus two loops.** The gesture recognisers (swipe, pinch, hold) port whole, and both `GestureBinding` scans become index probes. |
| `switch.c` (63) | `switch.c` | **port.** The lid switch and its `SwitchBinding` scan, the scan becoming an index probe. |
| `tablet.c` (431) | `tablet.c` | **port, and extended in stage 10.** Device plumbing ports now; the stylus binding type does not exist and is §6's job. |
| `touch.c` (363) | `touch.c` | **port.** Touch-to-pointer emulation, which Mango does well and which is not a binding kind at all. |
| (none) | `binds.c` | **fresh.** The index from `generaldesign.md` §14.4, the journal maintenance, and the one dispatch path all five kinds share. This is the file that has no Mango ancestor. |
| (none) | `input.c` | **fresh.** The component descriptor, its `wm.input.*` options, and the wlroots listener registration that `device.c` currently does implicitly. |

Line counts are at `d5a0e1e` and are there so the size of each piece is visible,
not as a target.

`filestructure.md` §"The remaining src directories" declined to sanction the
empty headers that were on disk and left the decision here. The table above is
that decision. It **replaces** the previous set: `binds.c` and `input.c` stay,
`devices.c`/`keyboard.c`/`mouse.c`/`tablet.c`/`trackpad.c` stay, `gestures.c` goes
because a gesture is a trackpad event rather than a device, and `switch.c` and
`touch.c` are added because Mango has both and dropping either would mean
inventing a replacement.

The rename `pointer.c` to `mouse.c` is a naming decision, not a correction.
Mango's file handles mouse buttons, the wheel, and the pointer cursor together,
so "pointer" is the more accurate name for it, but a reader opening `mouse.c` is
looking for the mouse and "pointer" would not tell them the wheel is in there.
The skeleton already said `mouse.c` and it is the better of the two for the
person who has to guess.

## 4. One component, and what it registers

`helpers.md` §3 makes each `src/<dir>` a component, and `helpers.md` §9 puts
input at priority `100`, below core and the config facade and above tags and
windows. That ordering is load-bearing in one direction only: a binding names an
action, so input needs the action registry to exist, and it does not need tags or
windows to exist to hold a binding, because `generaldesign.md` §14.1 makes a
binding's action name resolve on use rather than at set time (`helpers.md` §6.2).

The descriptor's shape is `helpers.md` §3.1's. The parts that are specific here:

- **`enable_key = "wm.input.enabled"`, default `true`**, per the `wm.<component>.enabled`
  convention in `helpers.md` §3.3. Input can be torn down at runtime, so it
  suspends rather than declaring `NULL`.
- **Options** are the device-rule-shaped keys and the two timing knobs Mango
  carries as globals: `wm.input.axis_apply_timeout_ms` (Mango's
  `axis_bind_apply_timeout`, default 100, clamped 0..1000) and
  `wm.input.key_order`. Everything else Mango keeps in `Config` is either a
  binding, which is a catalog entry, or a device rule, which is §5.
- **Actions** are the things a binding can name, and they are not declared by
  this document. They belong to whichever component owns the subject: `wm.focus`
  and `wm.snap` are `windows.md`'s and `layoutengine.md`'s, `wm.tags` is
  `tags.md`'s. This is the seam `generaldesign.md` §14 calls the second one, and
  it is why `input` registers almost no actions of its own: it names the
  registry, it does not populate it.
- **Triggers** are `BINDS_UPDATED` (`OMNI_JOURNAL_KIND_BINDS_UPDATED`, id 5) and
  the `KEY_SET` pattern, per `configstorelayout.md` §8. `KEY_SET` is the
  incremental path and `BINDS_UPDATED` is the group invalidation;
  `generaldesign.md` §14.4 explains why both exist, because one rebind and a
  two-hundred-binding reload are one event to a client that only needs to know
  its index is stale.

Suspending `input` means: stop listening to devices, drop the index, and release
the seat's capabilities. It does not mean unregistering the action names, because
those belong to other components.

## 5. The three seams

`generaldesign.md` §14 names three places where the port is not verbatim. This
section is where each one lands in code, because that is the information
`generaldesign.md` does not have.

### 5.1 The block replaces Mango's config arrays

Mango's `Config` carries five arrays (`key_bindings`, `mouse_bindings`,
`axis_bindings`, `switch_bindings`, `gesture_bindings`) plus
`ConfigDeviceRule *device_rules`, all realloc'd during parse. Under
`generaldesign.md` §14.1 none of them are storage. The five arrays become
catalog entries, one per binding, and `binds.c` is what reads them.

**Device rules are the part this seam does not cover, and they need a home.**
Mango's `ConfigDeviceRule` matches on device name or a `vendor:product:name`
identifier, or on device type, and then carries libinput and xkb settings
(`parse_config.h:214-250`). It is a match-and-apply record, which is what
`client_rule` and `rule` already are in the block, so it is stored as a
`client_rule`-shaped record under its own namespace rather than as a sixth
binding kind. The decision of which tag it is, and where the namespace sits, is
owed and is listed in §7.

### 5.2 Actions replace function pointers

Every one of Mango's five binding structs has
`int32_t (*func)(const Arg *)`, and every match site calls `k->func(&k->arg)`.
Two things change, and they are the whole of this seam:

- **The pointer becomes a name.** `func` becomes `action_ref`, a frame offset
  holding the action's name, per `helpers.md` §6.2. Resolution happens on use,
  and a name that does not resolve is `ACTION_NOT_FOUND` on use.
- **`Arg` becomes the positional `args` array.** Mango's `Arg`
  (`dispatch/bind.h:8-19`) is ten named fields: `i`, `i2`, `f`, `f2`, `v`, `v2`,
  `v3`, `ui`, `ui2`, `tc`. It is a struct with a fixed shape and no schema, and
  each handler reads whichever fields it cares about and ignores the rest. The
  `args` array is positional and typed, validated against the action's declared
  schema from `helpers.md` §6.1 before the handler is entered.

This is the seam that is not mechanical, and it is worth being precise about why.
Mango's `Arg` is a bag that every handler knows how to read by convention, so a
handler that takes three arguments and one that takes none share a type. The
`args` array is a contract declared by the action, which means the five `v`
fields collapse into one positional list and a handler cannot ask for the fourth
string of a two-string call. Every `Arg` use in `bind.c` therefore has to be
read against the handler's actual arity, and that reading is per-handler, not
mechanical. `bind.c` is 2,674 lines and it is the largest single piece of work in
this port for exactly this reason: it is not a port, it is a translation of 2,674
lines of function pointers into names and schemas.

The `tc` field is the interesting one, because it is a `Client *` and not a
value. Under `windows.md` §2 a client is an `entry_ref`, so a `tc` argument
becomes an `entry_ref` argument and a `NULL` one becomes `OMNI_REF_NONE`.

### 5.3 Namespaces and tags replace modes

Mango's binding has `char mode[28]` plus `iscommonmode` and `isdefaultmode`, and
`set_binding_keymode` (`parse_config.c:261-274`) folds the three cases into those
three fields at parse time. The match is
`iscommonmode || (isdefaultmode && server.key_mode.isdefault) ||
strcmp(server.key_mode.mode, k->mode) == 0`, and it appears verbatim in all five
scan sites.

omniWM has no key mode. A binding names an action, and an action operates on a
tag or a client by name, so "which mode are we in" is not a question the action
layer asks. What survives is the *scoping* question: which bindings apply to a
given event, and the answer is the namespace, not a global mode string.

**This seam is the one that is not decided.** `generaldesign.md` §14 lists the
mapping from Mango's modes and tags onto this project's namespaces as still owed
on substance, and this document does not close it, because closing it is a
decision about what a binding is scoped to rather than a porting question. The
options and what each costs are in §7.1.

## 6. Stage 4 versus stage 10

`README.md` splits input across two stages and the split has to be a real one,
because stage 5 needs windows on screen and stage 10 is a long way off.

**Stage 4, "set up basic input":** the device plumbing, the block-backed binding
records, the index, and the keyboard path. Specifically: `devices.c`,
`keyboard.c`, `binds.c`, `input.c`, and the `wm.input.*` options. A keybind
resolves an action by name and an action that does not exist yet is inert, which
`helpers.md` §6.2 already decided is the correct behaviour rather than a fault.

**Stage 10, "implement more advanced input":** `mouse.c`, `trackpad.c`,
`switch.c`, `touch.c`, `tablet.c`, and the stylus binding type.

The reason the pointer and trackpad files wait is not that they are harder. It
is that `pointer.c` and `trackpad.c` are the files that reach into `Client` and
`Monitor` structs directly (`find_closest_tiled_client`, `pointer_place_drag_tile`,
`pointer_warp_to_monitor`), and at stage 4 there are no clients and no monitors
yet. Porting them at stage 4 means porting code that cannot compile against the
types it names. `keyboard.c` has no such dependency, which is the whole of why
the split falls where it does.

**The stylus binding type is a genuine gap, not a porting task.** Mango has
`tablet.c` for the device plumbing and the wlroots `tablet_v2` protocol
throughout, and it forwards pressure, distance, tilt and rotation to the client
(`tablet.c:365-380`), but it has no binding struct for a stylus: there is no
`StylusBinding`, so there is nothing to copy. A stylus binding needs pressure and
tilt thresholds, absolute versus relative pointing, and an annotation mode, and
none of those have a Mango equivalent. This is the one input feature that is
fresh work rather than a port, and it is why `configstorelayout.md` §4 defers the
`binding` record's offsets: the header grows again for stage 10, so fixing
offsets at stage 4 would fix them against a shape stage 10 revises.

## 7. Not decided here, and what is still open

Not covered, deliberately:

- **The mode and tag mapping** (§5.3). This is the substantive gap. See §7.1.
- **Whether `binds.c` is one component or five.** It is described here as one
  because the index is one and the record is one, but the five trigger kinds
  could each be a sub-component with its own `enable_key` if per-kind
  suspension turns out to be wanted. Nothing needs it at stage 4.
- **Key repeat policy.** Mango's `keyboard_repeat` and its
  `key_repeat_source` port with the file, and whether repeat should fire bindings
  at all is not a porting question and is not asked here.
- **The seat's capabilities and virtual devices.** Mango's
  `handle_new_virtual_keyboard` and `handle_new_virtual_pointer` port with their
  files. What an extension may do through a virtual device is
  `helpers.md` §6.3's question, not this document's.

### 7.1 The mode and tag mapping, and why it is not a porting question

Mango's `mode` is a 28-byte string with two sentinels, `common` and `default`,
set by a binding at parse time and switched at runtime by the `set_key_mode`
action (`bind.c:900-910`). A binding is in scope when the current mode matches its
string, or when it is `common` and therefore always in scope.

omniWM has no global mode. The question that replaces it is **what a binding is
scoped to**, and the candidates are the namespaces that already exist:

| option | what `mode` becomes | what it costs |
|---|---|---|
| drop scoping | nothing; every binding is always in scope | loses `common`/`default` and with them the ability to have a binding that only applies in one context. Simplest, and it is a real loss. |
| scope to a tag | a tag name or `entry_ref` | a binding that applies on one tag. But a tag is where windows are, and Mango's mode is a global focus context, so this changes the meaning rather than translating it. |
| scope to a client | an `entry_ref` | a binding scoped to one window, which Mango has no equivalent for at all. This is a new feature wearing an old name. |
| keep a mode string, renamed | a namespaced string like `wm.input.modes.<name>` | the closest to a port, and it keeps a global mutable string, which is the thing `tags.md` and `windows.md` are built to avoid. |

None of these is obviously right, which is why the seam is listed as open rather
than resolved. The one thing that can be said without deciding: `generaldesign.md`
§14.4 already puts `mode_id` in the index key, so whatever this becomes, the
index is already shaped for an integer mode and the answer is a u32 rather than a
string compare on the hot path. That is a consequence of §14.4, not a decision
about what the mode means.

### 7.2 Smaller open items

- **Device rules have no type tag. Deferred, and not phase 02's to pick.** §5.1
  places a device rule as a match-and-apply record, and the `client_rule` tag at
  `0x33` cannot be reused because its fields are layout vocabulary, so a new tag is
  needed. The number itself is trivial; what is not trivial is that it is an ABI
  addition to the block, which makes it substrate rather than input. **It is
  decided when the input design is fleshed out**, and the reason it cannot be
  decided earlier is that fleshing it out is what requires knowing the input
  design: a device rule's record shape is only knowable once the rest of input is,
  and picking a number for a shape that is still moving is how two phases end up
  asserting different ones. So neither `03-input.md` item 2 nor phase 02 claims the
  number, and `configstorelayout.md` §4's vocabulary and the static assert in
  `include/shared/omni_layout.h` are written when the number is. The namespace is a
  separate and easier question, since a device rule is keyed by device and
  `wm.input.device.<n>.*` follows from the same convention as the other options.
- **`isallowconflict` is not carried, and the binding match test belongs to phase
  03.** The rule `generaldesign.md` §14.4 now states is that ordering inside an
  index bucket is config order and the **last** match wins. That is not a
  tie-break invented for the index: a binding is an ordinary catalog entry, so a
  later `set` replaces an earlier one — over the socket and from the config
  parser writing through the block on the backend alike — and last-wins is simply
  that overwrite arriving at the index. It is also what lets a config file read as
  a script, where an assignment overrides what came before it rather than
  competing with it.
  Mango's `isallowconflict` means the opposite thing, that the scan continues past
  a match so an earlier binding defers to a later one, and it only has meaning
  under first-match resolution. We do not carry it, and the reason is a policy
  rather than a technicality: **we are not Mango-compatible out of the box and are
  not trying to be.** The config format is ours and differs, so a Mango config is
  a different document rather than a near-miss, and building compatibility into
  the native format would mean carrying flags for a format we do not otherwise
  match. `12-languages.md` owns a Mango interpreter for omniWM — a small program
  over our own value types — and Mango's semantics are reproduced there, so a
  Mango config behaves like Mango through that path and our format stays clean.
  **Owner: `03-input.md`.** The index key, the bucket layout and the trigger
  resolution walk are that phase's subject and are not designed yet, so the test
  follows the design rather than preceding it. This bullet was previously owned by
  phase 01 item 5, which was a misassignment: phase 01 owns the framework and the
  seam, and a test of an undesigned index would have had to guess its shape.
- ~~**The `spec` field.**~~ **Moot: the field is not carried.** Mango keeps
  `char *spec`, the config line a user would edit, on every binding, and it exists
  there as a workaround for a storage model that could not hand a binding back over
  its IPC: the binding lived in process memory, so the source text was the only
  handle a human had on it. `generaldesign.md` §14.1 now drops it alongside
  `line_number` and `file_index`, because in a block the handle is the key and the
  record is the value, both of which `ipc.md` §4's `get` returns verbatim. There is
  no field to place and therefore nothing this document owes for it, and no
  `get binds` wrapper is wanted for the same reason: there is nothing a binding
  would return that the general `get` does not already return.

## 8. References

| reference | use |
|---|---|
| `generaldesign.md` §14 | the use-Mango's-code-where-we-can decision, the three seams, and §14.1 to §14.4 the field map, the block/index split, the measured scan cost, and the index |
| `helpers.md` §3, §9 | the component descriptor, the `wm.<component>.enabled` convention, suspension, and input's priority of `100` |
| `helpers.md` §6, §6.1, §6.2 | the action registry, the argument schema, and `action_ref` as a name resolved on use |
| `helpers.md` §7.2 | `omni_regex_match` as the single PCRE2 engine, which gesture patterns would reach for |
| `configstorage.md` §4 | the `binding` tag, the `modmask` and `keysym` type tags, and the positional `args` array |
| `configstorelayout.md` §4, §8 | the `binding` record at `0x2B`, its known-too-small header, the deferred offsets, and the `BINDS_UPDATED` and `KEY_SET` journal entries |
| `windows.md` §2, §9 | a client as an `entry_ref`, which is what a `tc` argument becomes |
| `tags.md` | a tag as a catalog entry, which is one of the scoping candidates in §7.1 |
| `ipc.md` | the socket surface, and §4's general `get`, which is how a binding is read now that `spec` is not carried (see §7.2) |
| `filestructure.md` | `src/input`'s place in the tree, and the unsanctioned decomposition this document replaces |
| `layoutengine.md` §7.6 | floating snap, whose direction argument arrives with the port rather than waiting on the solver |
| Mango | `include/mango/input/` and `src/input/` at `d5a0e1e`, the five binding structs in `parse_config.h:61-302`, `Arg` in `dispatch/bind.h:8-19`, `set_binding_keymode` in `parse_config.c:261-274`, `find_device_rule` in `keyboard.c:33`, and the absence of a stylus binding struct |
