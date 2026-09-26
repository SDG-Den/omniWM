# Layout engine (devnotes/layoutengine.md)

The authority for the constraint solver: what a constraint is, what a layout
program is, what the solver does with them, how a solved layout becomes an
arranged one, and what the required layout set needs from all of it. It owns the
mechanism behind Any Layout (`generaldesign.md` §1, §2) and the roadmap stage 6
deliverable (`README.md` stage 6, mapped in `generaldesign.md` §18).

**This is not yet a design.** It is the boundary between what is already
decided and what this document still has to decide, written down so the design
starts from the constraints instead of rediscovering them. §2 is the inheritance
from the existing documents and is not open for reinterpretation here. §3 is
the open questions. §4 is the layout namespace, §5 the update pipeline, §6
what the pipeline demands of the language, §7 the required layouts and the
primitive each one needs, §8 what that set says about the engine's shape, and
§9 the references. §10 records what this document blocks and what blocks it,
and §11 is the carry-forward list.

Nothing here restates a contract owned elsewhere. The store and its byte layout
are `configstorage.md` and `configstorelayout.md`, with
`include/shared/omni_layout.h` as the only numeric home; the component
substrate is `helpers.md`; the lifecycle is `server.md`; the socket is `ipc.md`;
the config file binding is `tomlparser.md`; the subsystem boundaries are
`generaldesign.md` and `filestructure.md`.

## 1. Scope, and the index

`missing-devnotes-topics.md` assigns this document seven questions. They are the
spine of §3, and the status column below is the honest starting position for
each, after the update pipeline in §6 and the layout set in §8 were settled.

| # | question this document must answer | status |
|---|---|---|
| 3.1 | the constraint record on a client | intent fixed, representation open |
| 3.2 | the soft-with-priority model and how the solver degrades | intent fixed, mechanism open |
| 3.3 | the constraint language a user writes | settled, and written out in `layoutlanguage.md` |
| 3.4 | built-in layouts as constraint presets | mechanism exists, unstated |
| 3.5 | how a layout is selected per tag | open, and blocked on an identity the block lacks |
| 3.6 | nested layouts for groups, clusters and scratchpads | intent fixed, and the required set confirms all three cases |
| 3.7 | the arrange pass | the pipeline is fixed by §6, solved storage by §2.10; triggers, arranged storage and re-entrancy open |

`layoutlanguage.md` owns the syntax: the model, the rule kinds, the keys at each
level, the guards and the frozen limits. It is normative, and where this document
disagrees with it the disagreement is a bug in this document.

The documents that also touch layout are not written yet. `windows.md` owns the
client, the floating layer, focus and stacking, groups and clusters.
`tags.md` owns the tag entity, the per-tag layout, and tag membership. `animate.md` owns
animation and the interaction with solver geometry. This document owns the
solver and the geometry it produces, and §6 states each seam.

`devnotes/layoutsystem.md` and `devnotes/looks.md` are empty by decision and
stay empty (`missing-devnotes-topics.md`, preamble). The layout promise is split
across this document, `layoutlanguage.md`, `tags.md`, and `windows.md`; this
document is the solver part of that split and `layoutlanguage.md` is the syntax
part.

## 2. What is already decided

### 2.1 Layout is a solver, not a set of layouts

This is the project's central structural claim. It is stated in
`generaldesign.md` §1, §2 and §7, and it is inherited here rather than chosen:

- Tiling, scrolling, floating and canvas layouts are not hardcoded algorithms.
  They are constraint programs the user writes (`generaldesign.md` §1).
- If layout is a constraint program, then a layout nobody anticipated is a
  config file rather than a patch, which is what makes Any Layout true rather
  than aspirational (`generaldesign.md` §2).
- Layout is a constraint solver, not a set of layout functions, and this is the
  mechanism behind Any Layout (`generaldesign.md` §7).

Two consequences are already binding:

- Nothing in the arrange path may be hardcoded to a particular layout
  (`generaldesign.md` §7).
- Adding a built-in layout means writing the layout, registering it, and
  recompiling, and it means doing that without adding a solver mechanism
  (`generaldesign.md` §7). The two halves matter separately. Recompiling is
  accepted; built-ins are compiled into the compositor. The mechanism constraint
  is the part that disciplines the engine, because it is the only part that can
  fail quietly.

That is the pressure this document is under, restated so that it survives the
built-in decision. If the language is wrong, every shipped layout is wrong with
it, and there is no second code path to fall back on. It is the reason the
language question is treated here as the hardest one rather than the most
creative one. What the decision removed is the stronger and now false claim that
a built-in is a config change: it is not, and §7.1 had already made that plain by
equating a stack with a group.

### 2.2 Constraints are soft, prioritised, and seeded

`generaldesign.md` §7 fixes the model:

- A layout is a generic program of soft constraints. The named examples are
  fills-parent, left-of, half-width, and gap-around.
- The program names no window. It constrains whatever set it is handed, in walk
  order, so it is the same program for every client it arranges.
- Each window carries its own state and overrides, and a window that deviates
  from the layout does so through its own values. A user can opt a window in or
  out, and window rules can adjust what a window overrides.
- On each arrange pass the solver takes the program's constraints and the
  windows' overrides, and produces a geometry for each.

Three routes reach a window's overridden state, and they are one mechanism rather
than three: the user's opt in or out, a window rule, and the window itself. None
of them is a seed, because there is no per-window seed to write: the generic
program is the layout, and it is fetched from SHM by name and applied to whatever
the walk reaches. That is the same convergence property `generaldesign.md` §11
states for per-window decoration overrides, and it is already the project's answer
to how any per-window value is reached without a privileged path.

This is MangoWM's division, kept deliberately. A Mango layout is a compiled
algorithm over the client list and each client carries its own size and master
status; the layout never names a client. What differs here is only the mechanism
underneath, a soft-constraint program solved by weighted least squares rather than
a direct assignment of rectangles, and §7.5's relaxation on top. The model is not
redesigned, so Mango is the reference for it wherever the two could disagree.

### 2.3 The solver degrades, it does not fail

`generaldesign.md` §7: when the constraint set cannot be satisfied, the solver
does not discard anything. Every constraint carries a weight, and the solver
returns the geometry that minimises total weighted violation, so a subtly wrong
user layout produces a slightly odd arrangement rather than an error. The intent
is fixed and the choice of failure mode is deliberate: the feature is aimed at
experimentation.

The mechanism was inherited here as a requirement rather than a specification,
because the original wording was "drops the lowest-priority constraint it cannot
honour" and both halves of that were undefined. §3.2 settles it as weighting,
and `omni_layout.h` had already encoded weighting before this document did:
`OMNI_CONSTRAINT_OFF_PRIORITY` is documented there as the solver's linear
weight, and the four bands are separated by an order of magnitude each
specifically so that a band dominates the compromise rather than trades evenly
against it. The prose was the stale half, and it has now been amended to match
the header rather than the other way round.

### 2.4 The engine is a general service, and tags are its first consumer

`generaldesign.md` §7: a tag is the unit of "where windows are" and owns its own
layout, its own client set, and the other data needed to view it. It is not per
tag per monitor. A monitor displays an ordered list of tags and the first still-set tag is
the primary, which supplies the layout; a secondary tag contributes clients and
nothing else. A layout is not exclusive to tags. A group is a nested layout, a
scratchpad and a pop-up tag each lay out their contents, and a cluster lays out
its members. The engine is a general service and tags are its first consumer
rather than its owner.

This paragraph previously inherited "per tag, per monitor, plus a floating
layer" and counted three scopes: a tag's tiled set, a nested container, and a
floating layer. `tags.md` §4 and §6 change the count twice over, and neither
change is cosmetic. A monitor holds no layout state, so there is no per-monitor
scope to count. And the floating layer is not a scope at all: whether a client
floats is state the client carries (`tags.md` §6), and a floating client is exempt
from the layout rather than arranged by a second one. What is left is **one kind
of scope, a client set to arrange, plus the groups that nest it.** A tag's set
and a group's set differ in who owns them, not in what the solver is asked to do.

The solver's input is now stated rather than assumed. A solve takes the primary
tag's layout, the union of the client sets of every tag in the monitor's list, and
each of those clients' own floating state. That is a different shape from "arrange the clients in this
tag", and it is the shape `tags.md` §4 defines.

`generaldesign.md` §8 adds that a group is a nested layout whose members obey
the layout engine inside it, and that a cluster is a movement relationship with no
chrome of its own. A cluster **occupies one space of its parent** rather than one
space per member, which is what makes a group bar expressible at all, and
`layoutlanguage.md` §3.8 is where that is worked out. It has one consequence this
paragraph used to assert the opposite of: a cluster whose members straddle a primary
and a secondary tag is no longer obviously coherent, because the cluster is placed as
one occupant and a space belongs to one layout. `tags.md` §4 records that as open.

`generaldesign.md` §5 also says every user-facing capability is a component,
which is in tension with the two paragraphs above, and the tension is not
cosmetic. §3.7 needs the answer, because the answer decides whether the engine
appears in the `helpers.md` registration table with a priority band and an
`enable_key` at all, or is wired directly into `core/server.c` the way the store
is. §3.7 records it as open.

### 2.5 The engine's inputs must be expressible in the block

This is the constraint the store work has already placed on this document, and
it is why the design cannot start from the constraint language and work
backwards.

`configstorage.md` §0 makes the block authoritative and the storage layer an
open catalog with self-describing values and no key-specific code, and
`generaldesign.md` §4 makes every user-facing feature data over that block. A
constraint program is user-facing data, so a constraint program is a value in
the block, and that value must be one of the core tags in
`include/shared/omni_layout.h` §8.

What the current tag set offers a constraint language:

- geometry primitives: `point`, `size`, `rect`, `offset`, `vec2`, `vec2i`,
  `vec2u`, `ratio`, `percent`, and the scalar widths;
- `enum` for naming a layout or a constraint kind;
- `tuple` with an explicit `field_types` sequence and `array` with an explicit
  `elem_type`, both self-describing and length-checked (`ipc.md` §3.2);
- `option`, so "no constraint" is representable without a sentinel;
- `rule`, whose schema is a match-and-replace over keys (`helpers.md` §6.2);
- `array` of `tuple`, so a variable-length list of constraint records is
  representable.

What it does not offer: there is no tag for a constraint, a layout, or an
expression. `rule` is not one; it is a key match-and-replace record
(`helpers.md` §6.2). The three-way tag decision in `configstorage.md` §12.1 then
fixes what follows from that gap:

- `0x34..0x7FFF` is unassigned, and an unassigned low tag is malformed and
  refused by the core, so a new core type cannot be quietly introduced. §2.9
  reopens the freeze that made this expensive rather than forbidden, and §2.10
  is the resulting list;
- `0x8000..0xFFFF` is the extension range, carried framed and never
  semantically interpreted by the core, so a constraint language defined there
  is invisible to the store and to every core consumer;
- a new core tag above `0x33` moves the known-tag range, which is part of the
  block format. `OMNI_FORMAT_VERSION` is 1 and stays 1, because the compositor
  does not exist yet and the first runnable build is v1, so a tag added now is
  not a change to a released format but a change to the only format there has
  ever been. The header-tier check is still worth keeping for the case where a
  stale reader does turn up: without it, a reader would refuse an unknown tag at
  the value tier instead of refusing the block at the header tier, which is the
  coarser and better failure.

The representation of a constraint program is therefore a format decision and
not only a language decision. §3.1 and §3.3 cannot be answered without answering
it, and answering it late is expensive in a way answering it early is not.

### 2.6 Storage facts that bound the design

- The catalog is 16384 slots (`omni_layout.h` §2), entries are created live
  through a freelist, and live entries never move, so a holder can keep a
  reference stable (`configstorage.md` §3).
- Arena space is never reclaimed except by in-place overwrite of an existing
  value (`configstorelayout.md` §7, Reclaim). A framed value that is created
  and then destroyed keeps its arena for the life of the block.
