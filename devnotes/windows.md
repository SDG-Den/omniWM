# Windows (devnotes/windows.md)

The client layer: what a client is, how it comes and goes, what it owns, how it
is focused and stacked, how it floats, and how the window rules that reach it
compose. `generaldesign.md` §8 assigns this layer the client lifecycle, the
floating layer, focus and stacking policy, and the per-window identity that
decoration overrides key on, and `missing-devnotes-topics.md` widens it with
groups as nested layouts, clusters as movement relationships, window rules and
their precedence, and compositor-drawn fake clients as layout participants.

**This is a prototype, not a specification.** It is a scaffold built to
`d5a0e1e` of `mango-dev` so that the parts of this layer which have to agree
with the store, with `tags.md`, and with the solver have one written shape
rather than four implied ones. Nothing here is frozen, and §13 lists what it
deliberately did not decide. Where it disagrees with a document that owns the
subject, that document wins and the disagreement is recorded as an open item
rather than resolved here.

## 1. How each answer in this document was chosen

Three rules, applied in order, and the order matters because two of the three
sources can both be present.

1. **If an existing document defines it, that is the answer.** `generaldesign.md`,
   `configstorage.md`, `tags.md` and `layoutengine.md` are the authority. Mango is
   not consulted for anything they have already said, and a difference is recorded
   as a discarded alternative rather than adopted.
2. **Otherwise Mango is the guiding shape.** `mango-dev` at `d5a0e1e`, branch
   `config-scriptability`, which is the revision `research/mango.md` indexes. A
   working implementation of a window manager is worth more than an argument about
   what a window manager should do, and where Mango has done something the
   existing documents are silent about, the shape of what it did is adopted and
   the commit is cited so the claim can be checked.
3. **Otherwise existing logic guides the shape.** Where the subject is ours and
   Mango has no equivalent at all, the shape is derived from whatever the rest of
   the design already does, and §13 says which derivation.

Two consequences of the ordering are worth stating before the sections, because
both are places where the sources genuinely disagree.

- **A client identity is stronger here than in Mango, and the existing documents
  are why.** Mango mints a `uint32_t` per client and never invalidates it
  (`generate_client_id`, `struct Client.id`). `configstorage.md` §0.1 makes a
  catalog reference `(epoch, entry_id, entry_generation)` and requires an exact
  match to destroy, so a client is identified the way every other catalog entity
  is identified. Mango can get away with a bare integer because it resolves
  staleness by filtering at read time on a flag; §5 shows we cannot, because our
  scratchpad is a tag and therefore has a set to delete from.
- **A window rule matches a window, and how is now settled in §9.** This was the
  one place where `generaldesign.md` §8 and §11 wanted a mechanism the ABI had no
  home for, because the `client_rule` record at `0x33` carries the language's own
  keys and no matchers. §9 settles the shape as a block-structured if/then whose
  filter vocabulary is any observable client field, and it settles where the rule
  lives, which forces a consequence worth reading §9 for rather than skipping:
  a rule in the block can only match on values the block holds, so appid and title
  become client fields.

## 2. The client is a catalog entry, and its identity is an `entry_ref`

A client is a catalog entry, like a tag, and it is identified by `entry_ref`:
`(epoch, entry_id, entry_generation)` (`configstorage.md` §0.1, §4). Everything
else about a client follows from that one decision.

- **Per-client values are children of that entry**, which is `tags.md` §5's rule
  for a tag's parts applied to a client, and the `layoutengine.md` §4.3 rule that
  a name is a value applies to each of them.
- **The per-client values are `WINDOW_DEPENDENT`**, so `configstorage.md` §8
  excludes them from `save` and from a soft reset. `tags.md` §6 says why that is
  correct rather than merely tolerable: a window rule is ordinary saved
  configuration and re-applies to the new entry on restart, so a preference is
  never saved twice.
- **The generation is what makes a reference checkable.** `entry_id` is the slot
  index and `entry_generation` changes when the slot is reused, so a reference
  held across a teardown and a reallocation is detectably stale rather than
  silently pointing at a different window. `configstorage.md` §4 requires an
  exact match to destroy, and the solver's output is diffed by
  `(entry_id, entry_generation)` between two solves (`layoutengine.md` §2.10), so
  the same identity serves the store's lifetime and the animator's per-node diff.

What a client owns, as values on that entry. The third column is the split §9.3
forces: a window rule may read any of these but may write only the settable ones,
because a rule must not be able to overwrite the identity of the client it is
matching.

| value | settable | why it is here |
|---|---|---|
| `kind` | no | whether this is a real surface or a compositor-drawn one, §10 |
| `appid` | no | a protocol fact, mirrored so a rule in the block can filter on it, §9.3 |
| `title` | no | the same, refreshed on a title change |
| `pid` | no | a protocol fact, and observable so a rule can filter on it |
| `mapped` | no | derived from the surface, and observable |
| `floating` | yes | `tags.md` §6, a fact about the client and not about a place, §6 |
| `visible` | yes | the per-client visibility state `tags.md` §8 needs for a minimized client, §5 |
| `focus` | no | whether this client is focused on its monitor, §7, and set by focus rather than seeded |
| `cluster` | no | the container this client is inside, or none, §8, and a structural fact |
| `layer` | no | the stacking layer the client is raised into, §7, and derived from `floating` and `focus` |
| `border`, `radius`, `opacity`, `blur` and the rest | yes | `decorate.md`'s values; named here to mark whose they are, and seeded by `generaldesign.md` §11's three routes |
| effective `tags` | no | derived from membership and never stored here, `tags.md` §5, §9.3 |
| the named sets it is bound to | yes | one child per set, §9.5, and a rule's effect rather than a value |
| the last solved `rect` | no | the previous geometry, so an animation has somewhere to start from, §11 |

The last row is the one that is easy to get wrong. A client does not store where
it is; the solved layout section stores where it is going, and the live value
belongs to the scene graph. What the client keeps is the *previous* solved rect,
which is the same thing Mango does with its per-client `old_*` fields
(`old_stack_proportion`, `old_ismaster`, `old_grid_col_per`, and
`overview_backup_geom` on `struct Client`) and the reason §11 needs two phases.

