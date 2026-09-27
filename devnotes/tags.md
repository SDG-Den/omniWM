# Tags

The Tag entity, its identity, what it owns, and how a monitor displays tags. This
document exists to answer the questions `layoutengine.md` raises about tags, and
it deliberately stops short of the whole tag feature. What a tag looks like to a
user, tag colours, tag icons, per-output tag defaults and tag rules are not
covered here; they are listed as not covered in §9 so the omission is a decision
rather than a gap.

## 1. What a tag is

`generaldesign.md` §6 makes a tag a first-class entity that owns its client set,
its own layout state, and the other data needed to view it, and that belongs to
exactly one monitor. It does not own
floating state, because that is a fact about a client rather than about a place
windows are shown, and §6 makes that explicit. This document gives the entity an
identity and a storage shape, and it settles one thing `generaldesign.md` §6 left
implicit and `layoutengine.md` depends on.

A tag is **a container of three things: a set of clients, a layout, and the rest
of the data needed to view it.** Everything else a tag seems to be is derived
from those. This is a narrower claim than "a tag is a workspace", and the
narrowness is the point: it is what lets a monitor show several tags at once,
because a tag is an input to an arrangement rather than an arrangement itself.

The three parts, and who owns each:

| part | what it is | notes |
|---|---|---|
| client set | which clients are on this tag | the only part a secondary tag contributes, see §4 |
| layout | which layout arranges them, by name from `layoutengine.md` §4 | `generaldesign.md` §7, as amended |
| other tag data | viewport, spawn mode state, and whatever else a tag turns out to own | not designed here; floating is deliberately absent, because it is client state, see §6 |

## 2. A tag is a catalog entry, so its identity is an `entry_ref`

A tag is a namespace in the store. `configstorage.md` §0 makes the block an open
catalog where a key nobody has heard of is stored and served verbatim, and a
container of several named values is what a catalog entry with children is.
A tag is therefore a catalog entry, nested under its monitor's entry (§5), and a tag
is referred to by
`(entry_id, entry_generation)` through the existing `entry_ref` tag at `0x23`.

**The nesting is what makes §3's decision representable, and it is load-bearing
rather than cosmetic.** If a tag were a top-level catalog entry with its monitor in
a sibling value, then two tags could claim the same monitor, and "a tag belongs to
one monitor" would be a convention the storage declined to enforce. As a child of
`wm.monitor.<id>` the parent *is* the monitor half of the identity, so the store
already knows it, and a caller that finds a tag entry has already been told which
monitor it is on without reading anything the caller could have got wrong. This is
the same reasoning that put the layout program at a fixed prefix rather than
resolving it by scan, and it is why the tag needs no new identity kind for the
monitor: the path carries it.

This is the same shape a client has, and deliberately so. `omni_layout.h` already
makes a client a catalog entry: the solved layout node is keyed by `entry_id` plus
`entry_generation` (`configstorelayout.md` §11). A tag that was anything other
than a catalog entry would need a second identity kind beside the one the layout
engine already uses for the things it places, which would be a redundancy bought
for nothing.

The constraint record is deliberately *not* a second example, because under §6's
model a layout program names no window at all and a record has no entry identity.
The per-node `entry_ref` lives in the solved layout, which is the engine's output
rather than the program's input.

What this buys, and it is not only storage:

- A tag's identity survives its name. `configstorage.md` §0.1 says a rename
  creates a new entry identity, so a tag the user calls `web` and a tag the user
  later calls `browse` are the same tag to a layout binding that captured its
  `entry_ref`, and a different tag to a config file that names `web`. That is the
  behaviour the rest of the project already assumes of every other named thing.
- A layout can be selected per tag as a value, which retires the storage half of
  `layoutengine.md` §3.5's open question. The tag holds a layout name, the
  binding holds a name, and neither needs a new tag type.
- `save` and soft reset need no special case. A tag's own entry and its children
  are ordinary configuration, so they are written by `save` and cleared by a soft
  reset exactly as any other value is, and the client back-references are
  `WINDOW_DEPENDENT` and therefore survive a reset untouched.

What it does not settle, and this is a real cost: a tag is now a visible key in
the catalog, so a user can read and write tag state directly over the socket. That
is usually what the open-catalog property is for, and `configstorage.md` §0 is
explicit that the block cannot identify writers and enforces no ownership, so it
is a consequence of the existing model rather than a new hole. It does mean a
malformed tag state is a semantic failure the writer caused, which
`configstorage.md` §12 already has a category for.

## 3. A tag's identity is its monitor and its number

**A tag is identified by the pair `(monitor, tag number)`, and the monitor is half
of the identity.** Monitor 1's tag 3 and monitor 2's tag 3 are two different tags
with two different client sets and two different layouts, and nothing in the system
can cause one to become the other. This is the decision, and it reverses what an
earlier version of this section said: that text held a tag to be neither a bit
position nor an index into a per-monitor array, and then claimed moving a tag
between monitors renumbers nothing. Both halves of that are gone, because both
described a tag that belongs to no particular monitor, and a tag that belongs to no
particular monitor cannot be per-monitor.

**This is the same departure from Mango's, but the reason is a safety property and
not a modelling preference.** Mango's tag *is* a bit position in a 32-bit mask with a
per-monitor state array indexed by that number, which means Mango is already
per-monitor in its state and only pretends not to be through the bitmask, which is
global. omniWM keeps the per-monitor array, drops the global bitmask, and makes the
array the identity. So the break with Mango moved: the break is no longer "a tag is
an object", because under this decision a tag is very nearly a slot in a
per-monitor array, and the break is "the bitmask and the client-held mask are gone,
so there is no way to name a tag without naming its monitor."

