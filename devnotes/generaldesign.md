# General design (devnotes/generaldesign.md)

The high-level design of omniWM: what the project is for, what it promises, how
the promises are met, and where the boundary of every subsystem sits. This
document owns the shape of the whole system. It deliberately holds nothing
detailed; a contract stated in detail here and in a subsystem document is a bug,
and the subsystem document wins. The established contracts are already owned
elsewhere and are linked, not restated: the store is `configstorage.md` and
`configstorelayout.md`, the component substrate is `helpers.md`, the socket
facade is `ipc.md`, config file binding is `tomlparser.md`, the server half of
the lifecycle is `server.md` §1–§2, and directory boundaries are
`filestructure.md`.

What this document does own is the compositor. `architecture-audit.md` §8
deferred every compositor subsystem finding, and the source tree is empty
scaffolding, so the compositor's shape has to be stated somewhere. It is stated
here at the level of intent, data flow, and ownership, and each subsystem gets a
later document that refines it rather than originates it.

## 1. What the project is

omniWM is a Wayland window manager built in C whose organising idea is that the
compositor should expose a mechanism and let everything user-facing be built on
top of it. The three headline promises from `README.md` are Any Language, Any
Layout, and Any Look. Each is a statement about where the power lives.

- **Any Language.** Nothing about the user experience lives in C config code. A
  user reaches the compositor through a stable shared-memory interface, so a
  config in any language is an external program, not a fork.
- **Any Layout.** Tiling, scrolling, floating, and canvas layouts are not
  hardcoded algorithms. They are constraint programs the user writes.
- **Any Look.** Decoration, animation, backgrounds, and custom UI are not a
  fixed option set. They are nodes in a scene graph the user configures.

There is a fourth, quieter promise that the first three depend on: the
compositor itself is not special-cased. The same mechanism that lets a user
write a Mango config also lets the compositor ship its own defaults, and the same
mechanism that lets a user restyle a window also lets the compositor draw its
own. There is no privileged internal path.

## 2. Why it is built this way

Every structural choice below exists to keep one property true: the compositor
does not need to be modified to add a feature a user wants. Three consequences
follow, and most of the design is these three ideas applied.

- **The interface is data, not code.** If the interface between the compositor
  and everything else is a fixed block of memory with a documented layout, then
  any language can speak it by reading one header. If the interface were a C
  function table or an internal struct, every new language would need a change
  to the compositor. This is why the store is the centre of the system and why
  `filestructure.md` treats `include/shared/` as a header-only ABI surface that
  must compile without the compositor, without wlroots, and without a C
  toolchain that knows this project.
- **A user-facing *optional* feature is a component, not a branch.** What makes it
  optional rather than user-facing is necessity, not taste: if the compositor
  cannot manage windows without it, it is a core service rather than a component,
  which is the rule §5.1 states and which is why the layout engine is not an
  exception to this one. If a feature is a component exposing options, actions,
  and triggers, then it can be enabled,
  disabled, and reconfigured at runtime without a restart, and a user who does
  not want it pays nothing. This is `helpers.md`'s component model.
- **The layout engine is a solver, not a set of layouts.** If layout is a
  constraint program, then a layout nobody anticipated is a config file rather
  than a patch. This is what makes Any Layout true rather than aspirational.

## 3. Foundation

omniWM is built on wlroots and scenefx, and is expected to extend both.

wlroots supplies the parts a window manager should not write: the DRM and KMS
backend, session and VT handling, the allocator, dmabuf, explicit sync, the
swapchain, the EGL and GLES2 setup, damage tracking and frame timing, direct
scanout, the output layout, headless and noop backends for testing, and the
input plumbing for libinput, keyboards, pointers, touchpads, touchscreens,
tablets, tablet pads, tablet tools, and switches. It also supplies roughly
forty-six Wayland protocols, including the ones that are tedious to get right:
pointer constraints, relative pointer, keyboard shortcuts inhibit, virtual
keyboard and virtual pointer, and the whole Xwayland integration.

scenefx replaces the wlroots scene graph with a renderer that can blur, drop
shadows, round corners per corner, clip to a region, and apply gradients. It
keeps the upstream `wlr_scene` and `wlr_scene_tree` names, so the code and the
design patterns transfer directly from wlroots-based compositors.

Two dependencies are version-coupled and this is a known constraint on the
build: scenefx 0.5 requires wlroots 0.20, and the two are developed together.
Any work that starts the build must settle which wlroots release omniWM tracks.

**The largest technical risk to Any Look is stated here so it is not
rediscovered later.** scenefx 0.5 has no public API to register a user shader or
attach a filter to a node, and it has no 3D transforms at all: a node carries a
2D affine matrix and nothing more, and its own header says the scene-graph API
supports only basic 2D composition and that anything more complicated requires
compositors to implement custom rendering logic. omniWM wants user GLSL shaders
and 3D animation, so both require extending scenefx with a new node type and a
new render-pass entry point. This is the one place where "extend the base
library" means "maintain a patch", and the cost should be scoped deliberately
rather than discovered at stage 11.