- Per-client geometry is one of the named examples of `WINDOW_DEPENDENT`
  (`configstorelayout.md` §8), and `WINDOW_DEPENDENT` is excluded from `save`
  and from a soft reset by one shared test (`configstorage.md` §13,
  `ipc.md` §reset). A constraint record stored as per-window geometry therefore
  does not survive a save and reload, and a constraint record stored as ordinary
  configuration is not per-window state for the purposes of a reset.
- A reset does not restore individual values. The WM clears and rebuilds and
  whatever applied the configuration re-applies it (`ipc.md` §reset), so layout
  state is re-derived by the applier rather than restored by the store.
- The pool grows by doubling to a 512 MiB cap and then reports
  `BLOCK_EXHAUSTED` rather than truncating (`configstorelayout.md` §9).

The first two of these interact: a per-tag or per-window framed value that is
created and destroyed on a tag change costs arena that only a block recreation
or a growth commit recovers, and growth is bounded.

### 2.7 The shape the engine must take as a component

`helpers.md` §3 requires every user-facing capability to reach the three
universals, and `generaldesign.md` §5 requires every user-facing capability to
be a component. So the engine needs:

- **options**, the values it reads. The active layout, the constraint program,
  gaps, ratios and any per-layout option are all config keys, and
  `helpers.md` §3.1's `omni_option` is the existing shape: a full key, a type
  tag from the core set, and a native default.
- **actions**, what a binding, a rule, or a client invokes. The one existing
  example of a layout action in the whole repository is
  `ipc.md` §exec, `{ "action": "wm.cycle_layout", "args": ["master-stack"] }`,
  which passes a layout **name** as a string argument rather than a program.
  That is a datum about the socket contract, and §3.4 has to live with
  it.
- **triggers**, what it subscribes to. It reacts to key changes through the
  journal like every other component (`helpers.md` §7), with the same
  `gap_policy` argument and the same replay rules as an IPC watch.

`helpers.md` §3.2 fixes that exactly one instance exists per activation and that
a toggle never re-runs `init`; `helpers.md` §6 fixes that a suspended component
releases its subscriptions and actions and a resumed one re-registers them.
`helpers.md` §11 puts tags and windows in band 200 and decorate and draw in 300,
and states that priority is advisory and not a dependency edge, so a component
that activates before something it implicitly needs surfaces as an init failure
rather than as a detected violation. Where the engine sits in that order is
open, and it is a real question rather than a detail: see §3.7.

`server.md` §7 and `helpers.md` §6.1 fix the execution model. Dispatch is
single-threaded on the event loop, there is no quiescence protocol, a callback
that commits re-enters dispatch one level deep, and arguments are resolved
before the callback is entered. The arrange pass therefore runs on the event
loop like everything else. `helpers.md` §6.1 does permit a component to start a
thread provided it joins it inside `destroy` or `suspend`, so a solver that
wants one has to justify it against that rule.

`generaldesign.md` §9 and §13 make the engine a participant in the scene graph
and in fake clients: a fake client takes part in layout and in focus exactly as
a real window does, and everything visible is a node in one scene graph. The
solver's output is consumed by the scene, not by a client-specific path.

### 2.8 What the engine must not be

Already rejected, and recorded here so a later design does not re-open them:

- Not a table of layout functions. `architecture-audit.md` §7 records that
  Mango's model is a fixed table of function pointers with an `arrange`
  operation, and that is the model being replaced.
- Not a per-tag index into a fixed array. `generaldesign.md` §6 rejects
  Mango's bit-position tags and per-monitor indexed state explicitly.
- Not a privileged path. `generaldesign.md` §4 and §5, and `helpers.md` §10: the
  compositor's own defaults and a user's configuration reach the engine the
  same way, and there is no privileged internal route.
- Not a plugin boundary. `helpers.md` §10 rejects a native plugin ABI as a
  decision rather than a deferral, so a layout cannot be extended by loading
  code. A layout is data.

### 2.9 The freeze on the block ABI is lifted

`architecture-audit.md` §8 closed the store's layout contradictions by freezing
the byte layout, and `configstorelayout.md` calls its tables the v1 ABI.
`configstorelayout.md` §14 records that the header is the only home for every
constant in §2. As of 2026-09-26 the project owner has reopened that freeze,
because the compositor subsystems are still at design stage and a change now
costs a paragraph while the same change after the first runnable build costs a
format version plus a compatibility story.

This does not reopen the concepts. `configstorage.md` §0's principles, the
open-catalog rule, guards-at-consumption, the commit protocol, and the identity
vocabulary are not what was frozen and are not in question. What is reopened is
the concrete surface: the type tag table, the payload shape of the existing
composite tags, the fixed section set, and the v1 capacity constants. Every
number still has exactly one home, `include/shared/omni_layout.h`, and a change
to any of them is still a one-line edit there plus the matching table in
`configstorelayout.md`.

§2.10 is the list of what the layout engine needs from that surface, with the
cost of each. Nothing in it has been applied.

### 2.10 What the layout engine needs from the block

Six requirements, checked against the current tables rather than assumed. Three
are satisfiable with what exists, three are not, and the three that are not are
the ones with a cost.

Satisfiable today, recorded so they are not re-litigated:

- **A stable node identity for anything the solver places.** §7 requires the
  solve output to be diffable per node so the animator can move one client
  without touching the rest. `entry_ref` at `0x23` is already
  `(epoch, entry_id, entry_generation)` with a framed payload
  (`configstorage.md` §4), which is exactly the identity `configstorage.md`
  §0.1 already defines for a catalog slot, and the solved layout section's node
  record carries the same `(entry_id, entry_generation)` pair with the epoch
  implied. A solved layout is an array of those pairs with a `rect`, and the
  identity is the reason a node can be diffed across two solves.
- **A pan and zoom viewport.** §7.2 and §7.5 need an offset and a scale.
  `offset` at `0x18` and `ratio` at `0x10` are both present, so a viewport is a
  two-field tuple.
- **A spawn mode, a bounds pair, and a grid-alignment flag.** §7.2, §7.3 and
  §7.4 need an enum, two booleans and a boolean. `enum` at `0x24`, `bool` at
  `0x01`, and a tuple of those cover all three with no format change.

Not satisfiable before the freeze was lifted, all three now resolved. The
requirements stand; what changed is that each has an answer rather than a cost
estimate:

1. **There was no type tag for a constraint or a layout program.** The tag table
   ran `0x01` to `0x31` and `0x32` to `0x7FFF` was the unassigned hole that
   `configstorage.md` §12.1 classifies as malformed. Taken: `OMNI_TAG_CONSTRAINT`
   at `0x32` and `OMNI_TAG_CLIENT_RULE` at `0x33`, so the table now runs `0x01`
   to `0x33` with `0x34..0x7FFF` as the hole. The gap was real rather than
   hypothetical, and what it cost was:

   *Cost of minting a core tag:* one constant in `omni_layout.h` §8, a row in
   `configstorage.md` §4, a row in `ipc.md` §3, and a static assert that
   `OMNI_TAG_MAX_KNOWN` moved. That is the whole cost. `OMNI_FORMAT_VERSION`
   stays 1, because the compositor does not exist yet and the first runnable
   build is v1, so a tag added now is not a change to a released format but a
   change to the only format there has ever been.

2. **A `binding` could not carry an action argument.** `0x2B` was
   `{modmask, keysym/keycode, action_ref}` and the wire form in `helpers.md`
   §6.2 was `{mods, key, action}` with no argument field, while actions
   themselves take positional arguments per `helpers.md` §6.1. The required set
   hits this immediately: §7.6 needs snap in each direction from a keybind, §3.4
   needs `cycle_layout` to take a layout name, and the existing `ipc.md` §exec
   example already shows `wm.cycle_layout` taking `"master-stack"`. A
   parameterized binding could only be expressed by minting one action per
   parameter value, which is a combinatorial mess that grows with every
   direction and every layout.

   *Taken:* a 24-byte header followed inline by an argument array, with
   `args_ref` naming the array's frame so a reader need not walk the payload
   (`configstorage.md` §4). The alternative was an action per parameter, and the
   requirement set had already made that untenable.

3. **A `rule` matches keys and replaces values, and none of the required
   behaviour is that.** `0x2C` is a generic match-replace record, and
   `helpers.md` §6.2 pins the schema to `{match: {key, type}, replace: {type,
   value}}`, so a rule rewrites one catalog key with another value. But §7.1
   needs rules that move a client into and out of a group as it spawns and
   leaves, §7.3's `replace` spawn mode needs a rule that relocates a client, and
   §7.5 needs cluster-on-attach. Every one of those matches against a *client*
   and acts on *membership*, so none of them is expressible as a key match and a
   value replacement, and none of them is a window rule in the sense
   `generaldesign.md` §8 already uses the word.

   *Taken:* `OMNI_TAG_CLIENT_RULE` at `0x33`, a 24-byte header plus the value its
   effect applies. The alternative was a version field on `rule` so one tag
   carries two schemas, which is worse because a core reader then has to
   dispatch on a field it previously assumed. The fact that this and item 1 both
   wanted a new core tag was the argument for taking them together.

One more that is not a gap but a cost worth stating now:

- **`OMNI_CATALOG_SLOT_COUNT` is 16384.** §3.1 wants a constraint record per
  client and §3.5 wants layout state per tag, on top of the per-window
  decoration overrides that `generaldesign.md` §11 already requires, and every
  one of those is a catalog entry. 16384 is generous for a v1 and this is not a
  reason to change it, but `configstorelayout.md` §14 is right that the value
  is load-bearing: it sets a fixed section size and every derived offset after
  it. If it is ever going to move, this is the moment, and the same edit is the
  moment to decide whether a solved layout belongs in the catalog at all.

That last paragraph was really §3.7's question, and it now has an answer.
`configstorage.md` §13 already has `EPHEMERAL` for values with no long-term
relevance, and a solved layout is exactly that: it is re-derived from the
constraints on every solve, so persisting it buys nothing. Two homes were on the
table. An `EPHEMERAL` catalog entry needs no new section, but it costs a commit
per solve, which is the cost §3.7 already names, paid on the most frequent event
in the compositor. A new fixed section written without journalling costs a
constant and a row: `OMNI_SECTION_SLOT_COUNT` is 16 and only ids 0 to 6 were
assigned, so the table had room.

*Taken:* the fixed section. `OMNI_SECTION_SOLVED_LAYOUT` is id 7 at `0x1D2800`,
`0x8000` bytes, a 16-byte header and up to 1024 24-byte nodes, and its section
row leaves `JOURNALLED` clear, so a solve is a bounded memcpy and a `generation`
bump rather than a commit. This is what makes the answer to §3.7's "how does an
external program read a layout" cheap rather than expensive, and it is why
`OMNI_CAP_DEFAULT` moved from `0x7F` to `0xFF`: the new section's presence is
what tells a reader the block is one it can read a solve from.

## 3. What is not decided

### 3.1 Spaces are the nodes, so a layout names no window

Resolved: what a constraint constrains, and therefore what a constraint record is.
The answer is a space, and it is the reason a layout can be a program shared by
every client it arranges.

- **A space is a placeholder that a client maps to.** It is logically equivalent
  to a client in the constraint system: it can be placed, constrained and nested.
  It is not a client, it has no `entry_ref`, no epoch and no lifetime, and the
  program never learns which client ended up in it. A program is therefore
  decoupled from the tag's current membership, which is what lets membership
  change without the program and the set ever disagreeing.
- **A client enters by publishing an intent, not by being named.** Mapping is a
  phase between the update and the solve, and it has three parts rather than one.
  Sources publish **intents**, a claim of the form "this client belongs in this
  space"; a conflict is a space with more intents than it can hold; and a
  **reflow resolution rule** decides what happens to the surplus. A push is the
  common case of a resolution rule and not a mechanism of its own. The sources are
  the program's own rules, §7.3's spawn modes, the user's dispatcher action or
  drag, and the client's own overrides, and none of them needs to know the others
  exist. The solver then arranges spaces and never learns that clients exist.