**Mirroring is not a feature this design gives up. It is the failure the per-monitor
identity exists to prevent, and the reason is a protocol guarantee rather than a
taste.** A client expects to have exactly one surface on exactly one output at a
time. Under mirroring, a client on a mirrored tag is being asked to present on two
outputs, which is not a layout the client has any way to honour: it has one
`wl_surface`, one set of buffer bounds, and one enter/leave history per output, and
a compositor that shows it on both is relying on behaviour nobody specified. That is
undefined behaviour, not a rough edge. Under this section's identity there is no
state in the system that can express it, so the class of bug is unrepresentable
rather than merely avoided.

This is worth stating as the section's own claim, because the previous version of
`generaldesign.md` §6 listed mirroring as a *benefit* of not having per-monitor tags,
and that was the reasoning running backwards. It quoted dwl and Mango's ability to
mirror a tag across outputs as something a shared tag entity would preserve, and used
it to reject per-monitor state. The argument now runs the other way: mirroring is
the behaviour that a shared tag entity makes expressible and per-monitor tags make
unrepresentable, and since mirroring is the unsafe one, the choice was never between
gaining a feature and losing one. It was between a design where a protocol violation
is expressible and one where it is not.

The second thing this decision rules out is the Hyprland model, and for a different
reason. Hyprland has one workspace set shared across all monitors, which is how it
makes "move this workspace there" a single object. Adopting that here would mean
either accepting a single global tag numbering, or, in the case the reference model
actually handles, treating several monitors as one large workspace and numbering
accordingly, so that what a three-monitor user experiences as one workspace is
workspaces 1, 11 and 21 combined. That is a genuinely janky user model, and it is
the thing per-monitor tags remove: each monitor numbers its own tags 1 upward, and
§7's swap is how a user moves content between them.

So the two properties being asked for are not in tension and neither required a
shared tag. Per-monitor numbering gives Mango's model (each monitor has tags 1
through 9 or more, and a monitor can display several of its own tags at once for a
multi-tag view), and §7's content swap gives Hyprland's ("get this over there")
without a shared workspace set. What the swap costs in fidelity is nothing, because
the tag under the content is not what the user cares about: two tags are two slots,
and exchanging their contents leaves the user holding what they asked to move. That
is why it is a swap of contents rather than a move of the tag, and it is why it is
functionally the same operation as exchanging two workspaces in Hyprland while being
expressible in a per-monitor model at all.

One consequence follows for the derived value rather than for the store: **a client
on two monitors is two tag memberships, not one mirrored membership.** Under the old
model a client on a mirrored tag appeared once; now it appears on
each monitor's tag, and the derived value in §5 is a set of `(monitor, number)`
pairs rather than a set of numbers. This is the same guarantee as above seen from
the other side: the membership structure is what makes "one client, one monitor"
expressible, because a client cannot be a member of a tag that is itself on two
monitors.

### 3.1 The invariant is *at most* one monitor, not exactly one

**A tag is displayed on at most one monitor at a time, and a tag that is displayed on
none is not an error.** That is the whole of the rule, and it is deliberately weaker
than "a tag belongs to exactly one monitor" because the scratchpad needs the weaker
form and because the weaker form is the one the safety argument actually requires.

The safety argument above forbids a tag on two monitors *at once*. It says nothing
about a tag on zero. A tag is a container that exists whether or not anyone is
looking at it: `wm.monitor.2.tag.5` with nothing displayed is a tag holding
windows the user has stepped away from, and there is no reason to destroy it or to
pretend it is a different kind of object. So the two cases are separated:

- **A per-monitor tag** is a slot in one monitor's array, so it has a monitor as
  part of its identity and it is displayed on that monitor or not at all. It is
  never on a second monitor, and the primary at the head of the list (§4) is simply
  the first of that monitor's own slots that the list still holds.
- **A singleton tag** has no monitor in its identity at all, and is displayed on one
  monitor or on none. The shared scratchpad is the one of these (§8), and it is the
  reason the rule is stated as "at most one" rather than "exactly one": a
  configuration in which every monitor has its own scratchpad is the same code with
  N singletons instead of one, and both configurations obey the same invariant.

This distinction is what lets §8 offer the scratchpad as shared *or* per monitor
without either option being a special case in the model. The invariant is stated
once, in terms of display, and the two options differ only in how many tags there
are and where their entries are stored. Nothing in the store, the solver, or the
membership rules needs to know which configuration is in use.

**A singleton is still forbidden from being displayed twice, and that is where the
scratchpad's one-monitor rule comes from.** "The shared scratchpad can only be open
on one monitor at a time" (§8) is not a scratchpad rule at all; it is this
invariant applied to the one tag that is not already pinned to a monitor, and it
holds for exactly the reason the mirroring argument holds. A client in the
scratchpad is on a surface on whichever monitor has it open, and opening it on a
second monitor while the first still has it is the same undefined behaviour with a
different tag. The reason a *per-monitor* scratchpad does not need a special rule is
that it is two separate tags with separate contents, not one tag shown twice.

Neither is being deferred. Mirroring in particular is the feature the previous
version of `generaldesign.md` §6 listed as a reason to reject per-monitor tag
state, and that reason no longer applies because the feature it was protecting is
the one now given up.

A tag number on its own is still not identity, and the rest of this section is
unchanged by the decision. Both the name and the number are conveniences a user
interface may present, and neither is identity by itself, for the reason
`configstorage.md` §0.1 already gives: a recycled slot is never accepted as the
same object without its generation.