## 4. The architectural spine: one authority, many clients

The whole system is one shared-memory configuration block, owned by the server
for the process lifetime, described in `configstorage.md` and
`configstorelayout.md`. Every other surface in the project is a client of that
block and nothing else.

```
                 ┌──────────────────────────────────────┐
   config file → │                                      │
   TOML parser → │        the shared config block       │ ← compositor
   socket API  → │        (the only authority)          │   components
   external lib → │                                      │
   interpreter  → └──────────────────────────────────────┘
```

The properties this buys, and the reason the design is worth its complexity:

- **The store is an open catalog.** It has no key-specific storage logic and no
  schema. A key nobody has heard of is stored and served verbatim. This is what
  lets a user keep their own state in the block under their own namespace, and
  it is why a language nobody has written yet works.
- **Every write path is identical.** The built-in TOML parser is a facade, not a
  privileged path: it runs in-process, but it is a client of the same block, and
  it has no write capability a socket client lacks. A value written from a
  config file and the same value written over IPC are byte-identical in the
  block. That property is what lets a user move between the config file, the
  socket, and the block without a semantics change, and it is the reason the
  compositor can be driven by a config format written for a different window
  manager.
- **A config file is a stream of operations.** A file is a sequence of `set` and
  `exec` operations, not a state dump. That makes the file's effect order
  explicit and makes an external program's writes the same kind of thing.
- **Writes are serialised, not locked into a design.** Compositor execution is
  single-threaded on one event loop; the block's futex serialises external
  writers. So a component never contends with a socket client, and a socket
  client never sees a half-applied commit.
- **The socket is optional.** Its absence degrades the project to a lower skill
  floor without making it unusable, because the block is authoritative.
- **There is no plugin ABI.** Components are compiled in. Extensibility is data
  and the socket, not dynamic loading, per `helpers.md` §8.1. This keeps the
  extension boundary to one stable interface rather than two.

## 5. The compositor core

The compositor is a single-threaded wlroots event loop owning a display, a
scene, a renderer, the store, and a component registry, per `server.md` §1. Boot
order, readiness, dispatch, and shutdown belong to `server.md` and are not
restated here.

### 5.1 What is a component and what is core

"Every user-facing capability is a component" is only half the rule, and the half
that is missing is what made `layoutengine.md` §2.7 ask whether the layout engine
is a component at all. The test is not whether a capability is user-facing but
whether the compositor can run without it:

- **Core service**: anything the compositor needs in order to be a compositor.
  The layout engine is one, because without a layout there is nowhere to put a
  window and `server.md`'s boot sequence has nothing to lay out during activation.
  So are the store, the input router, the output and surface management, and the
  component registry itself. These are not components, are not registered in
  `helpers.md` §9's table, are not toggleable, and are not in the activation
  order: they are the thing the activation order runs inside.
- **Component**: an optional capability layered on that core, which a user turns
  on because they want its behaviour and pays nothing for it when they do not.
  Visuals belong here, so decoration, effects, and the optional visual parts of
  theming are components. Optional layout features belong here too: an extra
  built-in layout is a component, because the core needs the *mechanism* to place
  a window and does not need any particular layout to be compiled in.

The distinction is what keeps the two sentences consistent. A capability can be
optional and still not be a component, and the case that proves it is the layout
engine: it is a general service that no user configures, so under "every
user-facing capability is a component" it looked like an exception, and the
exception invited the wrong fix, which was to make it a component. It is not an
exception, it is a core service, and the rule above says so.

The test is deliberately about necessity rather than about taste, because taste
is not checkable and necessity is: ask whether disabling this capability leaves
the compositor able to manage windows, and the answer decides. A visual effect
fails the test and is a component. The layout engine passes it and is core.

Every component exposes the same three universals, per `helpers.md`:

- **options** are the values a component reads to decide how to behave;
- **actions** are the things a client, a binding, or a rule can invoke;
- **triggers** are the events a component can subscribe to, so behaviour can be
  attached to the system rather than hardcoded into it.

Components register descriptors, activate in priority order (bands in
`helpers.md` §9), seed their options, and can be toggled and reconfigured at
runtime. A component that is disabled releases its subscriptions and its
behaviour stops without affecting the rest of the compositor.

This is what makes the compositor non-special-cased in both directions: the
compositor's own defaults are components reading the same options a user would,
and a user's custom component-level behaviour is the same mechanism.

## 6. Tags

Tags are the project's model of "where a window is", and the one place where
omniWM makes a deliberate, and now differently-shaped, break with MangoWM.

Mango's tags are bit positions in a 32-bit mask. A client holds a mask of tag
numbers, and each tag's per-tag layout state lives in a fixed per-monitor array
indexed by tag number. That structure is fast and has no allocation, and it is
why Mango can mirror a tag onto several monitors: one bit, set on three monitors
at once, over three separate state arrays.

omniWM keeps Mango's per-monitor array and drops everything built on top of it.
A tag is identified by the pair (monitor, tag number), and the pair is the whole
of the identity: monitor 1's tag 3 and monitor 2's tag 3 are unrelated tags.