The `appid` and `title` rows are the newest and the least obvious, and §9.3 gives
the reason they exist rather than assuming it. They are read-only for the same
reason `focus` and `layer` are: they are facts about a window rather than
preferences about one, and a client that could have its own appid rewritten by a
rule would make every later filter a filter on invented data.

## 3. The client lifecycle

Mango is the guiding shape here, because no existing document describes the
transitions. Its client lifecycle is a `wl_listener` per event on the surface
(`include/mango/manage/client.h`): a new toplevel is constructed, properties are
initialised, then map, commit, unmap and destroy. The two transitions worth
naming are the ones with a consequence for us.

- **Construction initialises every per-client value** in one place
  (`init_client_properties`, which is also where `iskilling` is cleared), so a
  client never carries a field the compositor did not write. That is the same
  property `configstorage.md` wants from a fresh catalog entry and it is worth
  copying exactly: initialise from the declared defaults rather than from whatever
  the previous occupant of the slot left behind.
- **Unmap is where a client stops being an input.** `handle_client_unmap` sets the
  kill flag as its first action, before removing the client from the switcher and
  the scroller bookkeeping, and every scope predicate in
  `include/mango/manage/client.h` (`ISTILED`, `ISNORMAL`, `ISFAKETILED`,
  `ISSCROLLTILED`, `VISIBLEON`, `TAGMATCH`) tests it. `pending_kill_client` does
  not set the flag; it only sends the close and refuses to send it twice. So the
  flag means gone, not going, and nothing in `arrange` has to abort or unwind.

Prototype lifecycle states, and what exists in each:

| state | the client exists as | its tag membership | in the solve input |
|---|---|---|---|
| constructed | a catalog entry with initialised values | no children yet | no |
| mapped, visible | a live entry | one child per tag it is on | yes |
| mapped, minimized | a live entry | children unchanged | no, §5 |
| mapped, floating | a live entry | children unchanged | no, §6 |
| unmap pending | a live entry, excluded | children still present for the exit animation, §11 | no |
| destroyed | gone | gone, by subtree delete | no |

The middle column is the part worth arguing about, and Mango is the reason it is
written this way. Mango's client carries three lists (`link`, `flink` and
`fadeout_link`) and the third exists only so that a client being faded out stays
reachable while its animation runs. That is direct evidence that a dying client
cannot have its membership removed at unmap time, which is why the prototype
defers the membership delete to the end of the exit animation rather than doing
it at the flag.

## 4. Visibility, scope, and the client set the solver is handed

A window's visibility is whether it is in any tag the monitor displays
(`generaldesign.md` §6), and a monitor's view is a set of tags rather than a
single index, so visibility is a question about a set union rather than a
comparison. `tags.md` §5 owns the storage and this section only records what the
client layer hands over.

- **Membership is stored on the tag**, as one `WINDOW_DEPENDENT` child per member
  holding the member's `entry_ref`, and a group or cluster is a member in its own
  right whose contents are not members of the tag at all. That is `tags.md` §5 and
  it is not revisited here. The client layer's obligation is the reverse
  direction: a client inside a container resolves its tags by walking to the
  container, which `tags.md` §5 puts behind a switcher and therefore off the
  solve path.
- **The straddle case is refused rather than represented.** A client that is
  inside a group and also carries its own membership entries is a state the store
  cannot hold, and `tags.md` §5 is explicit that refusing it is the point, because
  it is what stops a window being placed by one layout while counted by another.
  The client layer must therefore never write both.
- **The solver's input is a walk, so the walk has an order.** `layoutengine.md`
  §3.2 requires a reproducible iteration order and rules out a `HashSet`-shaped
  derivation, and Mango cannot supply one: its membership is a `wl_list`, so its
  order is the history of the insertions and two monitors that reached the same
  set by different routes would walk it differently. The prototype therefore takes
  the store's order, children by ascending `entry_id`, which is a slot index and
  therefore identical on every machine and after every restart. This is a
  prototype choice under rule 3 and it is cheap, since the hot direction is
  already only the tags a monitor actually displays.
- **A membership change is an ordinary commit.** It is a child entry set or
  deleted, and `configstorage.md` §1 orders commits by `commit_id` and gives
  readers a coherent snapshot by double-reading `commit_state` and `commit_id`.
  So a membership change is visible to a solve either wholly or not at all, which
  is the property `layoutengine.md` §3.2 wants, and there is nothing extra to
  build. Whether a membership change *causes* a pass is `layoutengine.md` §3.7's
  open question and is not answered here; this document's obligation is only to
  make the mutation a single commit so that question has one answer to reason
  about.

## 5. A minimized client is a member of its tag and in no visible scope

`tags.md` §8 and `layoutengine.md` §7.7 already decide this, so this section
records the decision and the one thing about it that is easy to get wrong.

A minimized client stays a member of the tag it came from, so it still appears in
overview and in alt-tab style switchers and on bars and docks, and a per-client
visibility state keeps it out of every visible scope so no layout places it. The
two facts are held simultaneously and neither implies the other. Two things
follow that the rest of the design leans on.

- **Overview and a switcher read a client set, not a solved layout.**
  `generaldesign.md` §6 makes overview a tag whose contents are the union of the
  others, so it needs the membership walk and not the solver's output, and a
  switcher that can show a minimized client does not wait on a solve.
- **Focus is an ejection, and it is two writes.** Focusing a minimized client
  kicks it out of the scratchpad, which `tags.md` §8 defines as deleting the
  membership child for that client from the scratchpad tag. The visibility state
  has to clear as well, or the window would stay invisible on its ordinary tag
  after leaving the scratchpad.

That second write is where our design and Mango's part company, and it is worth
being precise about because the obvious reading of Mango is wrong. The hidden or
shown state *is* a per-client flag there: `SCRATCHPAD_SHOWN` is
`is_in_scratchpad && !isminimized`, and every scope predicate in
`include/mango/manage/client.h` filters on `isminimized`, so a minimized client
drops out of every scope by being read as absent rather than by being removed.
But the scratchpad is not only a flag. `show_scratchpad` in `src/manage/client.c`
**reassigns the client's tags**, saving the mask it had in `oldtags` so it can be
put back, reinserts the client at the head of the client list, and forces the
client floating at a scratchpad-sized geometry. `client_park` is stronger still:
it unlinks the client from the client list and the focus list and clears its
monitor, and `client_unpark` puts it back beside an anchor. So Mango mutates
membership on both the show and the hide, in a bitmask and a linked list, and it
keeps a copy of where the client was to reverse it.