- **Spaces nest.** A space may hold a group, and a group has its own layout name,
  so a nested layout is one mechanism rather than a per-layout feature. The parent
  addresses the group as a single node. §3.6 owns the consequences.
- **One resolution rule covers every way a conflict arises.** In master and stack,
  `master` holds one client and `stack` is a group laid out by the vertical stack
  layout. A second intent for `master` is a conflict, and the resolution rule
  reflows the incumbent into `stack`. A dispatcher action, a drag out of the
  stack, and spawn-on-push are not three features: they are the same event, a
  second intent for a space that holds one. This is the reason the mechanism is
  worth having, and it is the shape Mango gets from dwindle's master handling by
  construction rather than by declaration.
- **Resolution can cascade, so it has to terminate.** Pushing `master` into `stack`
  is only the depth-one case. If `stack` were itself bounded, resolving the first
  conflict creates a second, and the requirement is that resolution reaches a
  fixed point. The cheap sufficient condition is that a resolution rule's target
  is either unbounded or strictly lower in a declared partial order, which makes
  cascades finite and checkable rather than emergent. §3.3 owns the surface syntax
  for declaring that order.
- **Overrides reference clients, and that is the asymmetry.** A client override is
  the one place a client is named, because a deviation is by definition about a
  particular window. Clustering is the example: it is a client-side override
  expressing a relationship between specific clients, and the layout is untouched
  by it. This is the division Mango already has, where each client carries its own
  state and the layout carries none.

**What a resolution is not.** Reflow resolution moves a client from one space to
another, which no positional residual can express, so it is a mapping operation
and not a constraint. It is deliberately outside the solver, because the solver's
defining property is that a solve is a pure function of committed state, and a
resolution mutates which client is where. `generaldesign.md` §7's built-in policy requires that
adding a layout needs no new solver mechanism, and mapping is how that policy is
kept: the mechanism is the mapping phase, built once, and a layout contributes
intents and resolution rules to it rather than code to the engine. That is also
the honest test of whether a proposed feature belongs here. If it can be phrased
as an intent or as a resolution rule, it is a program-level declaration and it
costs no engine change. If it needs the solver to know something new, it belongs
in §7 first.

**The program format.** A program is a space table, a set of mapping rules, and an
array of constraint records, and it is stored in SHM in exactly one shape however
the user authored it. A space needs a name, a capacity, and an optional nested
layout name. A resolution rule needs a source space, a target space and a
declared position in the partial order that makes cascades terminate. A record names a subject space and an operand space,
and no client. `OMNI_TAG_CONSTRAINT` at `0x32` is therefore a program rather than
a bare record array, and the record itself is unchanged in size at 16 bytes with
its two space references as `u16`.

Blocks §3.3, §3.4, §3.6, `windows.md` and `animate.md`.

### 3.2 The soft-with-priority model and how the solver degrades

Resolved: the degradation mechanism, and determinism, which its own last bullet
states as a requirement. Open: what a user writes.

**Weighted minimisation, and nothing is dropped.** §2.3's inherited wording asked
for a drop rule. The mechanism is weighted least squares over the constraint
system, which is what `omni_layout.h` already encodes, and it retires four of the
six questions below without answering them, because all four were artefacts of
thinking in terms of dropping:

- The iteration bound, the fixed drop order, and the defined terminal
  arrangement are not needed, because no constraint is ever removed from the
  system. There is no second pass to bound and no order to fix.
- Degradation cannot be sticky, because nothing is lost between passes. A window
  that was too small last pass is still subject to the same constraint this
  pass, and the layout cannot permanently lose it.
- The user is never told what was dropped, because nothing was. The only
  remaining question in that area is whether to report total residual violation
  as a diagnostic, which is a logging question rather than a protocol one and
  does not need the socket's closed error set.
- Unsatisfiability is not a detectable event, and that is the correct answer
  rather than a gap. There is no such thing as an unsatisfiable state, only a
  geometry with a cost. Two windows each demanding the full width produce two
  half-width windows, which is the "slightly odd arrangement" §2.3 wants, so the
  solver never has to detect anything and never has to choose between degrading
  and silently overlapping, because weighted minimisation degrades and overlaps
  are just what a high residual looks like.

DuckWM is the working reference for this and is the closest existing
implementation of the mechanism §2.3 asks for. It solves per strongly-connected
component rather than as one global system, warms each component from the
previous frame's solution, and treats the highest-weighted equation whose
variables are all already-solved as a fixed point, which is what keeps the warm
start cheap. The weight mapping is already in the header: positional constraints
are `OMNI_CONSTRAINT_PRIORITY_STRONG` and structural ones are
`OMNI_CONSTRAINT_PRIORITY_DOMINANT`.

**There is no hard constraint, and the constant is named for that.** The top
band was `OMNI_CONSTRAINT_PRIORITY_REQUIRED` and has been renamed to
`OMNI_CONSTRAINT_PRIORITY_DOMINANT`, because at 60000 against `STRONG`'s 6000 it
is one decade of weight and not a different kind of constraint. DuckWM does the
same thing with `REQUIRED` special-cased to an effective weight of `1e8`, and its
own note is that this is a weight large enough for the fit to honour it rather
than a separate code path, so a program with two of them that cannot both hold
still produces a compromise. This is consistent with `generaldesign.md` §7's
"constraints are soft and prioritised", and it should stay that way. The rename
is recorded here because the old name was the thing worth fixing: it would have
been read as a guarantee the solver does not make, and `DOMINANT` describes only
how much weight the band carries. The header comment now says so explicitly.

Still open:

- Whether priority is a strict order, whether equal priorities are allowed, and
  what a user writes to set it. `helpers.md` §11 has an unrelated priority
  concept for components, so the word is overloaded in this repository and the
  two must not collide in a config key or in a struct. The header's answer so far
  is that the bands are separated by a decade each so that one band dominates the
  compromise, which means the bands are ordinal and the values inside a band are
  not, and the config surface has to say which.
- Whether the solve is deterministic given the same input set, and this is now a
  requirement rather than a question. Weighting makes it load-bearing in a way
  dropping did not: the solve is iterative and warm-started, so its output depends
  on the order constraints and variables are visited in. Without determinism "the
  solver degraded" is not a debuggable claim, `layoutengine.md` bugs are not
  reproducible, and the layout test `testing.md` owes stays unwritable. The
  requirement is that a solve is a pure function of the committed block state, so
  the same block always produces the same layout.

  Two things follow that are not optional. Iteration order has to be defined
  rather than inherited from a hash iteration, which is a live hazard rather than
  a theoretical one: driftwm derives cluster membership from a `HashSet` and
  iterates it, so a derived-membership design that copies that shape inherits
  non-reproducible output. And the warm start has to be keyed on something stable
  across the animation, not on wall-clock or frame count, or a solve that happens
  to run twice with identical inputs will disagree with itself. This is also the
  constraint that limits §7.7's membership fork, and it is why that fork is not
  this document's to close.

Not blocked. Priority lives in the record and the record's shape is settled by
§3.1; what is left here is the band naming, which is a config surface question.

### 3.3 The constraint language a user writes

Open: the surface syntax, its semantics, and the storage representation coupled
to it.

- The four named constraints imply a shape of left-of-a, half-width-of,
  gap-around, fills-parent, which is a predicate plus an operand plus an implied
  subject. §3.1 fixes the subject as the current walk position and the operand as
  a walk index, a parent, or a literal, so "a tag or a monitor" is off the table
  and the language is smaller than it looked. Still unstated: whether constraints
  may reference each other, and whether any arithmetic on operands is expressible
  at all.
  `tomlparser.md` §11 already records that the config language exposes no
  expression syntax, so the default answer is no arithmetic, and that is a
  constraint on the language rather than an accident.
- The required set in §8 is the real test of this question, and it narrows it.
  Master and stack, deck, monocle and groups all reduce to nested layouts plus
  chrome toggles (§7.1, §7.4), so they need nothing new from the language. Grid
  versus tile is a leftover-space policy rather than a language feature
  (§7.4). What remains is the scroll axis, the canvas bounds, the pan and zoom
  space, magnetisation, and the snap relation, and only the first of those is a
  plausible constraint. The language therefore has to be expressive enough for a
  one-axis stack and a nested group, and the rest of the required set is
  built on top of that rather than inside it.
- `generaldesign.md` §7 names the layouts that must be expressible: master and
  stack, scrolling, binary space partitioning, grid, plus the floating and canvas
  styles `README.md` describes. Scrolling and infinite canvas are the hard
  cases, and §7.2 and §7.5 have since narrowed why: a scroller is a viewport
  into a larger space, and a viewport is a transform, not a rectangle the solver
  hands out. So the language is a language over rects and the pan and zoom
  space sits above it, which is a smaller problem than making the language
  express a viewport. Whether that separation survives contact with the solver
  is still open.
- The representation decision from §2.5 belonged to this question and could not
  be deferred out of it, because it is a format question. Resolved in §2.10: a
  new core tag above `0x33`, which is nearly free and was taken; an extension
  tag in `0x8000+` that the core never interprets and therefore cannot validate,
  now ruled out for anything the solver reads back; or a `tuple` of existing
  tags, free and constrained, which is what a `tuple` encoding would have cost.
  §7 constrained this further, and the constraint it imposed has since dissolved.
  A layout program's output has to be diffable for the animator, which looked like
  a demand for per-node identity inside the representation. It is not: the program
  places no nodes. `OMNI_SECTION_SOLVED_LAYOUT` carries the per-node
  `entry_ref` identity, one node per placed client, and that is what the animator
  diffs. The representation therefore needs no identity field at all, which is why
  `OMNI_TAG_CONSTRAINT` is 16 bytes and holds no entry reference.
- A config file is TOML, and `tomlparser.md` binds TOML literals to the core
  tags and adds none of its own. Whatever the constraint syntax is, a TOML user
  must be able to write it, which means either a table-shaped encoding that
  `tomlparser.md` §8 already supports as a `tuple`, or an amendment to that
  document. This is a cross-document consequence, not a detail of the language.
- The Mango and Hyprland interpreters at stages 16 and 18 have to produce
  whatever this language produces, over the same block, from a config format
  that knows nothing about it. So the language cannot depend on anything only
  the built-in parser can express, which is the same constraint
  `generaldesign.md` §17 places on the whole project.
- Whether there is a user-visible way to ask what the current constraints
  resolved to. An external program can read the block, so if the resolved
  geometry is not in the block there is no way for an external program to
  discover it at all, and if it is then there is a cost. That is §3.7.

Blocked by `windows.md` for the override representation only; §3.1 and §3.5 are
resolved. Blocks §3.4, `windows.md`, and `animate.md`.

### 3.4 Built-in layouts are compiled in, and adding one is not a solver change

Open: what a built-in's registration looks like. The boundary itself is decided.

`generaldesign.md` §7 has been amended. It used to say that built-in layouts
ship as preset constraint programs rather than as separate code paths, and that
adding one is therefore a config change. Both halves were false, and the second
was false before this document noticed: §7.1 equates a stack with a group plus a
rule plus a one-axis stack layout, and a group is a nesting concept from
§3.6, not a constraint program, so the most basic entry in the required set was
never a pure preset. The amendment keeps what was true, which is that the engine
is not extended per layout, and replaces the claim that was false.

The decision, as `generaldesign.md` §7 now states it: a built-in is compiled into
the compositor, and adding one means writing the layout, registering it, and
recompiling, with no new solver mechanism. A built-in that needs a mechanism that
does not exist yet is a claim that the mechanism was missing, and the claim
belongs in §7 first, where it is designed once rather than accreted one layout at
a time. The two mechanisms that do not exist yet are both already designed: the
group from §7.1 and the viewport from §7.2. The strongest evidence that at
least two built-ins are not constraint programs is §7.1 and §7.2, not §7.1 and
§7.4; §7.4 says grid and tile are the same constraints with a different
leftover-space policy, which is the opposite of what it was cited for.