That is a statement about *references*, not about *addressing*, and the difference
matters to anything a human authors. A rule written by a user cannot hold an
`entry_ref`, because the epoch and generation in one are not stable across a restart
(`windows.md` §9.4's `then.tags` is the case), so it names a tag the way every other
piece of configuration names anything: by the string a user would type, which under
this decision includes the monitor. Resolution is
a lookup of that name in the catalog, which is a key-value store keyed by name
(`configstorage.md` §3), so a name does select exactly one tag at the moment it is
resolved. Every check after that uses the `entry_ref` the lookup produced, and
staleness is detected the way §5 requires, by comparing all three components. So a
name is a way to *find* a tag and an `entry_ref` is the only thing a *reference* may
hold, and the rule that identity is neither its name nor its number is not weakened
by anything in this section. What the decision adds is that identity is also not its
number *alone*, which is why every user-facing spelling of a tag in this project
carries a monitor.

## 4. A monitor shows an ordered list of its own tags, and the head is the primary

This is the part `generaldesign.md` §6 does not say and the part the layout engine
most needs, so it is stated plainly.

A monitor does not display a *set* of tags. It displays an **ordered list of its
own tags**, and the two words that matter are "own": the list may only contain tags
whose identity is this monitor (§3), so there is no such thing as a tag appearing
in two monitors' lists and the mirroring case that an earlier version of this
section left room for is not representable. The order is chronological: a tag is
placed in the list when it is set on that monitor, and the list is not re-sorted
afterwards. The first tag in the list that
is still set is the monitor's **primary tag**. Every other tag in the list is
**secondary**.

So, for monitor 1:

| action | result |
|---|---|
| set 3, then 4, then 5 | list is 3, 4, 5; primary is 3 |
| set 3, then 5, then 4 | list is 3, 5, 4; primary is 3 |
| unset 3 from 3, 5, 4 | list is 5, 4; primary is 5 |
| exclusively set 5, then set 3, then set 4 | list is 5, 3, 4; primary is 5 |
| set 3 on monitor 1 and 3 on monitor 2 | two tags, two lists, each with its own primary |

The last row is the decision showing through the ordering rules, and it is worth
being explicit that the list is not shared: monitor 1's list being 3, 5 and
monitor 2's being 3, 4 says nothing about either tag, and the two tag 3s have
unrelated members.

The order is chronological and not numeric, so a lower tag set later never
displaces a higher one set earlier. The primary is the **head of the list**, not
the lowest tag number and not the most recently focused tag.

The primary is **derived from list order rather than stored as its own field.**
That is a deliberate choice and it is the reason the rule is stated in terms of
order at all. A separately stored primary is a second fact that can disagree with
the list, and it would have to be repaired on every set, unset, exclusive-set,
content swap and monitor-unplug. Deriving it means there is nothing to keep
consistent,
and the promotion rule when the head is removed falls out for free: the next
element becomes the head.

Because the primary is derived, changing it without changing which tags are shown
is expressed the way the last two rows of the table show, by exclusively setting
the tag you want and re-adding the rest. There is no operation that reorders the
list in place. That is a real ergonomic cost and it is the price of not storing a
second fact, which is the right trade for a first implementation.

What the primary supplies, and this is the whole point of it:

- **The layout.** The primary's layout arranges the view, per `generaldesign.md` §7.
- **Everything else the tag holds.** Pan and zoom viewport, spawn mode state, and
  whatever else a tag turns out to own.

Floating is not on that list, and its absence is the point. Which clients float is
a fact each client carries (§6), so a secondary tag's clients arrive at the
arrangement already knowing whether they float, and the primary has no say in it.

What a secondary tag contributes is **its clients and nothing else.** A client on
a secondary tag is placed by the primary's layout, and floats or tiles by its own
state.

Three consequences follow, and they reach into `layoutengine.md` rather than
staying here:

- **`generaldesign.md` §7's "layout state is per tag, per monitor, plus a
  floating layer" is right about the first term for the wrong reason and still
  wrong about the floating one.** It used to be wrong in both of its first terms,
  and this section used to say so, because at that point a tag was not per monitor
  and a pairing of tag and monitor had no state of its own. Under §3 a tag belongs
  to exactly one monitor, so "per tag" and "per monitor" now say the same thing and
  the pairing adds nothing. The floating layer is not per tag at all and never was
  the tag's business; it is per client (§6), which is where `generaldesign.md` §8
  already put it. One of the two corrections is therefore withdrawn rather than
  applied, and the surviving one still changes how `layoutengine.md` §2.4 counts
  the engine's scopes.
- **The solver's input is now defined, and it is not a scope.** A solve takes the
  primary tag's layout, the union of the client sets of every tag in the monitor's
  list, and each of those clients' own floating state. The arrangement is of that
  union, and it is the monitor's list that selects the input rather than any one
  tag being arranged on its own. This is the answer to the part of
  `layoutengine.md` §3.5 that asked how a layout is selected per tag.
- **Which layout applies to a client on two tags is no longer ambiguous.** It is
  the primary's, always, because a secondary tag contributes clients and no
  layout. This retires the ambiguity `layoutengine.md` §3.1 would otherwise have
  to resolve for the multi-tag case. The same rule settles the other half: a client
  on two tags does not get two arrangements, because nothing about it is per tag
  except where it is placed. This holds across monitors too, and it is the one
  place where the per-monitor decision does not multiply work: a client on
  monitor 1's tag 3 and monitor 2's tag 3 is arranged twice, once per monitor,
  because those are two different views of two different monitors, and within
  either one the client takes the primary's layout.

One case the list order used to make visible, and which `layoutlanguage.md` §3.8
reopened, is now settled and it is not a case at all: **nothing inside a group or a
cluster is on a tag, so nothing inside one can straddle a tag boundary.** A group or
a cluster is one occupant of its parent, and the tag's member set holds the occupant.
The contents are in the container, not in the tag, and they carry no membership of
their own. §5 has the storage shape.

This is why a cluster occupying one space was not the regression it first looked
like. Under the old definition, where a cluster's members each kept their own place
in the parent, straddling was coherent by definition and this question never had to
be asked. Resolving clusters to one virtual client made the containment explicit
rather than changing the answer, and the answer was always going to be that a member
of a container is placed by the container.

## 5. What a tag stores, and where membership lives

A tag is a catalog entry, **nested under its monitor's entry**, so that the monitor
half of the identity from §3 is structural rather than encoded in a string a
keyboard path has to spell correctly. The shape is:

```
wm.monitor.<monitor_id>            the monitor's own entry
wm.monitor.<monitor_id>.tags       the ordered list from §4, held as a value
wm.monitor.<monitor_id>.tag.<n>    one tag, itself a container
```

and a tag's parts are children of *that* entry, with the `layoutengine.md` §4.3
rule that a name is a value applying to each of them.

**A singleton has no monitor to nest under, so it sits at `wm.tag.<name>` instead,
and that difference in shape is the whole mechanism.** The shared scratchpad is the
only singleton in v1:

```
wm.tag.scratchpad                   the shared scratchpad, a container
wm.monitor.<monitor_id>.tag.scratchpad   that monitor's own scratchpad
```

Nothing special-cases the `wm.monitor.` segment: a path that carries it is a
per-monitor tag whose identity includes the monitor, and a path that does not is a
singleton, which is what §3.1 means by the two kinds differing only in *where
their entries are stored*. A per-monitor scratchpad is therefore an ordinary tag
that happens to have a reserved name and cannot appear in a monitor's `tags` list,
rather than a second kind of thing. Two details fall out of the shape rather than
being stated separately:

- **The setting and the tag do not share a prefix.** Whether the scratchpad is
  shared is `wm.scratchpad.shared` (§8.2), a `bool` at the top level, while the
  shared tag is `wm.tag.scratchpad`. Keeping the setting out from under
  `wm.tag.` is deliberate: the catalog is a flat store of dotted names with no
  trie (`configstorage.md` §3), so the two would not collide, but a reader
  inspecting `wm.tag.scratchpad.shared` would have no way to tell a child of the
  tag from a setting, and this is a design that should not need a paragraph to
  explain its own key names.
- **A swap may name a singleton and a per-monitor tag in the same command**, which
  is what `ipc.md` §4's `BAD_TARGET` rule is written to allow. A swap of the shared
  scratchpad with `wm.monitor.1.tag.3` is a legal exchange of one singleton and one
  pinned tag, and it is the same operation rather than a special case, because §7
  defines a swap as exchanging subtrees and neither kind is a special subtree.

| child | value | notes |
|---|---|---|
| the layout | a layout name, `0x0D` string | a name from the closed set, not a slot index |
| each member | a client `entry_ref` | see below |
| viewport and other data | per whatever it turns out to need | not designed here |

**The tag entry is a container, and that is now decided rather than open.** An
earlier version of this section left it as a question pending whether the store
would grow a subtree concept, and §9 recorded it as the decision that would force
one. It is closed: a tag holds several named children, so the store needs a
container, a prefix walk, and a way to exchange two containers' contents, and §7
lists all three as requirements. Nothing else in the design needs a subtree, which
is why this is the section that pays for the feature rather than an argument for
it in the abstract.

**Membership is stored on the tag, as one child entry per member client.** A group
or a cluster is a member in its own right, and its contents are not members of the
tag at all: they are in the container, which is itself a member, so their tag scope
is the container's and not their own. This is the containment rule from §4 and it is
what makes the entry list flat. A group on three tags has three entries pointing at
the group; the windows inside it have none, because a window inside a group is not in
a tag directly. Under §3 a group on monitor 1's tag 3 and monitor 2's tag 3 has two
entries rather than one, because those are two tags. The alternative, storing a
back-reference on each client, is what
makes the "which tags is this client on" question answerable directly, and the
direction matters
because the two have very different access patterns. The layout engine needs the
union of the client sets of a monitor's whole tag list on every solve, and solves
are frequent, so that direction must not be a scan of every client in the
compositor. The reverse question is asked rarely, by a switcher, so it can afford
to be a scan or a secondary index. Storing membership on the tag makes the hot
direction a walk of only the tags actually displayed, and under §3 that walk is
additionally confined to one monitor's prefix, which is the one place the
per-monitor decision makes the hot path cheaper rather than merely different.

This also makes multi-tag clients fall out rather than needing a feature: a client
with three tags has three member entries pointing at it, and the same client
`entry_ref` appears in three different unions. Nothing needs to know how many tags
a client is on.

Containment costs the reverse question one hop, and the reverse question is the rare
one. "Which tags is this client on" is now "which tags is this client's container on",
or the client's own entries when it is not inside anything, so a client inside a
group resolves by walking up to the group and reading its entries. §5's own reasoning
puts that question behind a switcher, so a walk is affordable there and would not be
in the solve path, which is the direction that had to stay a walk of only the tags
actually displayed. A client that is inside a group and also carries its own entries
is not a state the store can represent, and refusing to represent it is the point:
it is the straddling case §4 rules out, and it would be a client placed by one
layout while counted by another.

**Membership is the only place tags are stored, and a client has no tag field.**
That is this section's own storage decision, and it is worth stating against the
alternative because Mango does the opposite: `client->tags` is a `uint32_t` bitmask
on the client (`include/mango/manage/client.h:122`), so in Mango a client does carry
its tags. Here the direction is chosen for the access pattern in the paragraphs above,
and the consequence is that anything wanting to *read* a client's tags has to walk
membership rather than read a field.

That contrast is sharper than it was, and it is worth saying why rather than leaving
it as two adjacent storage facts. Mango's bitmask is not only a faster way to hold
the same membership; it is *how Mango makes a tag global*. A bit in a mask names a
tag number and nothing else, which is exactly why one bit can be set on three
monitors at once and why Mango needs its per-monitor state arrays to keep the three
views apart. Under §3 there is no global tag number to put in a mask, so the mask has
nothing to encode, and the per-monitor prefix is the whole of the mechanism. The two
choices are the same decision seen from two ends: no bitmask because no global tag,
and no global tag because tags are per monitor.

So the tags a window rule sees are a **derived read-only value**, and it is derived
from membership with one rule:

- a client inside a container is on whatever tags that container is on, since its own
  member entries are not consulted;
- a client inside nothing is on the tags whose entries name it.

Under §3 the derived value is a set of `(monitor, number)` pairs rather than a set
of numbers, and that is not a cosmetic change to the type: a filter that asked
"is this client on tag 3" has no well-formed answer without naming a monitor, so
every caller of the derived value gains a monitor argument it did not have. This is
the cost of the decision and it is paid at the filter boundary rather than smuggled
into it, which is the right place for it.

That derived value is exposed to filters as `tags` (`windows.md` §9.3) and it is
never written, which is what keeps §4's straddling refusal intact: a rule that *sets*
tags does so by adding a member child to a tag's entry, and if the client is inside a
container that would be the refused state, so the write is deferred until the client
leaves rather than applied to the container or dropped. The deferral is the one piece
of that mechanism whose storage is not settled; `windows.md` §9.3 records where it
would go and why.

The membership child is `WINDOW_DEPENDENT`. It is scoped to a live client, so
`configstorage.md` §8 excludes it from `save` and from a soft reset, which is
exactly right: the client set is reconstructed from the live clients, and a stale
membership entry surviving a restart would resurrect a client that no longer
exists. It is also what makes a client's teardown a subtree delete, and the store
has no subtree operation, so §7 records that as a requirement rather than assuming
it.

## 6. Floating is window state, so the client carries it

`generaldesign.md` §8 puts the floating layer in the window layer, and §4
confirms that placement rather than moving it. Floating is not tag data, so it is
not one of §1's parts, and it is not a scope the solver arranges.

**Whether a client floats is a fact about the client.** It is a value under the
client's own catalog entry alongside the other per-window values, and it travels
with the client to every tag that client is on. A client that floats floats
everywhere it appears. The general form of that is worth stating because it is
what makes the rest of this document predictable: anything that is a fact about
one window rather than about a place windows are shown is carried by the client.

Two consequences, and the first is a simplification rather than a new mechanism:

- **A secondary tag does not have to contribute the floating state of its clients,
  because the clients bring it.** Under §4 a secondary tag contributes its clients
  and nothing else, and per-client state is part of what a client is rather than
  part of what a tag contributes.
- **A client cannot float in one tag and tile in another.** An earlier version of
  this section claimed that it could and used it as evidence for a
  per-client-per-tag constraint record. That evidence is gone, and what replaces
  it is sharper: because all window state is on the client, `layoutengine.md`
  §3.1's question becomes *where a per-tag layout's seed lives*, since the client
  already owns the field a seed would go in, but a client on three tags has one of
  each rather than three. §3.1 decides it, and the multi-tag case is why it is
  still a real question rather than a formality.

Floating state is `WINDOW_DEPENDENT` like the rest of a client's values, and that
is not a problem for a preference the user expects to survive a restart. A window
rule seeds it by the if/then seed `windows.md` §9.2 and §9.4 defines, the rule is
ordinary configuration held in the block and is saved, and on restart it re-applies
to the new client entry. Seeded state does not need to be saved twice.

## 7. Moving a tag is swapping two tags' contents, and what that needs of the store

**There is no operation that moves a tag, because under §3 there is nothing that
could move.** A tag is a slot in one monitor's array, and the array is the monitor's,
so a tag that appeared on monitor 2 would be a different tag by §3's own definition.
The user-facing thing people call "moving a tag to another monitor" is therefore
re-expressed as what it has always really been in a per-monitor world: **exchange
the contents of tag A on monitor 1 with the contents of tag N on monitor 2.** The
two tags stay where they are; the clients, the layout, and the viewport swap places.

This is the same operation Hyprland and i3 perform for "move this window to that
workspace", and it is the reason this document cites Hyprland at all. The citation
used to be for something this project no longer has: an earlier version said a tag
could be *carried* to another monitor with its clients and state, which is an entity
move, which requires the entity to be monitor-independent, which §3 forbids. What
survives from Hyprland is the swap, and the swap is a strictly weaker thing that
needs no new identity model.

**The whole swap is one IPC operation, and one commit.** This is a requirement and
not a convenience, and the reason is the same reason the client layer is one commit
(`windows.md` §4). A swap performed as two writes is observable halfway: there is a
moment when monitor 1's tag 3 holds what monitor 2's tag 7 held and monitor 2's tag 7
holds nothing, and a solve, a switcher, a `save`, or a client reading membership
during that moment sees two tags that never existed. Publishing the exchange as one
grouped commit means no reader ever sees the intermediate state, and the journal
carries both halves in one group, so a replaying subscriber applies both or neither
under the group rules `configstorage.md` §8 already defines.

The exchange is over the tag's **contents**, not its entry, and the distinction
matters for identity. Each tag's entry stays at its own path with its own
`entry_id` and generation, so every `(monitor, number)` reference in the system stays
valid across a swap and nothing has to be re-resolved. What moves are the children:
the member entries naming client `entry_ref`s, the layout name, and the viewport. An
`entry_ref` is monitor-independent, which is why a client can be written into the
other monitor's tag without conversion; the monitor is part of the *tag's* identity,
never part of a client's.

Appending is the right choice over inserting at the head, and it is worth being
explicit that it is a choice: appending means a tag swapped onto an empty monitor
becomes that monitor's primary, which is almost certainly what is wanted, and it
means a tag swapped onto a busy monitor does not silently steal the arrangement.
This is unaffected by the change in this section, because the list being appended to
is the same list §4 defined.

Three requirements this places on the store, none of which exist yet, and all of
which are consequences of §5's membership shape rather than of tags as a concept:

- **A subtree delete.** Destroying a client must remove its membership children
  from every tag it is on. `configstorage.md` §8 classifies `save` and soft reset
  by flag, which sidesteps this entirely, but client teardown cannot: it is a
  delete of every entry beneath a prefix. Without it, a destroyed client leaves
  membership entries naming a recycled `entry_id`, and the next client to land in
  that slot appears on the old client's tags.
- **A subtree exchange.** The swap above is a prefix-level operation on two
  containers at once, and it is a *different* operation from the delete, not a
  special case of it. A delete can free frames as it goes; an exchange cannot free
  anything, because every child of one tag is still live in the other, so it has to
  move values between two existing subtrees without an intermediate state in which
  either is short a member. This is the requirement that most constrains the store's
  internals, and it is why the two are listed separately: an implementation that has
  a subtree delete and assumes a swap can be built from it will discover the
  allocation problem at the point where it cannot free.
- **Generation-correct deletion.** A membership delete must name the client
  `entry_ref` including its generation, or it will happily delete a new client's
  membership after a slot is recycled. This is `configstorage.md` §0.1's rule
  applied to a new operation, and it is the reason the membership child stores an
  `entry_ref` rather than a bare `entry_id`. The exchange needs the same care in a
  different place: a membership child naming a client that was destroyed and whose
  slot has since been recycled must not be allowed to carry the stale half across,
  so the exchange either validates both sides' children or refuses, and refusing is
  the correct answer for a swap because a swap with a dead member in it is already
  a request the caller got wrong.

**A reverse index is still not required, and that is now a stronger statement than
it was.** "Which tags is this client on" is answered by the derived value in §5, and
under §3 it is answered per monitor, so the question a switcher asks decomposes into
"which of *this monitor's* tags name this client", which is a walk of one prefix.
The earlier version of this section offered a choice between an index and an
accepted scan; with the monitor half of the identity structural, the prefix walk is
the answer and the index is a optimisation that nothing currently requires.

## 8. Overview and the scratchpad are tags, and why they are the two exceptions

`generaldesign.md` §6 makes overview a special tag that displays every tag's
windows at once, and makes the scratchpad a special tag whose windows are hidden
until summoned. Under §1 both are tags. They are not the same shape of special,
and the difference is worth separating up front, because an earlier version of this
section treated them as a matched pair and got the scratchpad wrong in three ways
at once: it made both of them per-monitor as a forced consequence, it left the
scratchpad with no layout of its own, and it left the scratchpad participating in
the union rule that §4 defines. All three are corrected below.

### 8.1 Overview is the degenerate case, and it is per monitor

An overview monitor's list contains that monitor's overview tag, and the overview
tag's own client set is the
union of every other tag *on that monitor's*, so the union of the list is the union
of everything on that monitor. Note the scoping: it is the monitor's own tags, not
every tag in the instance, which under the old global model was the same statement
and here is a narrower one. A monitor's overview shows that monitor's windows and
not the ones parked on a second output.

Overview is therefore the primary, its layout is whatever arranges an overview,
and it needs no special case in the solver at all. That is worth having: it means
the arrangement code has exactly one input shape. The one thing §3.1's
*at most one* rule does not disturb is that each monitor has its own overview tag
as a per-monitor slot, because there is nothing about overview that wants to be
shared; a user who wants to see monitor 2's windows switches to monitor 2.

### 8.2 The scratchpad is its own thing, and it is configurable

**The scratchpad is a tag under the hood, and it is exclusive to itself.** No other
tag *arranges* it, and this is a departure from overview: overview works by being
the degenerate case of §4's union, and the scratchpad works by being *outside* the
union entirely, contributing nothing to any arrangement of any monitor's list and
being arranged by nothing but its own layout. A scratchpad tag in a monitor's list
would not have its members placed by that monitor's primary layout, because §4's
rule for a secondary tag does not apply to it. It is a tag, so it is stored, saved,
swapped, and membership-indexed exactly as §5 says, and it is not a tag, so it takes
no part in §4's union.

**"Exclusive" is about arrangement, not about membership, and the distinction is
load-bearing because the alternative does not implement.** A client in the scratchpad
usually *is* a member of an ordinary tag as well, and it stays one. What the
scratchpad excludes is that client's **participation in another tag's layout**: a
solve places a client from the set it computes, and a client holding scratchpad
membership is skipped by every set but the scratchpad's. Deleting the client's
ordinary membership instead would look stricter and would be unimplementable,
because dismissal then has to put the client back somewhere and nothing records
where. Mango needs a place to record it — `show_scratchpad` saves the old tag mask
in `oldtags` and reassigns the client's tags on the way in and out — and that
second copy of the truth is exactly the hidden state this design is built to avoid
(`configstorage.md` §1's scope taxonomy, and the whole reason a tag is a container
rather than a bit). So the rule is: **membership is retained, participation is
withheld**, and a client leaves the scratchpad by clearing the scratchpad's
membership child, at which point it is already a member of the tag it reappears on
and needs no restoring.

The one thing that *is* a membership mutation is the operation of putting a client
into the scratchpad, which adds a second tag to its membership rather than moving
it, and that is what §8.4 and `swap_tags` in bulk are for.

**The scratchpad is shared or per monitor, and that is configuration rather than a
modelling fork.** Both options are supported, and the difference is only how many
scratchpad tags exist and where their entries live:

- **Shared**: one scratchpad tag for the whole compositor, a singleton under §3.1
  with no monitor in its identity. It can be opened on any monitor, and the
  invariant is that it is open on **at most one** at a time. Summoning it on a
  second monitor while a first still has it open is refused, and the refusal is
  §3.1's mirroring argument applied to a tag that is not pinned to a monitor: a
  client in it would be asked to present on two outputs.
- **Per monitor**: one scratchpad tag per monitor, each a per-monitor slot exactly
  like any other tag on that monitor. Each is opened and closed independently, and
  the "one at a time" rule needs no special case, because opening two of them shows
  two different tags holding different clients rather than one tag twice. A client
  parked on monitor 1's scratchpad is not in monitor 2's, and §7's swap is how it
  gets there.

This is the Mango behaviour the reference is cited for: in Mango the scratchpad is
a single thing that can be summoned on whichever monitor you are on, which is the
shared option, and the per-monitor option is the same machinery with N of them.
Because §3.1 states the invariant in terms of display rather than identity, both
options are the same code path and neither is a special case in the store, the
solver, or the membership rules.

**The shared-or-per-monitor choice is exposed as a high-level config key, and it is
also reachable directly through SHM.** A user writing a TOML config sets one
boolean and never touches either; a user driving the compositor over IPC or a
script can arrange the scratchpad by hand, because a scratchpad is an ordinary tag
entry and ordinary tag entries are writable. Both routes reach the same state,
which is the point of §5's decision to make a tag a catalog entry: there is no
privileged path and no second representation.

| key | type | default | meaning |
|---|---|---|---|
| `wm.scratchpad.shared` | `bool` | `true` | one scratchpad for the whole compositor, openable on at most one monitor at a time, rather than one per monitor |

The default is `true` because it is the Mango behaviour and because a single
scratchpad is what almost every user wants: a parked window is a thing you recall
wherever you happen to be, and per-monitor scratchpads are the case where someone
is deliberately keeping separate parking lots. The key is a plain option key with a
static default under the same rule as every other option, so an absent key and a
key set to `true` are the same state, and the value is never seeded into the block
(`configstorage.md` §12 has no semantic tier; the default is the consumer's).
Changing it while a client is parked is a user error rather than a silent
rearrangement, and the choice of what to do with parked clients in that case is
small enough to belong to whoever implements the key.

Note what this key is *not*: it does not create or destroy tags. It selects which
of two already-described arrangements is in use, so flipping it does not orphan the
other arrangement's contents by surprise — a user moving from shared to per-monitor
gets a fresh per-monitor scratchpad, which is empty, and the old shared scratchpad
is still a tag they can swap out of. That is a consequence of §8.3's position that
membership and tag entries are ordinary store data, and it is why no migration code
is needed for the flip.

### 8.3 The scratchpad has a layout, and it opens as an overlay

**The scratchpad is an overlay workspace, not a hidden hole.** It holds a layout
like any other tag, and summoning it places its clients by that layout *over the top
of the monitor's existing view* rather than replacing it. This is the correction to
the earlier version of this section, which described the scratchpad only in terms
of what was *absent* — a client "in no visible scope", kept out of every layout by a
per-client visibility state — and never said what happened when it appeared. That
description made the scratchpad a gap in the arrangement rather than an arrangement
of its own, and it is why §7.7's requirement that a client can be in a client set
and outside every layout was being satisfied by a special case.

With a layout, the requirement is satisfied structurally. The scratchpad is a
client set with a layout attached, so the engine solves it the same way it solves
any tag, and the only thing that is different is *when* it is solved and *what it is
solved over*. The engine needs no knowledge of "overlay" to arrange the clients; the
overlay is a placement of one solved set on top of another already-solved set, which
is a composition of two arrangements rather than a third kind of arrangement.

**Minimised state and the scratchpad are related but not the same question, and the
relationship is now explicit.** A minimised client is not in the scratchpad; it is a
member of whatever tag it lives on, with a per-client visibility state keeping it
out of that tag's arrangement. So there are two distinct mechanisms that keep a
client off screen: the scratchpad, which is a different tag with a different
layout, and minimisation, which is a flag on a client in a tag whose layout would
otherwise place it. Both are needed and neither subsumes the other. The earlier
text conflated them by treating the scratchpad as the home of minimised clients,
which was wrong: a user who minimises a window wants it back in place on its own
tag, and routing that through the scratchpad would move it out of its tag to get it
out of view.

### 8.4 Focus, hiding and minimisation are three operations, not one

**Focus is not a membership mutation, whatever it looks like.**
`layoutengine.md` §7.7 notes that focusing a minimised client un-minimises it, and
the previous version of this section got that wrong twice over: it described focus
as deleting the scratchpad's membership child, and it described the un-minimise as
a separate write. Both are wrong for the same reason, which is that a client has no
second copy of its own state to keep in step. The rule is that a client can be a
member of the scratchpad and be focused, and being focused is what brings it onto
the overlay; **the membership child is not touched by the focus event.**

**Hiding into the scratchpad is the membership mutation, and it is an addition.**
Under §8.2 the client keeps the tags it already has and gains the scratchpad, so
"hide" is not a move and nothing is recorded for the way back. That is what makes
dismissal a single membership deletion, and it is also why the two operations are
worth separating by name: a user who hides a window and then focuses it expects it
to appear, not to be un-hidden somewhere else. `swap_tags` is the bulk form of the
same thing (§7): one swap exchanges a whole tag's worth of clients with the
scratchpad without the user naming any client, which is why the operation needed a
name and the focus path did not.

## 9. Not covered here, and what is still open

Not covered, deliberately, and listed so the omission is a decision:

- What a tag looks like: names, colours, icons, ordering in a bar.
- Tag rules, and per-output tag defaults. These are `generaldesign.md` §6 promises
  that need a document of their own; they do not bear on the layout engine.
- The surface syntax for setting, unsetting and exclusively setting a tag, which
  belongs to `input.md` and `helpers.md` §11.

Covered here rather than elsewhere, and listed because the placement is a
decision: a monitor's ordered tag list is stored at `wm.monitor.<id>.tags` (§5)
rather than in a monitor document of its own, because no `monitor.md` exists yet
and a promise to hand storage to a document that has not been written is how a tag
list ends up owned by nobody. The ordering semantics are this document's and the
storage follows them, so holding both here is the lower-risk arrangement until a
monitor document exists to take it. A monitor document, when there is one, reads
these two rather than being cited by them.

**Nothing in either group above, or in the list below, is a question any more.** The
one item `layoutengine.md` §11 used to carry, stored or derived membership, is the
same question the first entry below used to carry and is closed by the same
decision; `layoutengine.md` §11 agrees and does not reopen it. What is still
genuinely undesigned is the bar-side surface — what a tag is *called* in a user
interface, its colour, its icon, and the order the bar draws them in —
which is a real gap and is phase 05's deliverable rather than an open decision in
this document.

Open, and the ones that bear on the layout engine:

- ~~**Stored or derived membership.**~~ **Closed: membership is stored on the tag.**
  §5 says so and says where, and `layoutengine.md` §3.2's determinism requirement is
  the reason rather than an accident of the layout: a derived membership is a
  `HashSet`-shaped iteration order, which is not reproducible, so a golden-file
  solver test could not exist. Storing it costs a write per membership change and
  buys a solve that reads a list in `entry_id` order, which is the same order
  `windows.md` §4 walks. `layoutengine.md` §7.7 asks the same question about
  client-set membership more generally and is closed by pointing here, so the two
  documents cannot disagree.
- ~~**Where a per-tag layout's seed lives for a multi-tag client.**~~ *Closed, and
  the question was malformed.* A layout program names no window, so there is no
  seed to place: the tag stores a layout name, the program is fetched from SHM by
  that name, and a rearrange re-solves from it. A client on three tags has one
  copy of its own state and the program is the same one, so there is nothing to
  scope per tag. `layoutengine.md` §3.1.
- ~~**Whether a tag's own entry is a container or a value.**~~ **Closed: it is a
  container, and the subtree concept is therefore required.** §5 makes the entry a
  container with children, and §7 names the three store operations that follow,
  with the subtree exchange the one that is not implied by the other two. Nothing
  is deferred; the question was real and the answer is that the store grows the
  concept, rather than the question being parked until something else forced it.
- ~~**The surface form of the swap operation.**~~ **Closed: two full keypaths.**
  `ipc.md` §4's `swap_tags` takes `a` and `b` as complete
  `wm.monitor.<m>.tag.<n>` keypaths, rather than as a pair of monitor/number
  fields, and the reason is that it reuses the one addressing rule the rest of the
  protocol already has instead of adding a second way to name a tag. That choice
  also pays for itself on the singleton: a keypath says `wm.tag.scratchpad` in the
  same field where it says `wm.monitor.1.tag.3`, so §5's two tag shapes need no
  argument-order convention and no separate command, which a monitor/number pair
  would have required.

## 10. References

| reference | use |
|---|---|
| `generaldesign.md` §6 | the per-monitor Tag entity this document gives an identity and a storage shape to, the departure from Mango's bitmask, and the content swap that replaces Hyprland's workspace move |
| `generaldesign.md` §7 | layout is a solver, the built-in boundary, and the per-tag-per-monitor and floating wording §4 amends |
| `generaldesign.md` §8 | groups, clusters, window rules, and the floating layer, which §6 confirms belongs to the client |
| `configstorage.md` §0, §0.1 | the open catalog, and the identity vocabulary including the generation rule |
| `configstorage.md` §3, §12, §13 | `WINDOW_DEPENDENT` scope, and the `save` and reset classification |
| `configstorage.md` §14.1 | the three store operations §5 and §7 oblige, including the subtree exchange |
| `configstorelayout.md` §6, §11 | the catalog entry, and the solved layout node keyed by `entry_id` and `entry_generation` |
| `ipc.md` §4 | `swap_tags`, the one-command grouped-commit exchange §7 defines |
| `layoutengine.md` §3.1, §3.5, §7.7 | the questions this document answers, and the ones it does not |
| `omni_layout.h` | `OMNI_SOLVED_NODE_OFF_REF_ID` and `OMNI_SOLVED_NODE_OFF_REF_GEN`, the entry-scoped identity the solver publishes, which §2 relies on |
| Mango | tags as bit positions in a 32-bit mask, which §3 and §5 reject, and the per-monitor state array indexed by tag number, which §3 **keeps** and makes the identity; also client-scoped values stored under a client's entry, which §5 and `windows.md` follow |
| Hyprland and i3 | exchanging two workspaces' contents, which §7 keeps; the *moving* workspace, which §3 and §7 no longer have a use for |
| dwl | a monitor displaying a set of tags and mirroring one across outputs, which §4 sharpens into an ordered list with a head and then drops the mirroring from |