- **A tag is displayed on at most one monitor at a time, and a per-monitor tag's
  monitor is part of its identity.** A tag owns its client set, its own layout
  state, and the other data needed to view it, and a per-monitor tag lives under
  its monitor's entry in the store, so the monitor half of its identity is
  structural rather than a value somebody has to keep consistent. It does not own
  floating
  state: which windows float is a fact about a client and is carried by the
  client, so a client on three tags floats on all three or on none. `tags.md` §6
  owns the argument. The rule is "at most one" and not "exactly one" because a tag
  that nothing is displaying is a tag holding windows the user stepped away from,
  and because the scratchpad below is a singleton with no monitor in its identity.
- **A monitor displays an *ordered list* of its own tags rather than a set**, and
  the first tag in that list which is still set is the monitor's primary, which
  supplies the layout and everything else the arrangement needs. A secondary tag
  contributes its clients and nothing more. This is `tags.md` §4, and the ordering
  is what makes a primary derivable rather than a second stored fact that could
  disagree with the list. Several of a monitor's own tags displayed at once is a
  multi-tag view and is just a list with more than one entry.
- **Mirroring is not a feature this design gives up; it is the bug the per-monitor
  identity makes unrepresentable.** A client expects exactly one surface on exactly
  one output at a time. Mirroring asks a client to present on two, which nothing in
  the protocol lets it honour: one `wl_surface`, one set of buffer bounds, one
  enter/leave history per output. Under this identity there is no state that can
  express it, which is a stronger position than declining to implement it. The
  previous version of this section listed mirroring as a *benefit* of a shared tag
  entity and used it to reject per-monitor state, and that argument ran backwards.
- **There is no move, and what people call one is a content swap.** Because a
  per-monitor tag's monitor is part of its identity, no operation carries a tag to
  another monitor. The user-facing operation is an exchange of two tags' contents,
  which is Hyprland's and i3's move, and it is one IPC command publishing one
  grouped commit so no reader sees a half-finished swap. `tags.md` §7.

A monitor's view is therefore a set of that monitor's own tags rather than a
single index, and a window's visibility is whether it is in any of the tags that
monitor displays.

This gets the benefits of both references without the parts of either that do not
work here. From Mango: per-monitor tags numbered from 1 on each output, and
multi-tag views, which fall out of a monitor's list having more than one entry.
From Hyprland and i3: "get this over there", as a content swap. What is rejected:
Mango's bitmask and client-held mask, because the mask is the mechanism that makes
a tag number global; and Hyprland's single shared workspace set, because a
three-monitor user who wants three monitors to look like one workspace ends up
treating workspaces 1, 11 and 21 as a single combined workspace, which is a janky
model that per-monitor numbering removes.

Two tags are special, and they are special in different ways, so they are not a
matched pair:

- **Overview** is a special tag that displays its monitor's tags' windows at once,
  laid out as a grid of cards rather than by the active layout, so the user can
  see and jump to anything. It is not a separate view mode with its own state
  machine; it is a tag whose contents are the union of the others, which makes it
  the degenerate case of the list rule above. One per monitor.
- **Scratchpad** is a special tag whose *arrangement* is exclusive to itself: no
  other tag's layout places its clients, because it holds a **layout of its own**
  and is an **overlay workspace**:
  summoning it places its clients by that layout over the top of the monitor's
  existing view rather than replacing it. It can be **one shared tag across all
  monitors, in which case it is openable on at most one monitor at a time**, or
  **one per monitor**, and which of those is in effect is a config key a TOML user
  sets once and never thinks about again; a user driving the compositor over SHM
  can arrange it by hand, because a scratchpad is an ordinary tag entry. Because it
  is a tag, `swap_tags` moves a whole tag's worth of clients in or out of it in one
  operation. `tags.md` §8.

The scratchpad is the one case where the shared option is right, and it is right
for the same reason mirroring is wrong: a shared scratchpad is one tag that is
open on one monitor at a time, which is exactly the "at most one monitor" invariant
applied to a tag that is not pinned to a monitor. Making it per-monitor is not a
different feature, it is the same code with N tags.

Both special tags hold clients that are also on ordinary tags, and both animate,
decorate, and bind like any other tag, so the exclusivity above is about
placement and never about membership. The two differ in what that overlap means:
overview is a *union*, so an ordinary member appears in it and appears twice, and
the scratchpad is *outside* the union, so an ordinary member is skipped by its
ordinary tag's layout and placed only by the scratchpad's own. Membership is
retained in both cases, which is what makes dismissal work without a stored copy of
where the client came from. `tags.md` §8.2.