Still open, and all of it downstream of the decision:

- What a built-in's registration actually is. Whether it is a value in the block,
  a named entry the engine expands, or a compiled-in default that seeds an
  ordinary key is still unstated, and the block-honest answer is a seeded value,
  which is exactly what `helpers.md` §3.1's option array already does for every
  other component option, so the mechanism exists and does not need inventing.
  This question survives the decision because being compiled in and being seeded
  are not exclusive: a compiled-in default that seeds a key is both.
- Which of the §8 set ship at stage 6 is unstated. `README.md` stage 6 says a
  single demo layout, and `generaldesign.md` §18 maps stage 6 to this document.
  On the required set, the cheapest genuine first deliverable is a one-axis stack
  in a nested group, because it needs only §7.1 and §7.3's attach mode and no
  viewport and no snapping.
- The existing `wm.cycle_layout` example passes a layout name
  (`ipc.md` §exec), so names are already part of the surface vocabulary. Whether
  a name selects a seeded key, an entry reference, or an `enum` constant is
  unstated, and it interacts with §3.5 because a per-tag layout needs a name
  that means something per tag.
- If a built-in seeds a value, then a user who edits one is editing a seeded
  value, and `ipc.md` §reset means a reset clears and rebuilds and the applier
  re-applies, so the hand edit is lost. That interaction needs stating rather
  than discovering. §7.1 sharpens it, because a stack is a group plus a rule plus
  a layout, and a user who edits any one of those three is editing a seed. This
  is the sharpest remaining consequence of the decision, because it is the one
  place where "compiled in" and "user editable" are in genuine tension.
- A built-in expressed only in the constraint language cannot do anything the
  language cannot do, so the shipped set is the real test of §3.3. It should be
  written after the language, not alongside it. On the required set, that test
  is only meaningful for the subset §9 identifies as the solver, which is a
  smaller claim than "the built-in layouts are presets" currently reads as.

### 3.5 How a layout is selected per tag

Resolved: the selection model and the identity underneath it. `tags.md` §2 and §4
answer both, and the answers are recorded here because this is the document that
was blocked on them.

- **A tag is a catalog entry.** `tags.md` §2. A tag is referred to by
  `(entry_id, entry_generation)` through the existing `entry_ref` tag at `0x23`,
  which is the same shape a client already has. The candidates this section listed
  are settled: not a generated stable string, and not a name-mangling convention,
  because a tag is a container of several named values and a container in an open
  catalog is an entry with children. `wm.tags.3.layout` still does not exist, and
  now cannot, because a tag is not a number.
- **The tag holds a layout name.** A value under the tag's entry, naming a slot
  from `layoutengine.md` §4's closed set. This retires the storage half of the
  question, and a binding can carry the name as its argument exactly as the
  existing `wm.cycle_layout` example does.
- **A monitor displays an ordered list, and the head is the primary.**
  `tags.md` §4. The primary is the first still-set tag in chronological insertion
  order, it is derived from list order rather than stored, and it supplies the
  layout and everything else the tag holds. Floating is not among it, because it
  is client state. A secondary tag contributes clients only. This answers the
  "what is selected when a monitor displays several tags" question in the only way
  consistent with a tag owning its own layout: the primary's, and the newest does
  not win.
- **Floating is not a selection, because it is not tag data.** `tags.md` §6. This
  section asked whether floating is a layout the solver runs or a mode it respects.
  It is neither: which clients float is state each client carries, so a solve
  takes it as a per-client input and there is nothing to select. A floating client
  is exempt from the layout, not placed by a second one.
- **Per-tag layout state is ordinary configuration, not `WINDOW_DEPENTENT`.**
  This resolves the last bullet, which was a real fork under §2.6: a tag's layout
  is a persistent fact about a tag, so it is written by `save` and cleared by a
  soft reset like any other value. The `WINDOW_DEPENDENT` flag goes on the
  membership children instead (`tags.md` §5), because those are scoped to a live
  client and must not survive it. The arena cost §2.6 warned about is real for
  membership entries and is accepted there, not for the layout.

One consequence for the pipeline. A solve's input changes when the primary changes,
and the primary changes when the head of a monitor's list is unset. That is a
rearrangement of every client on the monitor, not an incremental one, and §3.7's
question about what a second update does mid-animation now has a case that is not
hypothetical: unsetting the head while an animation is in flight is a full
re-solve triggered by a membership change rather than by anything the user did to
a window.

Blocks §3.4 and `windows.md`. §3.1 is resolved and no longer waits on this.

### 3.6 Nesting: a child solver, acyclic, and five deep

A group names a layout, and that layout is **a child solver instance with its own
constraint set**, not a set of constraints qualified by its parent. A stack is a
group, a deck is a group whose inner layout differs, and so a deck is two nested
layouts rather than one, which is what §7.1 needs in order for a deck to be
rearranged independently of the layout that contains it. A group has its own
active layout and inherits nothing from its parent, so moving a group changes its
rect and nothing about how its children are arranged inside it.

Two bounds, and both are needed. The first is the one that prevents a crash and
the second is the one that prevents absurdity.

**No layout may transitively contain itself.** Layout alpha cannot nest anything
that contains alpha, not even with other layouts in between, so the layout
reference graph is acyclic. This is the rule that stops the unbounded case: if
alpha is configured with two spaces that are both groups using alpha, then
arranging alpha requires arranging alpha, which requires arranging alpha, and the
cascade never terminates. Without the rule that configuration is not a bad layout,
it is a compositor that stops responding, and the failure is a hang rather than a
mistake the user can see and correct. Acyclicity also gives nesting a hard
ceiling for free, because the chain can visit each layout at most once: 26 in
principle, being the 24 user layouts chained into each other with a built-in at the
end.

**Maximum depth 5**, as `OMNI_LAYOUT_MAX_NEST_DEPTH` in
`include/shared/omni_layout.h` beside the other limits. Five is well inside the 26
that acyclicity allows, and it is the point past which nesting stops being
usable: a layout five groups deep is already at the limit of what can be
shown on screen, so the configurations the extra depth would permit are ones
nobody could read. Taking a separate, lower cap rather than relying on the 26 is
what makes the arrangement cost bounded as well as the recursion safe, since the
depth is what multiplies the number of solvers the arrange pass has to run.
No built-in nests doubly, so 5 is not reachable from the shipped layouts alone
and a user has to build all of it.

Both bounds are checked when a program is loaded, not when it is solved, and they
belong in the semantic guard of `configstorage.md` §12 next to the check that a
name is in the closed letter set. Rejecting the program at load means the user is
told which layout is at fault and the running layout is untouched, whereas
checking during an arrange pass means the pass has to unwind a partial cascade
before it can report anything.

Still open in this section:

- `generaldesign.md` §8 and §6 fix that a group is a nested layout with a
  titlebar that can be moved as one window, that a scratchpad lays out its
  contents, and that a cluster is a movement relationship whose members keep
  their own place in the parent layout. So two of the three nest and one does
  not, and the distinction between a group and a cluster is already decided and
  is stated as deliberate.
- The parent's view of a group is decided by §3.1's spaces rather than left
  open: a group is one space in the parent's program, so the parent places and
  constrains it as a single unit, and its members are visible only to the child's
  own program. The titlebar follows from that rather than being left open. A group
  is moved as if it were a single window and its members are focusable, so its bar is
  neither: it is a fake client in the group's own program, clustered with the
  group's members, positioned by the child solver's client rules like anything else,
  and the parent sees only the group as one space and never the bar separately.
- Unstated: whether the parent's constraints see a group as one client or as its
  members. `generaldesign.md` §8 says a group is moved as if it were a single
  window, which implies the former at the parent level, but a group with a
  clickable titlebar and focusable members is two things at once, and the two
  readings disagree about what a parent's left-of applies to.
- The depth limit and the cost bound are both settled above, and what remains is
  the consequence for §2.7. Nested solvers cost at least linear in constraints
  per level, the arrange pass runs on the event loop, and a depth cap of 5 turns
  "arbitrarily expensive" into "expensive by a known constant factor". The cost is
  still paid on the event loop, so the honest statement is that depth is bounded
  rather than that the pass is cheap, and `helpers.md` §6.1's rule that a callback
  must not block on another thread is a separate problem that this does not
  solve. §7.2's nested scrollers sit comfortably inside the cap: a horizontal
  scroller whose sections are each a vertical scroller is two levels.
- Scratchpad adds a further case, and §7.7 is the concrete form of it.
  `generaldesign.md` §8 says a scratchpad's windows are hidden until summoned,
  that both special tags can hold clients that are also on ordinary tags, and
  §7.7 requires a minimized client to stay on its tag's client list while being
  in no visible scope. A client in two scopes is in two constraint sets, only
  one of which is visible, and which one the solver is responsible for is
  unstated. The minimized case is the harder one because the client is in *no*
  scope rather than two, so there is nothing for the solver to satisfy and
  something for a switcher to display, and §7.7's conclusion is that a switcher
  reads membership rather than a solved layout.
- Cluster is now load-bearing in three places rather than one. `generaldesign.md`
  §10 defines it, §7.5's gravity canvas needs cluster-on-attach, and §7.3's
  replace spawn mode pushes a displaced client somewhere that in a master and
  stack layout is a stack. All three are the same membership question wearing
  different clothes, and the movement half of the concept may be the smallest
  of the three.
- Fake clients arrive here too. `generaldesign.md` §13 makes a fake client a
  layout participant, so a bar laid out by this engine is a client with a
  constraint record, and a bar that a user resizes must be re-solved like
  anything else.

### 3.7 The arrange pass

The pipeline in §6 now fixes the shape of the pass, so what is left here is the
part the pipeline does not answer.

- **Where the produced geometry lives is settled, and the remaining half of the
  question is not.** §6 establishes that a solved layout is a written value and
  an arranged layout is a live one, which removes the choice between "geometry
  in the block" and "geometry in memory" as a single question. The solved half is
  answered: `OMNI_SECTION_SOLVED_LAYOUT` (§2.10) is a fixed, non-journalled
  section, so produced geometry is in the block and any program that can map the
  block can read a layout, diff it, and author against it, at the cost of one
  bounded memcpy and a `generation` bump per solve rather than a commit. The
  arranged half is still open: whether an *arranged* layout is also written, and
  if so what distinguishes it from the solved one. Writing both would mean the
  block carries both a plan and its execution, and the pair is what an animator
  would need to interpolate; writing only the solved one means a script can
  always compute the arrangement itself and the compositor holds no authority
  over it.
- **What triggers a pass.** `generaldesign.md` §7 says the solver produces
  geometry on each arrange pass, and §6 says a pass begins with a call to update
  the layout, but nothing says who makes that call. The candidates are a change
  to a constraint key, a change to the client set, a monitor or scale change, a
  tag change, a viewport change, and a user action. Per §2.7 the first is a
  journal subscription and the rest would need to be triggers as well, so this
  is also the decision about what the engine's trigger patterns are.
- **Whether the pass is idempotent for an unchanged input set**, which decides
  whether it can be called defensively on every relevant commit without causing
  churn. §7's determinism requirement is about the solve; idempotence is the
  stronger and separate claim that solving twice changes nothing observable.
- **Whether the pass is re-entrant.** `helpers.md` §7 allows a callback to
  commit and re-enter dispatch one nesting level deep, and §7.3 and §7.7 both
  produce commits from inside an interaction, so a commit arriving during a
  solve is reachable in normal use rather than theoretical. The nesting guard
  catches self re-entry; it says nothing about what a solve does when its own
  input set changes underneath it.
- **What happens when a second update arrives while an animation from the first
  is still running.** §6 fixes the pipeline for one update and says nothing
  about two overlapping. Interruption, queueing and reversal are three different
  answers with three different visual results, and this is the most likely place
  for the pipeline to have an unstated hole in it.