That is the same shape as ours, and the difference is the representation rather
than the mechanism. Our membership is a `WINDOW_DEPENDENT` child of the tag
holding an `entry_ref`, so the mutation is a store commit that is ordered by
`commit_id`, read coherently by a solve, checked by generation on the way out,
and writable by a script. Mango's is an assignment to a `uint32_t` and a
`wl_list` splice, which is cheaper and disappears with the process. The thing
`generaldesign.md` §6 buys by making the scratchpad a tag rather than a flag is
that it can be moved to another monitor and carry its clients, and Mango has to
reimplement that by hand in `switch_scratchpad_client_state`, which rescales the
client's `float_geom` by the ratio of the two monitors' dimensions and rewrites
`oldmonname`. That is a special case our design does not have, and it is the
clearest argument in this document for the tag.

## 6. The floating layer

`tags.md` §6 settles the storage and this section settles the interaction.
Floating is a per-client `WINDOW_DEPENDENT` value, it travels with the client to
every tag that client is on, and it is **not a scope the solver arranges**.

The consequence is the one the solver depends on: a floating client is excluded
from every layout program, so it is not in the solve's input set at all, and the
floating layer places it instead. Mango is the guiding shape for how the two
decisions are tied together, and it ties them with one boolean. `isfloating` sits
on `struct Client`, `ISTILED` in `include/mango/manage/client.h` excludes it, and
`client_target_layer` in `src/manage/client.c` returns `LyrFloat` rather than
`LyrTile` for the same flag. One value decides both that the solver never sees
the client and which layer draws it, and the prototype keeps that, because the two
can then never disagree.

- **A client that floats floats everywhere it appears**, and cannot float on one
  tag and tile on another. `tags.md` §6 rejects the alternative, and Mango is the
  project that has it: `check_match_tag_floating_rule` in `src/manage/client.c`
  makes floating a per-tag rule. Having seen it implemented is worth recording,
  because the feature looks harmless and it is the exact thing that forces a
  per-client-per-tag constraint record, which `layoutengine.md` §3.1 dissolved.
- **Geometry on a floating client is not layout geometry.** Mango's window rules
  do carry geometry, `width`, `height`, `offsetx` and `offsety` into `float_geom`
  on the client, and it is safe there for the same reason the layer is: a
  floating client is not in the solve, so a size on it never reaches the solver.
  If the override representation in §9 ever grows geometry, this is the precedent
  for where it may go.
- **A new client has a floating default, and it is derived once.** `tags.md` §6
  says a window rule seeds the value, which leaves the moment before any rule has
  matched unanswered, and Mango answers it inside the same pass that applies
  rules: `client_apply_rules` sets floating from `client_is_float_type(c) || parent`
  before it walks the rules, so a rule can override the default but the default
  is already true. `client_is_float_type` asks the protocol, the XDG toplevel's
  own state and then the window type, and `parent` is the XDG or X11 transient
  parent from `client_get_parent`, so a dialog floats because it has a parent
  rather than because a rule said so. The prototype takes the shape and not the
  predicate, because the predicate is protocol detail: a client is constructed
  with a defensible default, and a rule is an override of that default rather than
  the thing that makes the value exist. It is also the only place in this document
  where a per-client value is seeded by something other than the three routes in
  §9, and it is worth naming as a fourth so the three are not mistaken for the
  whole set.

## 7. Focus and stacking

No existing document defines a focus policy, so Mango is the guiding shape
throughout, and it is a good one because it separates three things our design
tends to blur.

- **One focus pointer per monitor, and a remembered predecessor.** Mango keeps
  `sel` and `prevsel` on the monitor struct. The predecessor is what makes "send
  it back" and a focus-aware layout possible without a search, and it is
  destroyed rather than recomputed on every focus change.
- **Focus recency is a separate list from stacking order.** Focusing a client
  moves it to the head of a global `server.focus_stack` through the client's
  `flink`, with the comment in `src/layout/arrange.c` spelling out the reason: the
  focus order is remembered for any tree shape, so it survives a layout that
  reorganises the scene. This is the single most transferable idea in Mango's
  window code, because it means "next" and "previous" are answered by a list the
  solver is allowed to reorder the scene around without invalidating.
- **Raising is a parameter of focusing, not a side effect of it.** `client_focus`
  takes a `lift` argument and only calls `client_raise_group` when it is set. So
  focus-to-front is a decision a caller makes, focus-without-raise is available,
  and a focus policy that never raises cannot accidentally destroy a layout's
  intended z-order.
- **Raising moves a whole group together.** `client_raise_group` raises every
  member, which is the same rule the client layer needs for a cluster, and it is
  why a cluster that travels as one unit also stacks as one unit rather than
  being interleaved by focus.

The refusals are worth carrying over verbatim, because each is a case where
focusing is meaningless rather than merely undesirable: a locked session, a client
that is killing, a client that is not mapped, and a client marked `nofocus`.
Focusing a client on another monitor also selects that monitor
(`set_selected_monitor`), so focus and the active monitor are one operation rather
than two that can disagree.

One coupling is deliberately left open. The focused client is an *input to
arrangement*, since a stack or a monocle shows the focused member, and Mango
re-arranges on a focus change but only conditionally, and only when the layout
wants it: the check in `client_focus` requires both the previous and the new
client to match the monitor's tags, neither to be floating, and the layout to be
the scroller or the monocle. That condition is a layout's business, not the
window layer's, so this document records the dependency and leaves the trigger
with `layoutengine.md` §3.7.

## 8. Groups and clusters, as the client layer sees them

`generaldesign.md` §8 owns both concepts and this section covers only what the
client layer holds. A group is a nested layout with a titlebar; a cluster is a
movement relationship that adds no chrome; a cluster occupies one space of its
parent and is resolved to a single virtual client before the program places
anything, and its members are then placed inside the space that occupant was given
by the client rules on the clients.