The alternative designs were considered and rejected, each for a reason that has
since changed and is worth restating so the rejection is not held for the wrong
one. **Keeping Mango's bitmask** is rejected because the mask is not merely a
compact membership set: it is the mechanism that makes a tag number global, and a
global tag number is the only thing that makes mirroring expressible, and mirroring
is the unsafe case. **A shared tag entity shown on several monitors** is rejected
because two monitors laying out one tag need either one arrangement they both share
or two state copies kept in step, and both are worse than two tags; the shared
entity was previously kept for its mirroring, so this rejection is the same one
with the reason corrected. **Hyprland's single shared workspace set** is rejected
not on correctness grounds but on the numbering: it forces a multi-monitor user who
wants one big workspace to combine workspace numbers, which is the jank per-monitor
numbering exists to remove. None of the three is rejected for being untidy; each is
rejected for preventing the one property the design needs, which is that a tag is
on one monitor or none.

## 7. Layout

Layout is a constraint solver, not a set of layout functions. This is the
mechanism behind Any Layout.

- **Constraints are soft and prioritised.** A layout is a generic program of
  soft constraints such as fills-parent, left-of, half-width, or gap-around, and
  it names no window: it constrains whatever set it is handed, in walk order. The
  named layout lives in the tag, and the windows carry their own state and
  overrides, so a window that deviates from the layout does so through its own
  values rather than through a layout written for it. A user can opt a window in
  or out, and window rules can adjust what a window overrides.
- **The solver assigns geometry.** On each arrange pass the solver takes the
  windows' constraints and produces a geometry for each. Nothing in the arrange
  path is hardcoded to a particular layout.
- **The solver degrades, it does not fail.** When the constraint set cannot be
  satisfied, the solver does not discard anything. Every constraint carries a
  weight, and the solver returns the geometry that minimises total weighted
  violation, so a user layout that is subtly wrong produces a slightly odd
  arrangement rather than an error, which is the right failure mode for a
  feature aimed at experimentation. Nothing is ever dropped, which is what makes
  this safe to expose: there is no drop order to define, no constraint to lose
  for good, and no way for a resize to change which constraints the solver
  believes in.
- **Built-in layouts are compiled in, and adding one is not a solver change.**
  Master/stack, scrolling, binary space partitioning, grid, and the floating and
  canvas styles the roadmap names are shipped in the compositor binary rather than
  in user config, and each one is written as a layout plus its registration over
  mechanisms that already exist. The test is narrow and worth stating exactly,
  because it is the thing that keeps the engine honest: adding a built-in means
  writing the layout, registering it, and recompiling. A built-in that needs a
  solver mechanism that does not exist yet is a claim that the mechanism was
  missing, and that claim belongs in the engine design first, where it gets
  designed once instead of accreted one layout at a time. Containers and
  viewports are the two mechanisms this already forced, for a group and for a
  scroller, and both were designed rather than per-layout.

Layout state is per tag, because a tag is the unit of "where windows are" and
owns its own layout, its own client set, and the rest of the data needed to view
it. Under §6's decision a *per-monitor* tag belongs to exactly one monitor, so
"per tag" and "per monitor" name the same storage for it and the old distinction
is spent: there is no per-monitor state to hold and no pairing to enumerate. The
qualifier is load-bearing rather than pedantic, because §6's rule is *at most* one
monitor and a singleton — the shared scratchpad — belongs to none, so it holds
layout state with no monitor to be per. It is displayed on one monitor or on none,
and that is a fact about where it is shown rather than about where it lives. A monitor does not hold
layout state, it displays an ordered list of its own tags of which the first
still-set tag is the primary,
and the primary's layout arranges the union of every displayed tag's clients. A
secondary tag contributes clients and nothing else. That is `tags.md` §4. What
replaced the old "a moved tag carries its arrangement for free" claim is narrower:
a tag's arrangement goes where its contents go, which is what makes the §6 content
swap carry a tag's layout and viewport along with its clients, and nothing more
than that.

Floating is not part of that. Which windows float is window state, so the client
carries it, and `tags.md` §6 owns the argument.

But a layout is not exclusive to tags: other components own nested layouts too. A
group is a nested layout. A scratchpad, a pop-up tag, and a cluster each lay their
contents out. The layout engine is therefore a general service, and tags are its
first consumer rather than its owner.

## 8. Windows, groups, and clusters

The window layer owns clients, the floating layer, focus and stacking, and the
per-window identity that decoration overrides key on.

- **Groups** are a nested layout. A group is picked up and moved as if it were a
  single window, and it has a titlebar that identifies it and can be used to
  cycle its members. A group is a nested layout, so its members obey the layout
  engine inside it. `layoutlanguage.md` §5 is normative for what a group is and
  what a program writes to make one.
- **Clusters** are several windows tied together so they move as one unit. The
  distinction from a group is that a cluster adds no chrome of its own: it is a
  movement relationship, not an internal layout.

The two exist because they are genuinely different intents: a group is a new
thing with its own internal layout and chrome, a cluster is several things that
happen to travel together. Treating them as one concept, as Mango does with its
flat group links, cannot express both.

A cluster **occupies one space of its parent**, which is an amendment to what this
section used to say and the reason is a bar. A cluster whose members each kept
their own place in the parent could not carry a group bar: the bar would be placed
by the parent wherever the parent liked and nothing would put it against its
group's edge. A cluster is therefore resolved to a single **virtual client** before
the program places anything, its members' boxes are the union of their minimums, and
the program sees one occupant however many windows are inside it. The members are
then placed inside the space that occupant was given, by the client rules on the
clients. `layoutlanguage.md` §3.8 has the pass, and it is why the update pipeline
in `layoutengine.md` §5 is nine steps rather than eight.