- **Where the engine sits in activation order.** §2.7 notes that `helpers.md` §11
  makes priority advisory, so a wrong band is an init failure rather than a
  detected violation, and a layout engine that activates before there are
  clients has nothing to do while a layout engine that activates after tags
  cannot lay them out at `init`. Whether the engine is a component at all, or a
  service borrowed the way the store is (`server.md` §1), changes its band, its
  `enable_key`, and whether it appears in the registration table in
  `core/server.c` at all. §9 sharpens this, because four of the six mechanisms
  in its table are not the solver and may not belong to the engine component at
  all.

## 4. The layout namespace

A layout is one addressable thing that a user defines once and then applies as a
unit. This section fixes what a layout is called and where its name lives, which
`missing-devnotes-topics.md` Q6 left open and which §3.5 needs in order to be
answerable.

### 4.1 One namespace of layout names

A layout is a layout. There is no tier distinction between one the compositor
ships and one a user writes, because there is no behavioural difference to
distinguish: both are named programs of rules and spaces, both are resolved
by the same lookup, and both are referred to by a name under a single prefix. What
differs is only where the program comes from and what it is called.

- **Built-in layouts** ship compiled into the compositor and carry ordinary
  descriptive names: `monocle`, `dwindle`, `vertical_stack`, `master_stack`, and
  the rest of §8's set. A built-in is referenced by its own name.
- **User layouts** live in the block and carry the Greek letters, `alpha` through
  `omega`. A user layout is referenced by its letter.

Two properties fall out of the name choice rather than being enforced, and that
is the reason for it. A Greek letter cannot collide with a descriptive name, so
**a user layout can never shadow a built-in** without any precedence rule, any
reserved list, or any guard that exists only to prevent it. And the letters stay
abstract, so a user can define `delta` as whatever their config needs delta to be
and change that definition later without touching anything that refers to the
name; nothing in the system may come to depend on what a given letter means.

Every user layout **starts as a blank slate**: an empty program, which §7.8 fixes
as the blank canvas and which is a valid program rather than an absence. Nothing
is pre-populated, so a letter means nothing until its owner gives it a definition,
and there is no default layout for a slot to fall back to. This is a startup
requirement, not a lazy default: `configstorage.md` §13's seeding step creates all
`OMNI_LAYOUT_SLOT_COUNT` of them at boot.

### 4.2 Why a closed set, and what it costs

A closed set means the core owns a compile-time table of every valid layout
name, and a name outside that table is refused. This is the whole reason the
owner chose letters over user-chosen names, and it is worth being explicit about
what it buys and what it spends.

It buys the absence of a name-registration system. A user-defined name would
need registering, lifetime, collision detection against built-ins and against
other users' names, and a removal path, and `helpers.md` §8 is explicit that
unregistering by name is deliberately not offered because a name does not
identify a registration. A closed set needs none of that: a name is either in
the table or it is not, and the table is fixed at compile time, so there is no
registration, no handle, and no lifetime question.

It spends generality, in three specific ways that should be recorded rather than
discovered later:

- **A fixed capacity.** The user gets N slots and no more. §4.4 sets N.
- **A sharing hazard.** Two configs that both use "delta" mean different things
  by it, so a config that travels between machines, or a set of fragments that
  are composed, has to agree on what each letter means. This is the price of the
  abstraction and it is not removable, only documentable.
- **No self-describing output.** Anything a human reads, a bar's layout
  indicator or a `list_layouts` reply, shows "delta" and not what delta is. The
  only way to make that legible is a user-supplied label, and §4.5 says whether
  one is allowed.

### 4.3 The name is a value, so the existing enum tag carries it

`configstorage.md` §4 defines `enum` at `0x24` as the string name of an enum
constant, and §12 places enum-name recognition in the semantic guard tier, where
the consumer decides what a name means and refuses what it does not recognise.
A layout name is exactly that: a string constant from a closed set, checked by
the consumer that cares. So the whole namespace mechanism needs no new tag at
all, which removes the one item in §2.10 that would have needed a new tag.

A layout name is not an `option` and not a bare string. `option` at `0x25` is a
nullable typed value and carries no name of its own, and a plain `string` at
`0x0C` would be structurally valid for any bytes and would push the closed-set
check onto every consumer that happens to read the key. `enum` makes the check
part of reading the value, which is where §12 already wants it.

The guard consequence belongs to `helpers.md` and not to the store: §6's registry
is a runtime registry of action handlers with handles and a release path, and
layout names must not be registered there. `configstorage.md` §12 already
separates "enum name recognition" from "action-name registration", so the store
needs no edit; §6 needs a sentence saying the layout table is a compile-time
constant and is deliberately not a registry, so nobody wires it up as one later.

### 4.4 Slots are keys, so the table is a count and not a grammar

Each slot is an ordinary catalog key whose name component is the letter, holding
a constraint program:

```
omniwm.layouts.delta    = <program>   # a user layout, a Greek letter
omniwm.layouts.eta      = <program>
omniwm.layouts.monocle                  # a built-in, compiled in, no stored definition
```

One prefix for all of them, which is what makes this a single namespace rather
than two that happen to be dispatched together.

A catalog entry's name is an arbitrary immutable UTF-8 string
(`configstorage.md` §3), so `omniwm.layouts.delta` is simply a name and nothing has to
parse it: the dotted form is a convention the layout code reads, not a path
grammar the store implements. That is the cheaper of the two possible designs
and it is what this document assumes. The consequence is that recognising a
slot is prefix matching on a flat name, so the guard that checks a name against
the closed set is a convention the layout owner has to keep, and a name like
`omniwm.layouts.delta.bak` would be a different entry that no guard rejects.
Adding `omniwm.layouts.theta` needs no ABI change and no registration.

The number of letters the system recognises must be a constant in
`include/shared/omni_layout.h` next to the other limits, and the names must be
listed there rather than generated from a rule. A closed set is only checkable if
it is enumerable, and the owner should also decide the trade explicitly: a fixed
table of 32 letters is trivial to check and cannot be extended at runtime, while
a larger cap wastes nothing but names. The cap is only worth revisiting when a
real user runs out of slots, and until then the smaller table is the one that
makes "is this a valid layout name" a single lookup.

Taken as `OMNI_LAYOUT_SLOT_COUNT = 24` with the whole Greek alphabet as
`OMNI_LAYOUT_SLOT_NAMES`, seeding one blank program per letter at startup. The count is the number of *user* slots, and the built-
ins are not among them: the count is the number of *user* layouts, and a built-in
like `omniwm.layouts.monocle` is named in the same namespace without occupying a
letter. 24
is the whole alphabet, which is the only defensible reason to stop there: it is a
closed set because the alphabet is, not because the number was convenient.

The cost of the fixed table is that extending it is an ABI edit, and the benefit
is that "is this a valid layout name" is one lookup in a compile-time list. Given
24 user layouts and however many built-ins ship, running out is not a credible
failure mode, so the cap stays until someone demonstrates otherwise.

### 4.5 A slot has a definition, a dispatch, and a lifetime

Three operations, and they are not the same operation:

- **Define.** Constraints are written into a slot key. An ordinary `set`, an
  ordinary commit, subject to the guards in §4.3. §4.6 covers the case where
  the writer is an IPC client rather than a config file.
- **Dispatch.** Something asks for a layout by name. The name is the `enum`
  value, the consumer is the layout engine, and the result is a constraint
  program to solve.
- **Apply.** The §6 pipeline, with the resolved program as its input. Applying
  a name is not itself a step in §5; it is what produces the input that §5's
  first step consumes.

An empty slot is valid and is not an error. It holds no rules, and a
layout program with no rules is not the absence of a layout: it is a
blank canvas, an infinite unconstrained space with no tiling and every client
floating. So a slot the user has never touched dispatches successfully and
produces a working layout, and a typo in a layout name is caught by the closed
set in §4.4 rather than by the emptiness of the slot. This also means the empty
program has to be given defined semantics in the language itself, which is
§7.8's requirement and not a special case in the engine.

Whether a slot may carry a user-facing label in addition to its letter is open
and is deliberately not decided here. It is the one thing that would make
§4.2's third cost go away, and it is also the thing that reintroduces exactly the
user-chosen names the closed set exists to avoid, so it is a real trade rather
than a missing feature. With 24 user slots confirmed as generous beside the
built-ins, the trade is worth taking the closed-set side of: a label would let a
user describe a layout in their own words, and a letter cannot.

### 4.6 A slot is built at runtime over IPC, then saved out

A slot is not only something a config file declares. The intended workflow is
that a slot is authored interactively and then extracted, which makes a slot a
live editable object rather than a read-only config artifact:

1. Load a fresh, empty slot. By §4.5 it is already valid, so there is nothing to
   clear and no error to survive.
2. Configure it over IPC, one rule at a time, watching the canvas respond
   as each solve lands.
3. Run `save omniwm.layouts.delta.*` to write the configured slot out to a file,
   so the layout can be kept, shared, and loaded back later. Plain `save` would
   write the whole block, which is correct but is not an extraction.

Three requirements fall out of that, and none of them is currently satisfied by
the documents as written.

- **`set` must accept a constraint program.** `ipc.md` already has a `set`
  command that writes an arbitrary catalog key, so the mechanism exists, but the
  program needs a wire encoding as a row in `ipc.md` §3 and a guard in
  `configstorage.md` §12. This is the same new tag as §2.10's first item, and it
  is the one that is now unavoidable rather than merely likely.
- **`save` must be able to select a subtree, not only the whole block.**
  Satisfied, and `configstorage.md` §13 owns the answer. `save` with no argument
  writes the whole block under the existing exclusion test, which is what it
  already did, so the argument is purely additive and nothing that relied on the
  old behaviour changes. With an argument it narrows to a key-path pattern, a
  dotted prefix with a single trailing `*` meaning the subtree below it, so a
  layout authored over IPC at `omniwm.layouts.delta` is extracted with
  `save omniwm.layouts.delta.*` and the resulting file contains that layout and
  nothing else. The exclusion test is identical inside a narrowed scope, so a
  narrowed save cannot leak window-dependent state, and it is the same test a soft
  reset uses, so the three operations still cannot disagree about what counts as
  configuration. A general glob is deliberately not supported: one trailing
  wildcard is enough to extract a namespace, and a pattern language would make the
  exclusion interaction harder to reason about than it needs to be.
- **The result has to be loadable.** An extracted slot has to be a file that a
  later config load can read back into a slot, which means the constraint
  program's file encoding is the same value as its IPC encoding. That is
  `tomlparser.md`'s existing rule for every other type, so it is a consequence
  rather than a new decision, but it does mean the tag cannot have an
  IPC-only shape.

This also settles something §4.2 left implicit. A closed set of names is not
only a way to avoid a registration system; it is what makes a slot a thing that
can be handed around. A user-built layout has to be referable by something
stable, and a letter is stable in a way that a user-chosen name is not, because
the letter never has to be agreed on with anyone.

### 4.7 What the namespace does not cover

- **Tags and monitors.** Whether a tag or a monitor names a layout independently
  of the current one is §3.2's open question about layouts-as-values, and the
  namespace neither answers nor obstructs it.
- **Groups.** §7.1 resolves a stack to a group with no bar, and §7.5's deck to
  monocle inside a group. Whether a group carries its own layout
  reference is open, and the namespace makes it a one-field question rather than
  a naming one.
- **Node identity across a definition change.** §2.10 settles that a solved
  layout's per-node identity is an `entry_ref`, so a layout's *name* is not node
  identity and redefining a slot does not need to preserve it. Under §3.1 the
  program holds no identity at all, so a redefinition cannot invalidate a node
  reference: the `entry_ref` is the client's, and it is stable across a solve of
  an unchanged or a changed program alike. The §6 animation needs node identity
  stable across a solve, and it gets it from the solved layout rather than from
  the program.