What the client layer therefore holds is small and mostly about identity:

- **A container is a catalog entry too**, so a group is diffable across two solves
  by the same `(entry_id, entry_generation)` pair as any other node, and its
  member list is a set of `entry_ref`s on its own entry rather than a field on
  each member. A client's `cluster` value names the container it is inside, and
  the straddle rule in §4 is what keeps that the only way to be inside something.
- **Membership of a container is not tag membership.** A group on three tags has
  three tag entries pointing at the group, and the windows inside it have none
  (`tags.md` §5). The client layer must not add any.
- **A group's titlebar is a fake client**, not a decoration, and §10 covers why
  that is a different thing from what Mango does.

**Clusters do not exist in Mango, and its groups are not our groups.** This is
worth stating plainly because `layoutengine.md` §3.2 currently attributes the
cluster arrangement to Mango and is wrong to. What Mango has is a flat linked
list of clients, `group_prev` and `group_next` on `struct Client`, where a client
is in a group exactly when either link is set, and it exports that test as
`is_group` over IPC. Its `ismaster`, `isleftstack` and the way
`pre_calculate_before_arrange` walks members show what it is for: it is a
**stack**, a set of windows bound together with one of them showing and focus
moving within the chain, which is what `layoutengine.md` §7.1 means when it says a
stack is a group. A cluster, in the sense `generaldesign.md` §8 defines, is
several windows that occupy one space of a parent layout, and nothing in Mango
expresses that. The flat links are the arrangement `generaldesign.md` §8 already
names and rejects when it says that treating the two as one concept cannot express
both intents.

Mango's group bar is the second half of the correction, and it is the opposite of
ours. `mango_group_bar_create` in `src/draw/text-node.c` returns a `MangoGroupBar`
scene node that is assigned to `c->group_bar` and parented to a layer, the
`GroupBar` constant is a node kind rather than a client type, and
`src/layout/scroll.c` computes the bar's height by branching on whether the root
client has a group. So Mango's bar is chrome the *layout* measures and draws,
attached to one member. Ours is a participant the group's own program places
(`generaldesign.md` §8, §13), which is the difference between a titlebar that a
layout has to know about and one that is just another thing in the group.

## 9. Window rules: an if/then seed, evaluated once per client

A window rule is a **declarative seed applied once, when a client is created**.
It is not a constraint, not a subscription, and not a source of truth: it writes
initial values and is then finished, and the block carries no record that it
fired. Everything below follows from that one sentence, with the one exception
§9.6 carves out.

### 9.1 The three tiers, and why the last one wins for free

A client's values come from three places, in this order:

| tier | written by | when |
|---|---|---|
| global defaults | ordinary configuration in the block | compositor start and config reload |
| window rule seeds | a matching rule's `then` block | once, at client creation |
| runtime writes | an IPC command, or a program writing the block | any time after creation |

Nothing has to enforce that order, and that is the point of making the middle tier
one-shot. A rule is not re-evaluated on its own, so every write that follows a seed
is necessarily the most recent one, so a runtime write beats a rule seed without a
rule saying so. `configstorage.md` §1's last-writer-wins ordering therefore only
has to break ties *within* the rule list, which is the narrow job §9.4 describes.

That guarantee has one hole and it is deliberate, so it is worth naming rather than
letting a reader find it. **A re-apply (§9.6) is a new seed, and it overwrites a
runtime write made since the last one.** Someone who runs a re-apply after a script
has customised a client gets the rules' values back, not the script's, and the
alternative would be to have no re-apply at all. What the guarantee buys is the
ordinary case: a script's write survives on its own, with nothing polling to undo
it.

The lifecycle consequence is in `tags.md` §6's general form: a client keeps what
it was given until it is destroyed. A client that unmaps and maps again is the
same catalog entry with the same values, so **a remap does not re-apply** and a
value a script wrote while the client was unmapped is not clobbered on the way
back. Mango applies rules twice on arrival, at the initial commit and again at
map, and for us those two collapse into one creation-time evaluation, because the
entry does not exist before then and appid and title are both readable by then.

### 9.2 Both sides are blocks, and the filter is any observable field

```
[[omniwm.window_rules.firefox_floats]]
if.appid    = "^foot$"
if.title    = ".*Picture-in-Picture.*"
then.floating = true
then.border   = { width = 2, color = "#88c0d0" }
```

The if/then split is not decoration, it is what makes the mechanism unambiguous.
Mango's record is `{match: {key, type}, replace: {type, value}}`, which is
unambiguous only because its filter is restricted to catalog keys and a then-side
can never be one. Opening the filter to any client field, as this design does,
destroys that separation: `title` on the left could otherwise mean "match the
title" or "set the title", and no positional convention distinguishes them. Naming
the two sides is the minimum structure that does, and blocks on both sides are
what let a rule carry a real predicate and a real set of assignments.

Three decisions inside that shape, and the third is ours rather than Mango's.

- **The filter vocabulary is any observable client field, not a fixed set.**
  Mango hardcodes two, appid and title. There is no reason to stop at two when a
  client already exposes a kind, a pid, a mapped state and every decoration value,
  and hardcoding would mean every new field is an ABI change. `generaldesign.md`
  §8 requires a rule that matches a window and nothing narrower.
- **The operator is a regex, unanchored, and it is the only operator.** Every `if`
  value is a pattern, whatever the field's type, so there is one thing to learn and
  one thing to implement. `is_window_rule_matches` calls `regex_match` on both of
  Mango's fields, and `regex_match` in `src/common/util.c` is PCRE2 with no anchored
  flag and a zero start offset, returning true for a match anywhere in the subject.
  Exactness is therefore the author's job, written `^...$`, which is what
  `parse_config.c` does with `"^bind[s|l|r|p|c]*$"`. Equality is not a primitive
  here, and title matching almost never wants one.