Window rules are declarative config that matches a window and seeds its state,
and they are the same mechanism used to seed a window's layout constraints and
decoration, described in `windows.md`.

## 9. The scene graph

There is one scene graph, and everything visible is a node in it: client
surfaces, borders, shadows, overlays, underlays, gradients, text, buttons,
backgrounds, vector lines, and the compositor's own drawn widgets. One graph
means one place where effects and animation apply, and it means a decoration and
a background are animated by the same code that animates a window.

The scene is owned by the server and rendered per output. Nodes are typed by what
they draw, and each type carries the properties the effect and animation layers
need.

## 10. Animation

Everything that can be animated is animatable. Rather than a fixed set of window
transitions, animation is a property of scene nodes driven by timelines declared
in config.

- **Animatable properties** are exposed on nodes, and a timeline maps a property
  over time with a curve. A window moving, a border changing colour, a node
  fading, and a layout shifting are all the same mechanism.
- **Both 2D and 3D** are supported. 2D covers translation, scale, and opacity;
  3D covers rotation and perspective, which requires extending scenefx as noted
  in §3.
- **Shader-driven animation** is supported, and is the same extension point: a
  shader can be a property source, so an effect can be animated rather than
  only configured.

The reference model this replaces is Mango's, which animates a single scalar
factor over an axis-aligned box with a baked curve per phase. That model cannot
express per-property curves, 3D, or a stagger, all of which are wanted here. The
timeline model is chosen to make those ordinary rather than special.

## 11. Decoration

Decoration is configured globally by default, and any window can be
individualised. The goal is to support everything MangoWM and Hyprland support,
and then the things neither does, with multi-layering throughout:

blur; borders that are multi-layer and can be gradients or textures; window
overlays and underlays; dimming; shadows; glow; opacity; rounding; and custom
GLSL shaders.

There are three routes to a per-window override, and they converge on the same
place:

- a dispatch action that applies a decoration to one window at runtime;
- an external program writing the override directly over the shared block;
- a declarative window rule in config that seeds the override when the window
  appears.

The per-window override is stored as a `WINDOW_DEPENDENT` catalog entry keyed by
window identity, so all three routes write the same data and the decorate
component resolves it the same way. This is what keeps "restyle a live window
from a script" possible without a privileged path.

## 12. Drawing and the compositor's own surfaces

The drawing layer is what produces every pixel the compositor draws itself:
backgrounds, text, images, vector lines, and the shaders behind them. It also
produces the compositor's own widgets, described next.

- **Backgrounds** may be a flat colour, an image, a shader, a vector overlay, or
  a panning infinite canvas, and the roadmap treats these as options settled at
  stage 13.
- **Vector lines** are first-class objects with position, colour, width, and
  animation, not a decoration of a background image.
- **The compositor's own drawn surfaces**, the group titlebar, the jump label,
  switcher previews, overview cards, split indicators, and the drag icon, are
  produced by this layer, so they animate and take effects like any other node.

## 13. Fake clients

A fake client is a widget the compositor draws that behaves like a window. Bars,
panels, launchers, on-screen displays, and counters are fake clients.

The point of calling them clients rather than decorations is that they take part
in layout and in focus exactly as a real window does. A bar is a window that the
compositor happens to draw, so it can be tiled, floated, tagged, and animated
through the same code, and it can be driven from config or a script the same way.
This is a large part of how the project reaches the level of custom UI the
`README.md` describes without a special UI layer.

## 14. Input

**Decided: we use MangoWM's code where we can, and change what we need.** The
reuse covers the keyboard, mouse buttons, the wheel, touchpad gestures, the
switch (lid) bindings, and per-device rules, which is Mango's
`include/mango/input/` and `src/input/` at `d5a0e1e`: `device.c`, `keyboard.c`,
`pointer.c`, `switch.c`, `tablet.c`, `touch.c`, `trackpad.c`, and the five
binding structs behind them (`KeyBinding`, `MouseBinding`, `AxisBinding`,
`SwitchBinding`, `GestureBinding`). Tablet and stylus are the deliberate gap:
Mango has `tablet.c` for device plumbing but no stylus binding type, so pressure,
tilt, absolute pointing and annotation mode are the extension work, and stage 10
rather than stage 4.

"Use where we can, change what we need" is the whole of the policy and it is
stated as a policy rather than a summary, because the word that used to stand in
for it was doing more damage than the decision. Calling the port "wholesale"
asserted a completeness nobody had checked and nobody could check without reading
both trees line by line, so a reader who took it at face value would look for a
place to check the port and find only the word. The three seams below are the
actual extent of the changes, and they are what a reader should audit against
instead.

This inverts the earlier position. The design had input as omniWM's own matching
work on top of wlroots, and that is now a rewrite of code that already exists
and works against the same wlroots signals. Where the port cannot be verbatim,
the reason is one of these three:

- **The block replaces Mango's config arrays.** Every binding is one catalog
  entry, so a binding becomes a value in the store rather than a row in
  `Config`. `generaldesign.md` §14's one-entry-per-combination rule is kept and
  is what makes the swap possible: Mango's five arrays become five prefixes.
- **Actions replace function pointers.** Mango's `func` field is an
  `int32_t (*)(const Arg *)`; omniWM's is a namespaced action in the registry
  (`helpers.md` §6), with `Arg` becoming the positional `args` array. This is
  the seam the port has to get right and the rest of the port is mechanical.
- **omniWM's namespaces and tags replace Mango's modes and tags.** A binding
  names an action, and an action operates on a tag or a client by name.

### 14.1 All binding data lives in the block

**Decided: every field of every binding is stored in shared memory.** A binding
is a catalog entry whose value is a `binding` tagged value
(`configstorelayout.md` §4). Mango's `config.key_bindings[]` and its four
sibling arrays cease to exist as storage; they become catalog entries, and the
keymap handler takes its candidates from the block. The reasoning is the same one
that settled `action_ref` (`helpers.md` §6.2): a binding a user cannot read in
the file is a binding the user cannot rebind, and a binding stored only in
process memory cannot be edited by the socket client at all.

What Mango's `KeyBinding` carries, and where each field goes:

| Mango field | becomes |
|---|---|
| `mod` | a field in the `binding` header, normalized the way `helpers.md` §7 will normalize it |
| `keysymcode.keysym`, `.keycode1..3`, `.type` | fields in the header, all four of them, because Mango matches either a keysym or up to three keycodes and the type says which |
| `func` | `action_ref`, a frame offset holding the action's name |
| `arg` | `args_ref` and the inline positional array, already in the header |
| `mode`, `iscommonmode`, `isdefaultmode` | fields in the header, subject to §14.4's namespace mapping, which `input.md` §7.1 records as still open |
| `islockapply`, `isreleaseapply`, `ispassapply`, `isallowconflict` | flag bits in the header's flags word |
| `spec`, `line_number`, `file_index` | **dropped, all three, for one reason.** They describe where a binding was *written* rather than what it does, and none of them is a fact a reader of the block can use. `line_number` and `file_index` are plainly TOML parse diagnostics. `spec` is subtler and deserves the reasoning, because it looks load-bearing in Mango and is not load-bearing here: it is a shim over Mango's original storage, where a binding lived in process memory and could not be fetched over the IPC, so the only handle a human had on a binding was the config line the user had written. `spec` was how Mango echoed something recognizable back. In a block the binding *is* the record, it has a key, and `ipc.md` §4's `get` returns it verbatim, so the handle already exists and does not need a stored copy of the source text. Storing one would also be a second copy of the truth that only some bindings would have: a binding written by `set` over the socket never had a config line, and a value reconstructed from a record by running a parser backwards is lossy in a way the record itself is not. A redesign that allows scriptable configuration is exactly the case where a binding's origin text is the *least* interesting thing about it. |

The header therefore grows past its current 24 bytes, which answers the question
this supersedes: the `binding` tag is one tag with a kind field, not five tags,
because Mango's five structs share their header and differ only in the trigger
fields, and the trigger fields are the part that has to be inline and fixed-width
for §14.3's lookup to work. The exact offsets are still open and stay deferred
until the port is in flight; what is settled is which fields exist.

### 14.2 Storing everything in the block does not mean reading the block per keypress

**The block is the source of truth for binding data. It is not the lookup
structure.** These are different things and conflating them is the one way this
decision could have gone wrong, so the split is explicit:

- A **frame offset is not stable.** The arena grows, the journal is append-only,
  and `configstorage.md` §2 gives a key reference as
  `(epoch, entry_id, entry_generation)` for exactly this reason. An index built
  out of frame offsets would dangle on the next commit that grows the arena.
- **`helpers.md` §1 already puts all indexes in process-private memory.** The
  registry, the action table and the client set are all private; the block is
  shared data. The binding index belongs in the first group.
- The **precedent exists.** `ipc.md` registers a client's watches as a
  process-private index over the shared block, because a watch is a query about
  the block and a lookup table over the block is not itself configuration.

So the shape is: the block holds the records, and `core/input` holds a hash index
over them, and the index is derived state that can be thrown away and rebuilt from
the block at any time. Nothing is authoritative in the index.

### 14.3 Why the index has to exist, measured on Mango

Mango's handler is a linear scan of the whole array on every key event, and the
struct it strides over is large. Measured from the field lists at `d5a0e1e` on
x86-64:

```
Arg              56 bytes
KeySymCode       20
KeyBinding      144    2.25 cache lines
MouseBinding    120
GestureBinding  128
SwitchBinding   120
```

Two things follow. A scan of 200 bindings touches 450 cache lines, and 39% of
each `KeyBinding` is the 56-byte `Arg` payload that **the scan never reads**, so
the stride is 2.25 lines to compare one 4-byte modmask. And the tests are
ordered worst-first: `iscommonmode || isdefaultmode || strcmp(mode) == 0` runs
before the modmask compare, which runs before the keysym compare. The most
selective cheap test is last, behind a string comparison, for every one of the
five binding types.