- **Spawn modes.** §7.3's five modes are a property of a program, not a layout
  name, so a slot that sets one carries it and no layout name encodes it.

## 5. The update pipeline

The arrange cycle is fixed, and it is the single most load-bearing thing in
this document, because it is what separates a solved layout from an active one
and therefore what makes animation and the solver the same mechanism rather
than two that have to be reconciled.

In order:

1. a call to update the layout is made;
2. **the cluster resolution pass runs**, collapsing each cluster to the single
   virtual client the program will place. It goes first because a program's
   geometry rules and occupancy rules are stated in terms of the occupants it
   places, and a cluster is one occupant however many clients are in it.
   `layoutlanguage.md` §3.8 has the pass and why it is a pass and not a
   mapping detail;
3. **the mapping phase assigns clients to spaces**, in three parts: sources
   publish intents, a space that is already taken is detected as full, and
   §3.1's resolution rules reflow the surplus to a fixed point. This step touches
   no geometry and it is where every membership change happens;
4. the solver solves the new layout, over spaces rather than over clients;
5. the relaxation pass runs over the solved geometry, if the program has one;
6. the solved layout is **written but not active**, that is, not yet arranged;
7. animations are triggered, if they are enabled;
8. the animation animates the state transition from the current layout to the
   new layout, and this step is skipped entirely if animations are disabled;
9. the new layout is **arranged**, which is what activates it.

The mapping phase at step 3 is the newest of these and has the most
consequences. It is why the pipeline is nine steps rather than six, and it sits
before the solve rather than after it for two reasons. A reflow is a membership
change, so a layout that has not decided which client is in which space has
nothing to solve for; and the solve stays a pure function of the mapped set, which
is what keeps §3.2's determinism requirement and §3.1's built-in policy intact.
Placing it after the solve would mean the solver ran against a set the pipeline
was about to change underneath it.

The consequences this fixes, none of which any existing document had stated:

- **A solved layout is a value, and it is not the live geometry.** There are
  two states of the same thing, a solved state and an arranged state, and
  "arrange" is the transition between them rather than a synonym for "solve".
  This is the answer to the question §3.7 raised about where produced geometry
  lives, and the answer is that the question was malformed: a layout is a value
  that passes through a solve, and a node's placement is a consequence of an
  arrangement rather than the output of a solve.
- **The solver never animates and the animator never solves.** A client whose
  window is mid-transition is showing a third thing, neither the current layout
  nor the new one, and what it is showing is not a solver result and not a
  layout. Any scene node that can be animated (`generaldesign.md` §11) can
  therefore be placed by the solver and moved by the animator, and no other
  component needs to know which of the two is currently authoritative for it.
- **Relaxation runs before the write, not after it.** §7.5 settles that
  magnetisation is a post-solve relaxation rather than a soft constraint, and
  §8's table gives relaxation its own row, so the pipeline has to say where it
  lands. It lands at step 5, between the solve and the write, and the placement
  is load-bearing. If relaxation ran after step 6 then the value in the block
  would be the solver's answer while the user saw the relaxed one, which breaks
  the first consequence above, since the solved layout would no longer be the
  thing the arrange acts on, and it would break §7.8's guarantee that a skipped
  animation cannot change the endpoint. Putting it at step 5 costs one thing and
  buys that: the solved layout is no longer purely the solver's output, it is the
  solver's output after a fixed, geometry-only, non-iterating pass. That is a
  weaker claim than "the solver produced this" and it is still strong enough,
  because relaxation never re-runs the solver and never feeds back into it.
- **Relaxation is part of the endpoint, not a decoration on it.** A layout that
  magnetises and a layout that does not are different layouts, not the same
  layout rendered differently, so relaxation has to run on every solve including
  the one behind a skipped animation, and its result has to be what step 6 writes.
  The consequence for `animate.md` is that the animator is handed a relaxed
  layout and has no idea it was relaxed, which is the correct split and is the
  same split the first consequence draws for the solver.
- **A skipped animation is a first-class outcome, not a degraded one.** Step 6
  disappearing is the normal path for a user who has turned animations off, so
  nothing may depend on an animation having run. The layout is arranged whether
  or not anything moved it there.
- **Triggering is separate from animating.** Step 5 starting an animation and
  step 8 running it are two events, so a layout that changes twice in quick
  succession has a defined case that is not yet written: what happens to an
  animation already in flight when a second update arrives. Interruption,
  queueing, and reversal are all open, and the pipeline does not answer them.
- **The write at step 6 is observable before the arrange at step 9.** Whatever
  a commit carries, the state a watcher sees at step 6 is not the state a user
  sees at step 9. This is the same shape as the readiness question in
  `server.md` §6, where a readable field is not the same condition as a
  serviceable one, and it is worth deciding deliberately whether a client may
  observe the intermediate state.

A consequence for `animate.md`: the pipeline makes the animator the owner of
the transition and the solver the owner of the endpoint. That is the split §3.7
said could not be decided here, and it is now decided.

## 6. The update pipeline as a constraint on the language

The pipeline in §6 means a layout program is evaluated twice in a sense, once
to produce the solved state and once implicitly as the animation replays the
difference. Three properties follow, and all three are requirements on §3.3
rather than observations about it:

- **The solve must be a pure function of its inputs.** If it is not, the
  animation has no stable target to move toward and a re-solve mid-flight
  produces a different answer. This is the same determinism requirement §3.2
  reached from the other direction, now with a second, independent reason.
- **A layout program's output has to be diffable.** The animation is a
  transition between two solved states, so the animation needs to know which
  node changed, from what value to what value, at what priority. §3.1 answers
  where that identity comes from: not from the program, which places no nodes, but
  from `OMNI_SECTION_SOLVED_LAYOUT`, whose nodes are the client's `entry_ref`.
  So the identity is the client identity from `windows.md`, and it is
  the same question as the fake-client case in §3.6.
- **Skipping the animation must not change the endpoint.** Otherwise "animations
  disabled" is a different layout rather than the same layout reached faster,
  and the two configurations would not be comparable.

## 7. Required layouts and the primitive each one needs

The target layout set, and what each layout actually requires of the engine. The
second column is the point of the table: several of these need something that is
not a constraint over rectangles, and naming which is how the engine's real shape
gets found.

| layout | source | primitive required |
|---|---|---|
| master and stack | mango | create a stack |
| deck | mango | create a stack, plus a monocle inside it |
| vertical stack, horizontal stack | derived | stack all windows in one axis inside a provided space |
| horizontal and vertical scroller | mango | per-axis monitor bounds, snapping and non-snapping scroll, optional grid alignment |
| infinite canvas, floating | driftwm | freehand pan, zoom, no bounds |
| infinite canvas, tiled | newm | grid alignment, windows as a fraction of the monitor, zoom that snaps to client edges |
| infinite canvas, gravity | hyprland, driftwm | post-solve magnetisation, a relaxation and not a constraint (§5, §7.5); formerly attributed to halley, retracted |
| dwindle | mango | spawn modes: split-on-spawn, replace, attach, direct, floating |
| grid versus tile | mango | whether windows auto-expand to fill space |
| monocle and deck | mango | a defined area in which windows tile behind each other |
| floating snapping | wayfire | named snap areas, edge and monitor-relative, reachable by drag and by keybind. The relation to another client is a drag-time target, not a layout constraint, so it belongs to §7.6 and `input.md` rather than to the program |
| client minimization | wayfire | a client that is on a tag's client list but in no visible scope, and is ejected from that state by focusing it |

### 7.1 A stack is a group, and that is not an approximation

A stack is a group with no bar, plus rules that move clients into and out of the
group automatically. "No bar" is not a flag on the group: a group bar is a fake
client clustered with the group, per `generaldesign.md` §13 and
`layoutlanguage.md` §3.8, so a layout that wants one includes it and a layout that
does not, does not, and the difference between a stack and a Mango-style group is
which of the two the author put in the group rather than anything the group
declares about itself. Vertical stack and horizontal stack
are then simple layouts that stack all windows in one axis inside the space they
are given, and a deck-based layout puts a monocle-like layout on the stack.

This is worth stating plainly because it is a structural claim rather than a
description. `generaldesign.md` §8 already makes a group a nested layout with a
titlebar, so a group with its chrome switched off is a nested layout with no
chrome, which is exactly a stack. Master and stack therefore needs no new
engine concept at all: it is a group plus a rule plus a one-axis stack layout.
Deck is the same thing with a monocle on the stack, and a monocle is the next
row.

The same identity holds in the other direction, and it is the reason the table
reads the way it does. `generaldesign.md` §8 says a group is picked up and moved
as if it were a single window, so a group with a group bar is a group with
chrome, which is what a Mango-style group is. One concept, two
configurations, and the difference is a visibility toggle on the titlebar rather
than a second kind of group. This is the design paying off rather than a
workaround: the project already needed nested layouts for groups, and the stack
turned out to be the same thing with a decoration switched off.

It does mean one thing is load-bearing that is not written down anywhere, and the
language has since split it in two. A group bar is **a fake client**, per
`generaldesign.md` §13, and its geometry is stated as client rules in
`layoutlanguage.md` §3.6. So *whether* a group has a bar is not a decoration
setting: it is which fake clients the group's program contains, and a stack is a
group whose program contains none. What `decorate.md` owns is the bar's
*appearance*, which `generaldesign.md` §11 makes configurable per window. A group is
a layout construct whose chrome is a drawn client, and a drawn client is two things
at once: a layout participant that the solver positions, and a set of effects that
the decoration layer renders. Both documents have to agree on that boundary, and the
line is presence against appearance rather than layout against decoration.

### 7.2 Scrollers and the canvas bounds

A scroller needs three things, and the first is the one that decides whether
infinite canvas is a separate layout or the same one with a different setting.

- **Bounds, per axis.** Whether the canvas is bounded by the horizontal edges,
  the vertical edges, both, or neither is four states, and they are two
  independent booleans rather than a four-way enum, because being able to set
  either or both is what makes infinite canvas fall out as neither rather than
  as a separate mode. Infinite canvas is not a different layout. It is a
  scroller with both bounds off, and that is a much smaller thing to build and
  a much smaller thing for a user to learn.
- **Snapping and non-snapping scroll.** Both must be selectable, per axis if
  not globally. The reason is grid alignment, below.
- **Grid alignment**, as a toggle. With it off, a scroller's sections are
  whatever the layout program produced. With it on, sections snap to a grid, and
  a horizontal scroller whose sections are each a vertical scroller becomes
  expressible, which is also what makes newm-style tiled infinite canvas
  expressible.

This resolves part of the §3.3 open item about infinite canvas. It is still true
that a scroller is not a partition of one rectangle, because a viewport into a
larger space is not a rectangle the solver can hand out. What the bounds toggle
settles is that the viewport is a separate, small, ordinary object with four
states, rather than a new solver. The coordinate space behind the viewport is
still unsolved and is still the hard part.

### 7.3 Spawn modes are not layout

Dwindle is handled primarily by spawn modes, and the five modes are:

| mode | behaviour |
|---|---|
| split-on-spawn | split the focused window in half |
| replace | replace the focused client with the new one, pushing the focused client out of the way, for example onto the stack in a master and stack layout |
| attach | create the window at a defined side of the focused window, which is how scrollers usually work |
| direct | spawn the window tiled where the cursor is |
| floating | as direct, but the window floats by default |

The important structural point is that four of these five are not constraints.
`split-on-spawn` and `attach` change the constraint set, so the solver can
honour them. `direct` and `floating` place a window at a point, and a point is
a constraint. But `replace` does not describe a geometry at all: it describes
what happens to a client that is being displaced, and the answer is "somewhere
else, chosen by the layout that receives it", which on a master and stack layout
means the stack. That is a client-set mutation plus a re-solve, not a
constraint.