- **It is PCRE2, so a numeric comparison is expressible and no second operator is
  needed for one.** A bounded range is ordinary alternation under anchors: a width
  of at most two digits is `^\d{1,2}$`, a value in `0..63` is
  `^(?:[0-9]|[1-5][0-9]|6[0-3])$`, and a value below 10 is `^[0-9]$` given the
  field is a decimal string. What makes those writable at all is that PCRE2 has
  alternation, character classes and lookahead, so "extended regex" in the sense of
  more than a basic POSIX pattern is already what is being used; there is no weaker
  dialect to opt out of. The cost is that such a pattern is unpleasant to write,
  and the honest answer to that is a comparison an author can read, not a different
  operator in the value. A `width < 63` form would need its comparison recorded
  somewhere, either as a mode on the value or as a rewrite of the pattern on the way
  in, and either is a second way to say one thing, which is the same objection §9.7
  raises against a priority field. If the patterns turn out to be a real burden, the
  sugar can be added later as a *defined* expansion to one of these regexes, so the
  stored value and the filter vocabulary stay single.
- **Compiling per evaluation is affordable here, and only because rules are
  one-shot.** Mango's `regex_match` calls `pcre2_compile` on every call, which
  would be indefensible in a per-solve path. A rule here is evaluated once per
  client at creation and only again if someone asks for a re-apply (§9.6), so the
  cost is paid once per client rather than once per pass. A cache is not needed to
  make this acceptable, and adding one before it is needed would be speculative.

- **A rule with no `if` is ignored, not treated as a match-all.** Mango's absent
  clauses are wildcards, and read literally an empty filter would match every
  client. Here it is skipped, because a match-all rule would compete with the
  global defaults rather than supplement them, and a seed that fights the
  configuration is worse than no seed. The wildcard property survives only inside
  a non-empty `if`, where a clause that is simply absent does not constrain the
  match. A rule is therefore valid only with both blocks non-empty, and one that
  is not is inert rather than an error, so a half-written rule cannot stop a
  client from being created.

### 9.3 The rule lives in the block, and that forces appid and title into it

Rules are **block values**, held as a referenced group at
`omniwm.window_rules.<name>`, readable and writable through the same route as
everything else. TOML is not a second source of truth: `tomlparser.md` §6 already
parses a config file into a private staging area and commits it, so the file is an
input format and the block is the state, and a window rule is simply one more
thing in it. This is what makes a rule inspectable and editable by a program
without a privileged path, which is the same property `generaldesign.md` §11 gets
from the block.

The consequence is worth stating rather than discovering later. **A rule in the
block can only match on values the block holds, and appid and title are not in the
block**, because they come off the surface. So they become client fields:

- `appid` and `title` are `WINDOW_DEPENDENT` children of the client's entry,
  written once at creation and refreshed on a title change. Mango's `set_title`
  listener is the precedent for the refresh, and the fact that Mango needs it too
  is the argument that mirroring is not a distortion of the value.
- **An unset one is the empty string, not an absent child.** A surface that never
  carried an appid, a layer-shell surface, an X11 window with no class: the field
  exists and holds `""`. This is what makes a filter over it well defined rather
  than undefined, and it is worth stating because the alternative, an absent child,
  would give every rule a second question it has to answer, namely what a filter
  means when the subject is not there. With the empty string the answer falls out
  of the regex: `^foot$` does not match a client that never set one, and `^$`
  matches exactly those. `title` takes the same rule, an untitled window being an
  empty title rather than a missing one, so the two never disagree about what
  absent means.
- They are facts about one window, carried by that window, which is exactly
  `tags.md` §6's general form, so this is an instance of a rule the document
  already states rather than a new mechanism.
- It is also what makes the detection half of the equivalence in `ipc.md` real. A
  script that wants to do by script what a rule does by hand has to be able to
  *read* appid and title, and the block is the only route all three surfaces share.
  Had the rules stayed outside the block, that capability would not exist and
  §9.1's justification would not hold.

**The field vocabulary is therefore split in two**, and the split is forced rather
than chosen. The **observable** fields are everything a client exposes, and they
are the whole `if` vocabulary. The **settable** fields are the preferences only,
and they are the whole `then` vocabulary. Protocol facts and solver outputs are
readable but not seedable, because a window rule must not be able to overwrite the
identity of the client it is matching: if a rule could write `appid`, then the
value a later rule filters on would be a value an earlier rule invented. The
observable set includes `appid`, `title`, `pid`, `kind`, `mapped`, every
decoration value and every value a previous rule seeded. The settable set is
`floating`, the decoration values, the geometry pins, the tag assignment of §9.4,
and the set bindings in §9.5. **`tags` is in neither half**, because a client has no
tag field to set or store: it is exposed read-only as a derived value, and §9.3
records the asymmetry that creates.

**Tags are not a client field at all, in either direction.** `tags.md` §5 stores
membership centrally, one member child per client on the tag's own entry, and that
is the only place it lives. A client has no tag field, so `tags` is neither a
settable client parameter nor a stored one, and the split above needs no exception
for it. What a rule and a filter see is a **derived read-only value**, exposed the
same way `appid` and `title` are in §9.3: computed from membership, never written,
and recomputed rather than maintained.

- **A filter reads the effective set.** `if.tags` sees what `tags.md` §5's reverse
  question returns, which is the container's tags for a nested client and the
  client's own member entries otherwise. So `if.tags` on a grouped client matches
  the group's tags, which is the answer a user would give if asked, and a rule
  cannot filter on an intention because there is no intention stored to read.
- **A seed writes membership, not a field.** `then.tags` is the one `then` value
  that is not a write to the client's entry: it adds a member child to each named
  tag's entry, so it lands in the same grouped commit as the rule's other writes
  (`configstorage.md` §3) and it is the only seed that asserts something about the
  solve. This is the "this program lives on that workspace" case, and it is why the
  field exists despite membership being central.
- **A seed landing on a grouped client is inert, and §9.4's answer still holds.**
  `tags.md` §5 refuses to represent a client with member entries while inside a
  container, since that is one client arranged by one layout and counted by another.
  Writing membership directly would produce exactly that state, so the write is
  deferred rather than applied to the container and rather than refused, and it
  takes effect when the client leaves. The straddling state stays refused because a
  deferred write produces no member entries.
- **The deferral needs somewhere to live, and it is the one thing here I inferred
  rather than read off a decision.** A client has no tag field, so a pending set
  cannot be one. The options are to drop a seed that lands on a grouped client,
  which loses the case §9.4 was written to keep, or to give the pending set an entry
  of its own keyed by the client's `entry_ref` and marked `WINDOW_DEPENDENT`, which
  keeps "the client has no tag field" true, is deleted by §11's teardown for free,
  and is read by nothing except the moment the client leaves a container. The second
  is what the text below assumes, and it is the piece to object to if it is wrong.