Neither is a reason to abandon the port, and neither is fixed by porting
faithfully, because both are the scan's cost rather than the port's.

### 14.4 The index

A hash table in `core/input`, keyed on the fields that decide a match:

```
key   = (normalized modmask, trigger, mode_id)
value = candidate list of (entry_id, entry_generation), in config order,
        each carrying a cached resolved action handle
```

- **The modmask is in the key, not a filter.** Mango's test is exact equality
  after `CLEANMASK`, so `super+a` and `super+shift+a` are different matches and
  belong in different buckets. This is exact, not an approximation, and it is
  what keeps the candidate lists short.
- **The trigger is in the key** for the same reason: a keysym match and a
  keycode match are both one hash lookup rather than a walk.
- **Ordering inside a bucket is config order and is load-bearing.** Mango's
  `isallowconflict` means the scan continues past a match, so the winner is the
  first match in document order. An index that reorders by hash or by
  registration time changes which binding fires.
- **Common-mode bindings get a short second walk.** Mango's
  `iscommonmode || isdefaultmode || strcmp(mode) == 0` means a common binding
  matches in every mode. Making it a wildcard in the hash key would mean either a
  second hash lookup per event or a wildcard encoding in the key, and both are
  worse than walking a list that is small by construction, since a config with
  many mode-independent bindings is not a config anyone has written.
- **The action handle is cached in the index.** This is the one place §14.1 and
  the `action_ref` decision in `helpers.md` §6.2 interact. Because the action is
  a *name* in the block, resolving it on every keypress would be a string hash
  into the arena on the hot path. The index resolves it once, at build time, and
  a name that does not resolve is `ACTION_NOT_FOUND` recorded in the index rather
  than a per-keypress failure.

**Maintenance is incremental, from the journal.** The index is not rebuilt per
keypress and not rebuilt per commit either. `configstorage.md` §5's `KEY_SET`
journal entry already carries `(epoch, commit_id, journal_seq, time_ns,
entry_id, entry_generation, type, value)`, which is everything an index update
needs: one commit's worth of changed keys, with the generation that says whether
a slot was reused. A commit that touches no binding costs a journal scan that
finds nothing.

This makes the cost of a keypress one hash lookup plus a walk of a list that is
usually one element, against Mango's 200-element scan. It also means an external
client editing a binding through the socket takes effect on the next keypress
without the compositor being told, because the journal is the same channel every
other writer already uses.

All input stays config-driven, and every binding remains an individual config
key, so a user can rebind, disable, or script any single binding. A gesture or
binding invokes an action from the same action registry the socket and rules
use, so there is no separate "input command" language. Focus policy is config,
including focus-follows-mouse alongside click-to-focus. wlroots still supplies
the device plumbing; what changes is that the matching above it is Mango's.

## 15. Monitors and outputs

Outputs support hotplug, per-output configuration, virtual monitors, and
Xwayland.

- **Hotplug** means an output appearing or disappearing reconfigures the layout
  without a restart, moving the tags that were on it.
- **Per-output configuration** lets a rule match an output by name, make, model,
  or serial and set scale, position, refresh, and layout defaults for it.
- **Virtual monitors** are headless outputs the compositor creates, so a user can
  have a workspace with no physical screen.
- **Xwayland** runs X11 clients inside the same tags, layouts, and decorations
  as Wayland clients, so X11 applications are not second-class.

## 16. Protocols and external rendering

The protocol surface follows MangoWM's: we use Mango's code where we can, and
change what we need. We do not curate the set, so that an existing Mango-compatible
client or config works unchanged; the things we change are the ones this document
lists below, and a protocol is on that list only when a Mango-compatible client
would otherwise stop working. The compositor instantiates the wlroots protocol
managers it needs and adds the
Mango set on top.

External rendering is supported in two stages:

- **Regions first.** An external program requests a framebuffer region from the
  store's existing region mechanism, paints into it, and stores a region
  reference in a decoration or animation slot. The compositor uploads it as a
  scene node. This reuses the mechanism `configstorage.md` already froze and
  needs no new protocol.
- **Then a protocol.** A Wayland buffer-injection protocol lets an external
  renderer hand the compositor a real buffer so it can use the GPU path, which
  a shared-memory region cannot.

The motivating case is compositing an externally rendered scene into the output,
for example a terminal rendered as a CRT on an object flying past. omniWM's
responsibility is to offer the compositing capability; what to render is the
external program's business.

## 17. Any Language, concretely

The endgame of Any Language is that a first-class library in another language
requires no change to the compositor. That is only true because the interface is
the block.

**None of the implementations that demonstrate this is a deliverable of the window
manager, and all of them come after the project is otherwise complete.** This is
the one place where the obvious reading of "Any Language" is wrong, so it is worth
being exact about why. The claim being demonstrated is that the surface is generic
and language-neutral, and **a language-specific component inside the compositor
would falsify the claim it is meant to support**: if the WM carried a Lua
component in order to serve a Lua library, then the surface is not generic, it is
Lua with a C wrapper, and the client-library story only holds for languages someone
remembered to add. So the compositor stays generic and knows nothing about any
language, and the demonstration programs are consumers of that surface rather than
parts of it:

- a C reference client library that maps the block using `include/shared/`
  alone;
- a Python reference scripting library;
- an interpreter for Mango configs;
- an interpreter for Hyprland configs.

**These are separate programs with no cross-dependency in either direction.** They
share exactly one thing with the compositor: they expect the SHM block to exist,
and that is a runtime expectation rather than a build or design dependency. Nothing
in the compositor knows they exist, nothing in them is required for it to run, and
no part of the block's shape is decided by what they need — a library in a language
nobody has written is then just another client of the same block, which is the
entire content of the claim.

Their purpose is to **prove the claims and inspire users**, which is a different
kind of work from building the thing and is worth naming as such. They are the
"check out what my WM can do" pieces, and that is precisely why they are last on the
roadmap in §18: an interpreter written against a finished design is a test of the
design, while one written against a design still moving is just a second thing that
has to change when the design does.

## 18. Roadmap

The stages are the `README.md` roadmap. The mapping is what shows the shape of
the build: the compositor core first, then layout, then looks, and only then the
languages and interpreters.

**The last four stages are not stages of the window manager.** They are the example
implementations of §17, they come after the project is otherwise complete, and they
are separate programs rather than parts of the compositor. They are last because
they are the proof rather than the thing, and a proof written against a design
still moving only tests the half that has been written. Nothing in stages 1 to 14
depends on them, and they depend on nothing in stages 1 to 14 except that the SHM
block exists.

| stage | work | subsystem documents |
|---|---|---|
| 1 | config store and ipc functional, simple write+read test | `configstorage.md`, `ipc.md`, `testing.md` |
| 2 | TOML parser for config store + helper for registering keys | `tomlparser.md` |
| 3 | shared helpers, core libraries, logging, server backend | `helpers.md`, `build.md`, `licence.md` |
| 4 | basic input | `input.md` |
| 5 | WM starts and shows windows | `windows.md`, `monitor.md` |
| 6 | layout constraints with a single demo layout | `layoutengine.md`, `layoutlanguage.md` |
| 7 | tags | `tags.md` |
| 8 | basic decorations | `decorate.md` |
| 9 | animations | `animate.md` |
| 10 | advanced input | `input.md` |
| 11 | advanced decorations | `decorate.md`, `animate.md` |
| 12 | fake client drawing | `draw.md` |
| 13 | background + infinite canvas background rendering | `draw.md` |
| 14 | vector line objects | `draw.md` |
| 15 | example scripting lib, python | `languages.md` |
| 16 | example interpreter, mango | `languages.md`, `protocols.md` |
| 17 | example scripting lib, lua | `languages.md` |
| 18 | example interpreter, hyprland | `languages.md`, `protocols.md` |

## 19. Open items

None of these is a document-reconciliation question, which is why phase 00 closed
it without answering any of them: each is either owned by a named phase or is an
external fact this design cannot decide. They are listed with their owner rather
than as open work because an item with no owner is what turns into a forgotten
question three phases later.

- **The scenefx extension set, and patch versus fork.** Unresolved, and the largest
  external risk in the project. Owned by **phase 02 items B2 and B3**, which is
  the only phase that can answer it, because the answer is a version and a
  packaging decision rather than a design one. It gates `draw.md` and `animate.md`,
  both of which inherit whatever it settles.
- **Which wlroots release the build tracks.** Unresolved; scenefx 0.5 requires
  wlroots 0.20, so the two answers are not independent and must be taken together.
  Owned by **phase 02 item B2**, same reason.
- **Whether a Lua library is a deliverable. Closed: no, and the same answer covers
  every interpreter and library in the project.** They are example implementations
  made *after* the project is otherwise complete, to prove the claims and inspire
  users, and they are their own separate deliverables rather than part of the
  window manager. §17 states why, and the reason is the substantive one: the
  compositor stays generic, and a language-specific component inside it in order to
  serve a language-specific library would be counter to the entire point, since it
  would make the surface Lua-with-a-wrapper rather than language-neutral. They are
  separate programs with no cross-dependency in either direction; the only thing
  they share with the compositor is that both expect the SHM to exist. That is also
  why they are last on the roadmap — they are the "check out what my WM can do"
  pieces, and an interpreter written against a finished design is a test of the
  design rather than a second thing that has to move when the design does. Nothing
  in the phase plan may assume a scripting surface exists inside the compositor.
- **The tag-move operation and how a tag's state migrates.** Closed by phase 00
  and owned by `tags.md` §7, which defines it as a content swap in one grouped
  commit rather than a move, with the store obligations in `configstorage.md`
  §14.1. This document fixes only that a tag is an entity and a monitor displays an
  ordered list of its own tags.
- **The background and infinite-canvas options.** Explicitly deferred to stage 13
  and `draw.md`, which is an owner.