So spawn modes are a layer above the solver: they decide which clients are in
a scope and what the arriving client is seeded with, and then the solver arranges
the result. `direct` and `floating` are the two that blur this, because they
need a cursor position converted into a placement, and that conversion is the
same problem as the snapping in §7.6, solved once. That is worth extracting
rather than writing twice, and the extraction is a decision this document has
not made yet.

Two things follow that are not in the user's list but are implied by it.
`attach` at a defined side is what makes a scroller a scroller, so the scroller
row and the spawn-mode row are the same feature seen from two ends, and one of
them is redundant. And if a spawn mode can push a client onto a stack, then a
spawn mode is a client-set mutation, which means it is a rule, which means
`generaldesign.md` §8's window rules and this list are the same mechanism
arranged differently, and the doc should say which is which.

### 7.4 Grid versus tile, and monocle

Grid versus tile needs one thing: whether windows auto-expand to fill the space
they are given. That is a solver behaviour flag rather than a language feature,
because it changes what the solver does with slack rather than what the user
wrote. A layout that auto-expands and a layout that does not are the same
constraints with a different leftover-space policy, which is a useful thing to
know because it means grid and tile differ by one bit rather than by two
layouts.

Monocle and deck need the ability to define an area in which windows tile behind
each other. Combined with §7.1, a monocle is a monocle-like layout in a defined
area, and a group with a group bar on that same area is a Mango-style group, so
the two are one feature with the chrome toggled, exactly as the stack and the
group are.

### 7.5 Freehand canvas, tiled canvas, and gravity

- **Floating infinite canvas, from driftwm:** freehand pan around the canvas, and
  zoom.
- **Tiled infinite canvas, from newm:** grid alignment, windows defined as a
  fraction of the monitor such as full, one half, or one quarter, and zoom that
  aligns to clients by snapping to client edges, so zooming in on the grid leaves
  clients exactly fitting.
- **Gravity infinite canvas, from halley:** *retracted.* Halley was cited here as
  a project whose clients magnetize to each other and cluster on attach. Read at
  the source, it has no `magnetise`, `magnetize` or `snap` function at all, and
  the only "snap" in its cluster code is a doc comment saying the opposite, that
  releasing a member returns it to the layout rect without snapping
  (`src/clusters/mod.rs:1377-1378`). What it actually does is a hard rect test
  comparing a dragged window's already-computed bounds against an already-known
  core position (`src/clusters/membership.rs:11-18`, `:48-49`), which is a
  proximity test feeding membership, not an attractive force shaping geometry.
  Its `Field` is likewise not a user-facing pan and zoom canvas: it is a spatial
  index with a world radius and a grid, used by bloom. This citation is removed
  rather than corrected, because there is no gravity canvas in Halley to cite.

These two share one requirement and it is the largest one in the document:
**a pan and zoom coordinate space that the geometry lives in.** Drift needs a
free viewport, newm needs a viewport that snaps to a grid of client edges. Both
are transforms on top of a space, and neither is a constraint on a window inside a
monitor-sized rectangle. This is the §3.3 open item about infinite canvas
arriving with two independent confirmations rather than three, which is still two
from two unrelated projects and is enough to keep it open, and the transform
belongs to the canvas rather than to the solver, so that a solver answer is
expressed in canvas coordinates and the viewport is what maps them to an output.
The magnetisation requirement survives this retraction on other evidence: Hyprland
applies forced-edge snapping after the solve, and driftwm resolves a magnetic
region after tiling has already produced rects, so the attract-then-place shape
is real even though the project cited for it does not have it.

Newm's "a fraction of the monitor" is the second confirmation that `ratio` and
`percent` are load-bearing, and it is the first concrete use of them in a layout
rather than as an option. Whether a window's size is a fraction of the canvas or
an absolute size, and whether the two can be mixed, is open.

Magnetisation is a post-solve relaxation rather than a soft constraint, and this
is now settled rather than open. Magnetizing to the nearest client edge is a
search over nearby geometry followed by an exact placement, and the difference
is observable: a soft constraint competes with every other constraint by priority,
whereas a relaxation is applied after the solve and competes with nothing. Only
the first is expressible in the constraint language, which is why the pipeline
carries a relaxation step at all (§5) and why §8's table gives it its own row.
The evidence for the shape is Hyprland's forced-edge snapping and driftwm's
magnetic region, not Halley.

Halley's cluster is `generaldesign.md` §8's **group**, not its cluster, and the
distinction is not cosmetic. A Halley cluster is a named persistent workspace
with its own layout, holding its members in either tiling or stacking
(`docs/clusters.md`, with the layout living in shell-side
`ClusterMetadata { name, output, layout, core, core_position }` at
`src/clusters/mod.rs:77-83`). A group is a nested layout with a titlebar, so
Halley's cluster is a group. `generaldesign.md` §8's cluster is the other thing
entirely: a movement relationship with no internal layout and no chrome of its
own, occupying one space of its parent rather than one per member. So Halley is not evidence
that the project can build our cluster; it is evidence that our group is a real
and precedented shape, and it means our cluster is the one concept in this table
with no external precedent at all. The nearest neighbour is driftwm's derived
clusters, and driftwm declines to make them first-class on principle, which is a
deliberate disagreement rather than an absence of prior art. This is the second
place in this document that reaches into `windows.md` for a concept it does not
own, and the one where the reach is largest.

### 7.6 Snapping is an interaction, not a constraint

Floating window management snapping, from wayfire, needs named areas such that
dragging a window there will snap it, and it needs to be defined what the window
snaps to:

- drag to the left side, snaps to the left half of the monitor;
- drag to the right, snaps to the right half of the monitor;
- drag to the top, snaps to fullscreen;
- dragging onto the side of another client snaps to an area related to that
  client, rather than to the monitor.

And all of it must work from a keybind as well as from a drag.

This is three separate things and the table in §9 compresses them, so they are
separated here:

1. **A snap-area registry.** Named areas defined against a monitor, and areas
   defined relative to a client. The relative form is what the last bullet needs
   and it is the one with no precedent in a layout language, because it makes
   the result depend on which client was hit, so a drag has to resolve a
   candidate client before it can resolve an area.
2. **A hit test.** Turning a pointer position, or a keybind with a direction
   argument, into a chosen area. This is the same conversion `direct` and
   `floating` spawn modes need from §7.3, and it should be one function.
3. **A snap, which is a constraint.** Once an area is chosen, "occupy this rect"
   is one constraint, and the solver handles it like any other. Drag and keybind
   differ only in how they produce the area.

The keybind requirement has a consequence worth stating. `helpers.md` §11 records
the key-to-action binding path as not yet designed, and a snap-by-keybind is an
action that takes a direction and produces a constraint on one client. So the
input design owns how a keypress becomes an action invocation, this document
owns what the snap action does, and between them there is now a concrete
requirement rather than an open question.

### 7.7 Minimized clients are on the tag but in no scope

Client minimization, from wayfire: a client that is minimized to a scratchpad
stays in the client list for the tag it came from, so it still appears on
overview and alt-tab style switchers and on things like docks and bars, and
focusing it kicks it out of the scratchpad automatically.

This is the concrete form of the case §3.6 flagged as unresolved. A minimized
client is a member of its tag's client set and a member of no visible scope at
all, so it is in a client set and outside every layout, and the two facts have
to be held simultaneously without either one implying the other.
`generaldesign.md` §6 says a scratchpad's windows are hidden until summoned and
that both special tags can hold clients that are also on ordinary tags, so the
project already anticipated a client in two places; this is the requirement that
says what the second place is.

Two consequences:

- **Overview and alt-tab read a client set, not a layout.** A switcher that can
  show a minimized client is reading membership, and `generaldesign.md` §6
  already makes overview a special tag whose contents are the union of the
  others, so overview needs the client set and not the solved layout. That is a
  useful separation, because it means a switcher does not wait on a solve.
- **Focus is an ejection.** "Kicked out of the scratchpad when focused" means
  focus is not only a pointer at a client, it is a mutation of which scope
  contains that client. That is the second place in this document where an
  apparently simple interaction turns out to change a client set, after spawn
  modes in §7.3, and the two together suggest that client-set membership is a
  first-class thing the engine has to be able to see rather than something each
  caller edits.

### 7.8 The empty program is a blank canvas, and it has to mean something

§4.5 settles that an untouched slot is a valid layout rather than an error, and
that its meaning is an infinite unconstrained canvas with no tiling and every
client floating. That places a requirement on the language rather than on the
engine: a program with no constraints must be well defined, and it must be the
blank canvas and not a degenerate case the solver has to special-case.

It is worth being explicit about why this is a requirement and not a free
consequence, because the constraint system's natural behaviour is close to it
but not identical. A solver given no constraints has nothing to satisfy, so the
trivially valid answer is to place nothing, and a client that is in no placed
node is then in no scope at all. But §7.7 established that a client can be in a
client set and outside every scope, and that state means *minimized*. So an
empty program that placed nothing would make every client look minimized, which
is wrong. The blank canvas therefore has to place every client explicitly, at an
unconstrained position, with floating as its effective spawn mode. This is the
same conclusion §7.3 reached about `floating` from the other direction: floating
is not an absence of layout, it is the layout you get when nothing constrains
you.

Two things follow for the language:

- **Emptiness is a value, so it needs a representation.** A slot whose program is
  the empty program, and a slot that has never been written, both have to mean
  the blank canvas per §4.5, so the simplest design makes them the same value
  and the distinction unnecessary.
- **A client needs somewhere to go when nothing applies.** The language needs a
  way to say "this client is placed here, unconstrained", and the solver needs a
  defined position for a client that no constraint mentions. This is the same
  degradation path §2.3 already requires, reached from a different direction.

## 8. What the layout set implies about the engine's shape

The seven subsections above are the useful part of this section, and they are
worth collecting because they say the engine is not one solver.

| mechanism | what it is | belongs to |
|---|---|---|
| solve | constraints in, geometry out | this document, §3.1 to §3.3 |
| arrange | the solved value becomes the live one | this document, §6 |
| viewport | pan and zoom, a transform from canvas space to an output | the canvas, §7.2 and §7.5 |
| client-set membership | which clients a scope contains, mutated by spawn modes, focus, and minimization | `windows.md` and `tags.md`, §7.3 and §7.7 |
| snap-area resolution | a pointer or a keybind to a chosen rect | input plus this document, §7.6 |
| relaxation | magnetize, applied after a solve | this document, §7.5 |

Only the first two are the constraint engine. The other four are the "other
general logic" the layout set requires, and each of them has an owner that is
not this document. That is the most useful thing this section produces, because
it is the difference between designing a solver and designing a layout system,
and the requested layouts need both.

## 9. References

| reference | use |
|---|---|
| DuckWM, `~/repos/duckwm` | a working constraint-based solver, consulted directly, and the closest existing implementation of the mechanism in §5 |
| Mango | master and stack, deck, scrollers, dwindle, grid versus tile, monocle, groups |
| newm | tiled infinite canvas, grid alignment, fractional sizing, zoom that snaps to client edges |
| driftwm | floating infinite canvas, freehand pan and zoom |
| havel | retracted, see below |
| wayfire | floating snap areas, drag and keybind snapping, client minimization |

DuckWM is GPL (`~/repos/duckwm/LICENSE`), as is Mango
(`architecture-audit.md` §7), and `architecture-audit.md` §8 records GPL-3.0 as
the project's chosen licence. Code from any of these may be read and ported
under that licence, with the applicable notices preserved. The `licence.md` row
in `missing-devnotes-topics.md` is where the file itself and the per-source
provenance get recorded, and it is still unwritten.

Behavioural references and code references are different things and are used
differently. The window managers above are named for the behaviour they have
already designed, not for their code, and Mango is the one project
`architecture-audit.md` §7 has already read at the source level. DuckWM is the
first reference whose solver is the thing omniWM wants to build, so it is the
one whose code is worth reading line by line, and §3.1 to §3.3 are the questions
its source can answer.