That split has a requirement attached to it, and it is a new obligation on
`ipc.md` rather than on this document: **the rule filter vocabulary and the
IPC-readable client field set must be the same set.** `ipc.md` already requires
that IPC carry the same capability as direct block access, so this follows from
that mandate instead of competing with it, and the check is simply that a field
which can be filtered on can also be read over the socket.

### 9.4 Applying: two phases, in that order

Rules are applied in two phases, and the order is the whole point.

1. **Evaluate every `if` against the state as it was before any rule ran.** No
   `then` has been applied yet, so no filter can observe another rule's write.
2. **Apply the `then` blocks in array order**, so a later rule wins per field.

The phase split is necessary because of §9.2. The two sides of a rule now draw from
the same vocabulary, so a single sequential pass would let a rule see a value an
earlier rule had just written, which makes `if floating == false then floating =
true` a well-formed rule and makes authoring order significant in a way no reader
would expect. Evaluating all filters first costs one extra snapshot and removes
the entire class of problem.

Within phase two, every match is applied rather than only the first one winning,
because that is Mango's shape. The order is the array order §9.7 defines, a name
sort rather than a hash, and a later rule wins per field. That satisfies
`layoutengine.md` §3.2's determinism requirement for free, because the order is a
stated comparison over a name and not an iteration over a table. Two details of
Mango's application are worth keeping and one is not.

- **A rule that supplies a value sets a sticky flag beside it.** Mango's
  `iscustomsize`, `iscustompos` and `iscustom_scroller_proportion` are set when a
  rule supplies a value, and a later arrange consults the flag rather than
  re-reading the value, which is why a per-client value needs to record *that it
  was customised* separately from *what it was customised to*. This prototype
  keeps it.
- **Tags are assigned as a whole set, last writer wins.** This is the one field a
  `then` block may set that is not a fact about the client, and it is deliberately
  not Mango's mechanism. `client_apply_rules` at `src/manage/client.c:1460` unions
  across matching rules:

  ```c
  if (r->tags) {
          newtags |= r->tags;
  } else if (parent) {
          newtags = parent->tags;
  }
  ```

  which is a special case inside a store whose stated rule is last-writer-wins
  (`configstorage.md` §1), and it is worth being precise about what Mango actually
  does, because it is not quite the shape it looks like: `newtags` starts at 0, the
  union is between *rules*, and the computed value then **replaces** the client's
  tags, with a per-rule parent fallback and a `TAG0_MASK` fallback when nobody has an
  opinion. So the union is not rules-on-top-of-the-default, it is rules-replacing-the-
  default with the defaults filling in behind them. Here the whole set is simply
  assigned, so `then.tags` is one write with one writer and the store's ordering rule
  is untouched, and two rules both naming tags means the later one in §9.7's order
  wins outright rather than merging. **A rule names tags, and the named set is where
  the client goes**, which is the "this program lives on that workspace" case and is
  the reason the field exists at all.
- **A once-only rule records that it fired on the rule, not on the client.** Mango
  keeps `is_once_applied` in the `ConfigWinRule`, which every matching client
  shares, so a second window matching the same once-rule is skipped as already
  applied. This is a defect and is not copied. Under this design the fact is
  unnecessary anyway, because a rule is applied once per client by construction
  (§9.1), so "once" is the only behaviour and needs no flag.

### 9.5 What a rule produces: a binding, not a copy

A `then` block does not carry a client's rules with it. It **binds named sets**,
and the rules stay where they already are, at `omniwm.clients.<set>.rules`, shared
by every client bound to that set and written once.

For a client working on the block directly, the simplest form is **one named child
per bound set** under a fixed prefix on the client's entry, not a list value.
Adding a binding is adding a key, dropping one is dropping a key, and enumerating
is walking a prefix. There is no index arithmetic, no append-versus-replace
question, and no addressing into a partially written record. `tags.md` §5 already
made exactly this trade for tag membership, one child per member rather than a
list, for the same reason, so this is the consistent answer and not a new one.
`layoutlanguage.md` §3.6.1's client-set syntax is unchanged by any of this; what
changes is only that a **real** client can now reach a set by matching, where
before only a fake client could and it did so by name.

Two consequences of binding rather than copying, and one is a limit.

- **Order stops mattering for the constraints, because they compose as a set.**
  They are soft weighted constraints (`layoutengine.md` §3.2) and a fixed point is
  reached regardless of the order they are presented in, so several bound sets
  need no ordering. The array order of §9.4 governs the *seed precedence between
  rules*, not the relationship between `0x33` values.
- **Resolutions are mutations, and mutations need an order.** `layoutengine.md`
  §3.2 draws the line explicitly: a reflow resolution moves a client from one
  space to another and is outside the solver because it mutates. A client bound to
  more than one set therefore has resolutions that have to be ordered, and two sets
  that both resolve the same client will disagree if nothing says which goes first.
  **The order is the rule list's name order, sorted, and run as an array**, which
  §9.7 sets out. The naming is the order, and a user who wants to be explicit
  writes names like `00_rule1` and `10_rule2`, so rule2 always applies after
  rule1.

### 9.6 Re-applying is a capability, not a behaviour

There is a function that applies rules to a client, and a function that applies
them to every client. Mango has both and has named the second one exactly what it
is, in `src/config/parse_config.c`:

```c
void reapply_window_rules(void) {
	Client *c;
	wl_list_for_each(c, &server.clients, link) client_apply_rules(c);
	arrange(server.selected_monitor, false, false);
}
```

That is wired to a config reload there, and here it is deliberately **not** wired
to anything automatic. Re-applying is available as an operation, on one client or
on all of them, so that a script which wants a refresh asks for one; it is never a
consequence of a remap or a reload happening to touch a client.

This is what makes §9.1's one-shot choice safe rather than lossy. A design that
only ever seeded once would have no way to re-run a seed after changing a rule, and
would be a dead end for anyone who wanted that. Exposing the existing function as
an operation closes it. Note that the Mango version also arranges, so ours has to
take the affected monitor or arrange the affected tags, or the command will appear
to do nothing.

### 9.7 The rule list is sorted by name, and the name is the order

The rule list is an **array ordered by name**, sorted, and run in list order. One
ordering answers all three of the questions this section has been separating, which
is why it is worth stating once here rather than in each place: the seed precedence
between rules (§9.4), the resolution order for a client bound to more than one set
(§9.5), and the tie-break between two rules that seed the same field (§9.4). All
three are the same array, so they cannot disagree.

**The order is alphabetical, and numbering is a choice a user may make.** That is
the whole rule: the list runs in name order. A user who wants to make an intent
legible writes names like `00_rule1` and `10_rule2`, and `rule2` then always applies
after `rule1`, because `00_` sorts before `10_`. A user who does not care about order
writes plain names, and gets alphabetical order, which is exactly as deterministic as
any numbered choice. The order is never undefined; the numbering only changes whether
the resulting order is the one the user meant. There is no separate priority field to
set, keep in sync, or misread, so a user cannot express an order in one place and a
different order in another, and the order is visible without reading the array,
since the names are in it.

**omniWM ships no default window rules**, so nothing occupies the namespace and no
name is reserved. That is worth stating next to the ordering because the convention
this borrows usually comes with a fragment already in place: those tools put their
shipped file at `50_` so a user can add one before or after it without renaming
anything. With no built-in rule there is no `50_` and no slot to be careful of, and a
user starting from an empty namespace has no reason to number anything until they
want the order to say something.

**The alphabetical convention is a known one rather than a local invention**, which
is the reason it needs no machinery around it. A great many Linux tools collect
fragments from a directory and apply them in filename order, and the way their users
take control of that order is by prepending a number to the filename. The same shape
appears in sysctl and systemd drop-in directories, in `update-motd.d` and
`logrotate`, and in runparts-style `rc.d` handling. A user who has configured any of
those already knows the convention and needs no explanation, which is the property
that matters most here: the ordering is discoverable from the tooling everyone else
uses rather than only from this document.

Two properties of sorting by name, and both follow from the order being a stated
comparison rather than a default.

- **The sort must be a defined order rather than whatever the comparison happens to
  do.** A comparison that treats `Rule10` and `Rule2` as equal, or that orders by
  locale, is not reproducible across machines, and §9.4's determinism requirement
  is exactly this. So it states byte order over the name, the same comparison a
  directory walk applies, and there is no tie to break afterwards because the names
  are store keys and keys are unique.
- **The order is stable across restarts and across machines**, which the order of a
  hash or of a directory walk is not. That is what lets a seed be reproducible, and
  it is the same reason `configstorage.md` treats a hash order as unusable for
  anything a user reads.

**No guard, and deliberately none.** Byte order will do what byte order does: `9_`
sorts after `10_`, and an uppercase name sorts before a lowercase one. Both are
reachable only by a user who has departed from a convention they already know from
everywhere else, and the alternatives would each cost more than the mistake is
worth. A `priority` field is a second place to express an order and a second thing
to keep consistent with the names; validating the prefix rejects a name shape that
other tools accept and that nobody has to use here; sorting a canonicalised name is
a comparison a reader cannot predict by looking at the namespace. The whole point of
borrowing the
convention is that it needs nothing added to it, and a guard would reintroduce
exactly the complexity the convention exists to avoid. `lcheck.py` cannot check this
either, and does not try: the suite has no case of two sets both resolving one
client, so there is nothing to assert against.


## 10. Fake clients take part in layout and in focus

`generaldesign.md` §13 is unambiguous and is the whole of this section's
authority: a fake client is a widget the compositor draws that behaves like a
window, and the point of calling it a client is that it takes part in layout and
in focus exactly as a real window does. A bar is a window the compositor happens
to draw, so it can be tiled, floated, tagged, and animated through the same code.

**Mango has no fake clients at all**, so rule 3 applies and the derivation comes
from our own logic. Mango's compositor-drawn surfaces, `MangoGroupBar` and
`MangoJumpLabel` in `include/mango/draw/text-node.h`, are both scene nodes hung
off a client, and neither is in the client list or the focus list. So there is
nothing to copy, and the shape is derived from what the rest of the design
already requires of a client:

- **A fake client is a catalog entry**, because `layoutengine.md` §7 requires the
  solve output to be diffable per node and a bar that is placed by a layout has to
  be diffable in the same array as a window, or an animator cannot move one
  without touching the rest. It therefore has an `entry_ref` like anything else
  the solver places.
- **It has a `kind`**, which is Mango's `Client.type` in the one place Mango is
  the right guide: a single tag on the client distinguishing a real toplevel from
  a compositor-drawn one. Mango's enum runs `XDGShell`, `LayerShell`, `X11`,
  `Snapshot`, `XdgPopup`, `XdgImPopup` and `GroupBar`, and the prototype keeps the
  idea and not the members, because our kinds are chosen by what participates
  differently.
- **It has the same lifecycle**, because §3's table is written in terms of states
  rather than surfaces and a fake client moves through them unchanged. A
  compositor-drawn client is constructed at a config-driven time rather than at a
  surface event, and that is the only difference.
- **A group's titlebar is one of these**, and §8 is why: ours is placed by the
  group's program and Mango's is measured by the layout, so ours is a participant
  and Mango's is chrome.

The one place Mango and this section genuinely disagree is focusability, and here
`generaldesign.md` §13 is the authority and Mango is the discarded alternative:
Mango's bar and jump label are not focusable, because they are not clients, while
§13 says a fake client participates in focus exactly as a real window does. That
is defensible and it is also the decision most likely to need revisiting, since a
focusable bar is a bar that can take the keyboard away from the user's window, so
§13 carries it as an open question rather than a settled one.

## 11. Teardown is an exclusion, and then a delete

`layoutengine.md` §11 records the mechanism as settled: teardown is an exclusion
and never a cancellation. Unmap sets the kill flag, every scope predicate drops
the client, and nothing in the arrange aborts, so a dying client simply stops
being an input and there is never work in flight to cancel. What the window layer
adds is the second phase, and Mango is the evidence for needing it.