**The havel row is a retraction, kept visible on purpose.** It was cited as a
gravity canvas with client magnetisation, on the strength of a name and a
one-line description, and it was never given a path, which is the tell. There is
no `havel` repository; the project meant is halley at `~/repos/halley`, and read
at the source it has no magnetisation and no user-facing canvas. The name was a
transcription error and the claim was an unverified inference built on top of it.
The layout requirement is unaffected, because Hyprland and driftwm supply the
evidence halley was supposed to, and the retraction is recorded rather than
quietly deleted because a wrong citation that went unnoticed once will go
unnoticed again if it is simply removed, and because halley turns out to be
genuinely useful for a different reason in §7.5. The general lesson is recorded
in `research/INTEGRATION.md`'s spirit: a behavioural reference with no path and
no read is an assertion, and this document had been carrying one as if it were
evidence.

## 10. What this document blocks, and what blocks it

Blocked, meaning the other document cannot be finished first:

| blocked document | what it is waiting on |
|---|---|
| `windows.md` | §3.1 leaves exactly one per-client thing for the solver to read, the user's override, so `windows.md` owns client identity, the override's representation and client lifetime; and §7.3, §7.5 and §7.7 all mutate client-set membership, which `windows.md` owns |
| `animate.md` | §6 fixes the split, with the solver owning the endpoint and the animator owning the transition, and §6 fixes what the animator needs to diff against |
| `draw.md` | a fake client is a layout participant (`generaldesign.md` §13), so the scene's own surfaces are arranged by this solver |
| `decorate.md` | a border or overlay is a scene node around a placed client, so placement precedes decoration; and §7.1 makes a group's titlebar a decoration toggle, which is `decorate.md`'s to own |
| `input.md` | §7.6's snap needs a pointer position and a keybind with a direction argument converted into a chosen area, and `helpers.md` §11 records the key-to-action path as undesigned |
| `tomlparser.md` | whatever the constraint syntax turns out to be has to be writable in TOML as a `tuple`, or the parser needs amending (§3.3) |

Blocking, meaning this document cannot be finished first:

| blocker | what is missing |
|---|---|
| `windows.md` | client identity, and the representation of the per-client override, which is the only per-client state the solver reads |
| `helpers.md` §11 | the key-to-action binding path, which is how a layout is chosen from a keypress, recorded there as not yet designed |
| `build.md` | the solver's arithmetic width, and whether a solve runs on the CPU or the GPU |
| `licence.md` | the provenance of anything ported from DuckWM or Mango, which §10 makes relevant and which is unwritten |

## 11. Open items

Carried forward, in the order they have to be answered rather than in the order
they are interesting. Three of these were closed by the §2.10 ABI pass and are
kept as a record of what the pass had to decide, marked *closed*.

- *Closed.* The representation of a constraint program (§2.5, §3.1) is
  `OMNI_TAG_CONSTRAINT` at `0x32`: a packed array of 16-byte records, no header,
  no per-record identity, operand as a walk index (`configstorage.md` §4).
- *Closed.* A `binding` carries an action argument (§2.10 item 2): a 24-byte
  header plus a positional string array, `args_ref == OMNI_REF_NONE` when there
  are none (`configstorage.md` §4, `helpers.md` §6.2).
- *Closed.* The number of slots (§4.4) is `OMNI_LAYOUT_SLOT_COUNT = 24`, the
  whole Greek alphabet, user layouts only, named in the same namespace but not
  including them.
- *Closed.* The client-rule record: `OMNI_TAG_CLIENT_RULE` at `0x33`, a 24-byte
  header plus the value its effect applies, with `scope` separating
  map-once from re-resolve-on-reload.
- *Closed.* Whether `save` gains a scope (§4.6). It takes an optional key-path
  pattern and defaults to the whole block, so the authoring workflow's extraction
  step is `save omniwm.layouts.delta.*` for a slot named delta. The exclusion test
  is unchanged in a narrowed scope and is the same one soft reset uses, so the
  three operations cannot disagree about what is configuration.
- *Closed.* The degradation mechanism (§3.2). Weighted least squares, nothing is
  dropped, so the iteration bound, the drop order, the terminal arrangement, the
  drop notification and the sticky-versus-per-pass question are all artefacts of
  a drop-based design and are retired rather than answered.
- *Closed.* Whether magnetisation is a soft constraint or a post-solve relaxation
  (§7.5). A relaxation, applied at step 5 of the pipeline in §5 and before the
  write, because only a relaxation is expressible outside the constraint
  language. This is what forced the pipeline to grow a relaxation step, and §3.1's
mapping phase later made it eight rather than seven.
- *Closed.* Whether the solver's output is in canvas or monitor coordinates
  (§7.5). Canvas, with the viewport as the transform from canvas space to an
  output. §7.5 settles this and §11 was contradicting it.
- *Closed.* Whether client-set membership is a first-class object the engine can
  query (§7.7). It is, and §8's table assigns its ownership to `windows.md` and
  `tags.md` rather than to this document, so the stored-versus-derived fork is
  not this document's to close. It belongs in a `tags.md` open item, and §3.2's
  determinism requirement now constrains whatever that answer is.
- The priority bands are ordinal and the values inside a band are not (§3.2).
  `omni_layout.h` separates them by a decade so a band dominates the compromise,
  which makes `OMNI_CONSTRAINT_PRIORITY_DOMINANT` a weight and not a guarantee.
  Nothing is hard, `generaldesign.md` §7's "soft and prioritised" is right, and
  the constant is now named `DOMINANT` rather than `REQUIRED` so that it cannot
  be read as a promise.
- Whether an *arranged* layout is written alongside the solved one (§3.7). The
  solved half is settled by `OMNI_SECTION_SOLVED_LAYOUT`, so a script can already
  read a layout; what is left is whether the block also carries the execution of
  one, which decides whether an external animator needs to be told where windows
  are going or can work it out. §5's step 6 sharpens this, because the value
  written is the solver's output after relaxation rather than the solver's output
  alone, and an external animator has to be told which of the two it is reading.
- *Closed.* Whether a constraint record is per client, per tag, or per
  client-per-tag (§3.1). The question dissolved: a layout program names no
  window, so there is no record to scope. Mango was the answer all along, since a
  Mango layout is a compiled algorithm over the client list and each client holds
  its own state, so the design follows it rather than departing from it. The
  earlier draft had the record be more expressive than any consulted project,
  which was a cost being mistaken for a capability: a tagged subject expressed
  per-tag and per-client-with-selector, and both of those are now expressible
  without it, because a window's deviation is its own override and a tag
  contributes only its clients. `generaldesign.md` §8's window rules seed those
  overrides, which the document should say rather than leave
  a config author to discover.
- *Closed.* Tag identity (§3.5). A tag is a catalog entry and is named by
  `entry_ref`, which is the shape a client already has, and per-tag layout
  selection is a value under the tag. `tags.md` §2 and §4 own both. The
  membership children need a subtree delete and a generation-correct delete,
  which `tags.md` §7 records as store requirements that do not exist yet, and
  that is now the only way tags block the store rather than the other way round.
- Client-set membership is now answerable and `tags.md` has answered half of it.
  Tag membership is stored on the tag as one `WINDOW_DEPENDENT` child per member
  (`tags.md` §5), chosen over derivation on access-pattern grounds: the solver
  needs the union of a monitor's displayed tags on every solve, and that must not
  be a scan of every client. The remaining half is client-side and belongs to
  `windows.md`, and §3.2's determinism requirement is what forces the answer,
  because a `HashSet`-shaped derivation is a non-reproducible iteration order.
- The pan and zoom space is confirmed twice, by driftwm and newm (§7.5), and it
  is a transform above the solver rather than a constraint inside it. A third
  confirmation was claimed from halley and is retracted; §7.5 says why.
- Whether a built-in layout is permitted to be something other than a constraint
  program (§3.4). Decided: a built-in is compiled in, and the test is that adding
  one means writing the layout, registering it, and recompiling, with no new
  solver mechanism. `generaldesign.md` §7 has been amended to say exactly that.
  The old claim in that document, that a built-in is a preset constraint program
  and that adding one is a config change, was already false before this
  decision, because §7.1 makes a stack a group and a group is a nesting
  concept. The correct citations for the two mechanisms that do not exist yet are
  §7.1 and §7.2, not §7.1 and §7.4: §7.4 says grid and tile are the same
  constraints with a different leftover-space policy.
- What happens when a second update arrives mid-animation (§3.7). The pipeline
  in §5 describes one update and is silent about overlap.
- Whether the engine is a component or a borrowed service. `generaldesign.md` §5
  says every user-facing capability is a component and `generaldesign.md` §7 says
  the engine is a general service, so this was not unstated, it was stated twice
  inconsistently. The live question is narrower: whether "user-facing capability"
  covers an engine no user configures, which decides whether it appears in the
  `helpers.md` registration table with a band and an `enable_key`, or is wired
  into `core/server.c` like the store. §3.7 needs it because a wrong band is an
  init failure rather than a detected violation.
- The built-in programs need somewhere to live. `devnotes/layoutsystem.md` was
  scaffolding for exactly this and was removed once this document took its
  place; recreating it to hold the compiled-in programs is the right call, and it
  is a new document rather than a correction to this one. The boundary is now
  decided and the document has to be written to it. A built-in is compiled into
  the compositor, and the requirement on it is that it is written over mechanisms
  the engine already has: a layout, its registration, and a recompile, with no
  new solver mechanism and no new tag. Most of them are a value of the same
  `0x32` tag a user slot holds, differing only in that they are compiled in
  rather than read from a key. The ones that are not are the two that forced the
  decision: a stack is a group plus rules plus a one-axis stack (§7.1), and a
  scroller is a viewport (§7.2), and both of those are a nest and a transform
  that already exist rather than per-layout code. So the test the document should
  hold itself to is that a new built-in never adds a mechanism, only a
  declaration. If one does, the mechanism goes to §7 first.

### 11.1 Edits this document implies in documents it does not own

Recorded here rather than applied item by item, because each of these is a change
to a document with a different owner and none of them is a design question. They
are consequences of §2.10's first item and §4.6, and they are listed so that
minting the tag was a checklist rather than a surprise. All of the block-format
rows are now done; the ones that are not are the `save` scope and the new
built-ins document.

| document | section | edit | state |
|---|---|---|---|
| `include/shared/omni_layout.h` | §7 tag constants | the new tag constants, and `OMNI_TAG_MAX_KNOWN` moves | done, 0x32 and 0x33 |
| `configstorage.md` | §4 type tags | rows for the new tags, framed, with their payload shapes | done |
| `configstorage.md` | §12 guards | a semantic guard per record, and a key-path rule that an `omniwm.layouts.<name>` component is checked against the closed set | partial, the tag-range guard is done, the record guards are not |
| `ipc.md` | §3 type encodings | a wire encoding, which has to be the same shape the file encoding uses | done |
| `tomlparser.md` | §2 | the TOML binding for the same encoding | done |
| `configstorage.md` | §13 | `save` gains a scope, at least a single catalog subtree | done, an optional key-path pattern, defaulting to the whole block |
| `helpers.md` | §6 | a sentence saying the layout-name table is a compile-time constant and deliberately not a registry | done, §6.2 |
| `generaldesign.md` | §7 | the degradation wording, which said the solver drops the lowest-priority constraint | done, now weighted minimisation |
| `generaldesign.md` | §7 | the built-in wording, which said a built-in is a preset constraint program and adding one is a config change | done, now compiled in with no new solver mechanism |
| `devnotes/layoutsystem.md` | new | the compiled-in built-in programs; removed as scaffolding, to be recreated | open |
| `include/shared/omni_layout.h` | §7 constraint constants | rename `OMNI_CONSTRAINT_PRIORITY_REQUIRED`, which is a weight and not a guarantee | done, now `OMNI_CONSTRAINT_PRIORITY_DOMINANT`, with the header comment stating that no band is hard |