- **Phase one, unmap: the client stops being an input and nothing is deleted.**
  Its membership children stay, because Mango's `fadeout_link` exists for exactly
  this: a client being faded out has to stay reachable while its animation runs.
  Deleting membership at unmap would make the node vanish from the very diff the
  animator is interpolating.
- **Phase two, destroy: the entry and its whole subtree go.** A client's teardown
  is a subtree delete, and `tags.md` §5 and §7 record that the store has no
  subtree operation, so this is a store requirement rather than something this
  document can assume. The focus pointer is repaired first, the client is removed
  from the focus-recency list, and the last solved rect has already been consumed
  by the exit animation.
- **The generation is what makes the second phase safe.** A membership child or an
  override naming the client carries its `entry_ref`, and `configstorage.md` §4
  requires the generation to match the live entry, so a delete that arrives after
  the slot was reused is refused rather than applied to the new occupant. Mango
  needs no such check because its links are pointers that vanish with the struct;
  ours survive in the block and have to be checked.

## 12. What the solver reads, and what it must not be told

The contract, in one place, because it is the boundary this document exists to
hold. A solve is a pure function of committed state (`layoutengine.md` §3.2), and
its input is a coherent snapshot (`configstorage.md` §1), so:

- **The input is a walk of membership children**, in the order §4 fixes, over
  clients that are visible, not killing, and not floating.
- **The per-node identity is `entry_ref`**, and the output is a `rect` per node in
  the solved layout section, diffable by `(entry_id, entry_generation)` between two
  solves (`layoutengine.md` §2.10).
- **A program names no window** (`layoutengine.md` §3.1), so the solver receives a
  set of identities and their state and never a selection. A cluster has already
  been resolved to its single occupant before this point, which is what makes the
  input a flat set of nodes rather than a tree.
- **What this document must not decide**: which layout is selected
  (`layoutengine.md` §3.5), what the program says (`layoutengine.md` §3.3), where
  membership is stored (`tags.md` §5), what a decoration value is
  (`decorate.md`), and how a move is animated (`animate.md`).

## 13. Open decisions this prototype does not settle

1. **Whether the membership walk order is catalog order for good.** §4 takes
   `entry_id` order because it is reproducible and Mango's list order is not. It
   has not been checked against the cost of a sparse client-id space, where
   ascending order is a scattered walk rather than a dense one.
2. **Whether a focus change causes a pass.** Deliberately left to
   `layoutengine.md` §3.7. §7 records that the focused client is an input to a
   stack and a monocle, and that Mango makes the trigger conditional on the
   layout, which is evidence that the answer is per layout rather than global.
3. **Whether a fake client can hold the keyboard focus.** `generaldesign.md` §13
   says it participates in focus exactly as a real window does, Mango's equivalent
   surfaces are not focusable because they are not clients, and the honest reading
   is that §13's sentence is about participating in a focus *order* and not about
   being able to take the keyboard.
4. **Whether the client kind set is closed.** §10 takes Mango's single tag on the
   client as the shape and leaves the members open, because our kinds are decided
   by what participates differently in layout, focus and stacking, and that list
   has not been written down.
5. **What a cluster contributes to stacking and focus beyond moving as one.**
   §7 and §8 take Mango's raise-the-whole-group rule by analogy for a cluster,
   which is an analogy rather than a finding, because Mango has no cluster.

Four that were open here and are now settled, kept because the reason is the useful
part. **The window-rule matchers** are §9: a block-structured if/then over any
observable client field, with the filter and the assignments on named sides so the
two can share a vocabulary, and the rules in the block so they are inspectable.
That in turn forced `appid` and `title` to become client fields, which is the one
requirement in §9.3 that was not obvious before the decision. **A once-only rule**
is no longer a question at all, because a rule is applied once per client by
construction, so the per-client versus per-rule defect Mango has cannot arise in
this design. **The order of resolutions** for a client bound to more than one set is
§9.7, and it is the list's name order rather than a field: the array is sorted and
run in order, so the naming is the order, and a user writes `00_rule1` and
`10_rule2` when they want to be explicit about it. That closed the last part of the
override-representation blocker, which is why `layoutengine.md` §10 no longer lists
it as blocking. **A tag seed landing on a grouped client** is §9.3 and
`tags.md` §5, and the answer is that it is inert rather than refused: a seed writes
membership, membership on a nested client is the straddling state `tags.md` §4 rules
out, so the write waits until the client leaves its container. Where the waiting
state is kept is the one thing in this group I inferred rather than was told, and
§9.3 names it.


## 14. Not covered here

What a window looks like, its decorations and their precedence
(`decorate.md`); how a move is animated (`animate.md`); how a gesture reaches an
action (`input.md`, `helpers.md`); what a tag looks like and how one is set
(`tags.md`); the client's own protocol surface (`ipc.md`); the surface syntax for
any of the above (`tomlparser.md`); and the order of a commit's visibility
(`configstorage.md` §1). The client's X11 and Xwayland integration is deliberately
absent too, since `generaldesign.md` §6 makes omniWM's break with Mango the tags
and there is no reason to import a second one here.

## 15. References

The authority for anything this document inherits rather than decides:
`generaldesign.md` §6, §8, §11 and §13; `configstorage.md` §0.1, §1, §4 and §8;
`configstorelayout.md` for the solved layout section; `tags.md` §1, §5, §6, §7 and
§8; `layoutengine.md` §2.10, §3.1, §3.2, §3.5, §3.6, §3.7, §7.1, §7.7, §8 and
§11; `include/shared/omni_layout.h` as the only numeric home.

The reference implementation, consulted at `d5a0e1e` of `mango-dev` on branch
`config-scriptability` and indexed by `research/mango.md`: `struct Client` and the
scope predicates in `include/mango/manage/client.h`; the lifecycle, focus,
per-target-layer and rule application in `src/manage/client.c`; the group link
operations in `src/dispatch/bind.c`; the arrangement walk and group-bar
measurement in `src/layout/arrange.c` and `src/layout/scroll.c`; the scene nodes in
`src/draw/text-node.c`; and the `is_group` export in `src/ipc/ipc.c`. Everything
attributed to Mango above is at that commit and is checkable there.
