# Tags

The Tag entity, its identity, what it owns, and how a monitor displays tags. This
document exists to answer the questions `layoutengine.md` raises about tags, and
it deliberately stops short of the whole tag feature. What a tag looks like to a
user, tag colours, tag icons, per-output tag defaults and tag rules are not
covered here; they are listed as not covered in §9 so the omission is a decision
rather than a gap.

## 1. What a tag is

`generaldesign.md` §6 makes a tag a first-class entity that owns its client set,
its own layout state, and the other data needed to view it, and that can be moved
to another monitor carrying its clients and its state with it. It does not own
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
A tag is therefore a catalog entry, and a tag is referred to by
`(entry_id, entry_generation)` through the existing `entry_ref` tag at `0x23`.

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

## 3. A tag's identity is not its name, and not its number

Both of those are conveniences a user interface may present, and neither is
identity, for the reason `configstorage.md` §0.1 already gives: a recycled slot is
never accepted as the same object without its generation.

Tag numbers are worth calling out because they are the thing Mango gets wrong and
`generaldesign.md` §6 explicitly departs from. A tag is not a bit position, a tag
is not an index into a per-monitor array, and a layout is never stored against a
tag number. The order a monitor's tags appear in is a property of that monitor's
display list, not a property of any tag, so it is not part of tag identity and
moving a tag between monitors does not renumber anything.

## 4. A monitor shows an ordered list of tags, and the head is the primary

This is the part `generaldesign.md` §6 does not say and the part the layout engine
most needs, so it is stated plainly.

A monitor does not display a *set* of tags. It displays an **ordered list**, and
the order is chronological: a tag is placed in the list when it is set on that
monitor, and the list is not re-sorted afterwards. The first tag in the list that
is still set is the monitor's **primary tag**. Every other tag in the list is
**secondary**.

So:

| action | result |
|---|---|
| set 3, then 4, then 5 | list is 3, 4, 5; primary is 3 |
| set 3, then 5, then 4 | list is 3, 5, 4; primary is 3 |
| unset 3 from 3, 5, 4 | list is 5, 4; primary is 5 |
| exclusively set 5, then set 3, then 4 | list is 5, 3, 4; primary is 5 |

The order is chronological and not numeric, so a lower tag set later never
displaces a higher one set earlier. The primary is the **head of the list**, not
the lowest tag number and not the most recently focused tag.

The primary is **derived from list order rather than stored as its own field.**
That is a deliberate choice and it is the reason the rule is stated in terms of
order at all. A separately stored primary is a second fact that can disagree with
the list, and it would have to be repaired on every set, unset, exclusive-set,
move and monitor-unplug. Deriving it means there is nothing to keep consistent,
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
  floating layer" is wrong in both of its first terms and needs amending.** Per tag
  per monitor implies a layout state exists for each pairing of tag and monitor.
  It does not. There is one layout per tag, and a monitor's arrangement is
  determined by its tag list together with which tag is at the head. The floating
  layer is not per tag either, and an earlier version of this document said it
  was; it is per client (§6), which is where `generaldesign.md` §8 already put it.
  Both corrections change how `layoutengine.md` §2.4 counts the engine's scopes.
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
  except where it is placed.

One case the list order used to make visible, and which `layoutlanguage.md` §3.8 has
reopened: whether a **cluster can cross a tag boundary**. A cluster is a movement
relationship, and it used to be defined as windows that each keep their own place in
the parent layout, which made a straddling cluster coherent by definition: one layout
placed some of the members while they travelled with the others. A cluster now
occupies one space of its parent instead, and a space belongs to exactly one layout,
so the straddling case has no obvious answer. Either the cluster as a unit belongs to
one tag and the other tag's window sits inside a space the first tag placed, or
crossing is refused. The first contradicts the solve input in `layoutengine.md` §2,
which takes the union of the client sets of the monitor's tags, so the second looks
more likely, but this has not been decided and it is not this document's to decide.

## 5. What a tag stores, and where membership lives

A tag is a catalog entry, so its parts are children of that entry, and the
`layoutengine.md` §4.3 rule that a name is a value applies to each of them.

| child | value | notes |
|---|---|---|
| the layout | a layout name, `0x0D` string | a name from the closed set, not a slot index |
| each member | a client `entry_ref` | see below |
| viewport and other data | per whatever it turns out to need | not designed here |

**Membership is stored on the tag, as one child entry per member client.** The
alternative, storing a back-reference on each client, is what makes the "which
tags is this client on" question answerable directly, and the direction matters
because the two have very different access patterns. The layout engine needs the
union of the client sets of a monitor's whole tag list on every solve, and solves
are frequent, so that direction must not be a scan of every client in the
compositor. The reverse question is asked rarely, by a switcher, so it can afford
to be a scan or a secondary index. Storing membership on the tag makes the hot
direction a walk of only the tags actually displayed.

This also makes multi-tag clients fall out rather than needing a feature: a client
with three tags has three member entries pointing at it, and the same client
`entry_ref` appears in three different unions. Nothing needs to know how many tags
a client is on.

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
rule seeds it by the same match-and-replace mechanism §3.3 gives for every other
client value, the rule is ordinary configuration and is saved, and on restart it
re-applies to the new client entry. Seeded state does not need to be saved twice.

## 7. Moving a tag, and what has to be true of the store

`generaldesign.md` §6 requires that a tag can be moved to another monitor carrying
its clients and its state with it, and that this is what makes moving a tag a move
of an entity rather than a reassignment of a number. Under §4 the move is: remove
the tag from the source monitor's list, append it to the destination's list, and
change nothing else. The clients travel because the membership children name the
client `entry_ref` and are unaffected by which monitor displays the tag. The
layout travels because it is a child of the tag. Nothing has to be rewritten
per monitor, which is the whole benefit and the reason §4's rejection of per tag
per monitor state matters.

Appending is the right choice over inserting at the head, and it is worth being
explicit that it is a choice: appending means a tag moved onto an empty monitor
becomes that monitor's primary, which is almost certainly what is wanted, and it
means a tag moved onto a busy monitor does not silently steal the arrangement.

Three requirements this places on the store, none of which exist yet, and all of
which are consequences of §5's membership shape rather than of tags as a concept:

- **A subtree delete.** Destroying a client must remove its membership children
  from every tag it is on. `configstorage.md` §8 classifies `save` and soft reset
  by flag, which sidesteps this entirely, but client teardown cannot: it is a
  delete of every entry beneath a prefix. Without it, a destroyed client leaves
  membership entries naming a recycled `entry_id`, and the next client to land in
  that slot appears on the old client's tags.
- **A reverse index, or an accepted scan.** "Which tags is this client on" is not
  answered by the membership shape in §5. It can be an index over membership
  children, or a scan, or a back-reference on the client kept as a cache.
- **Generation-correct deletion.** A membership delete must name the client
  `entry_ref` including its generation, or it will happily delete a new client's
  membership after a slot is recycled. This is `configstorage.md` §0.1's rule
  applied to a new operation, and it is the reason the membership child stores an
  `entry_ref` rather than a bare `entry_id`.

## 8. Overview and scratchpad are tags, and that is why they are special

`generaldesign.md` §6 makes overview a special tag that displays every tag's
windows at once, and makes the scratchpad a special tag whose windows are hidden
until summoned. Under §1 both are tags, and their specialness is a property of
what a monitor's list contains rather than a different kind of entity.

Overview is the degenerate case of §4, and it is a good test of the rule. An
overview monitor's list contains overview, and overview's own client set is the
union of every other tag's, so the union of the list is the union of everything.
Overview is therefore the primary, its layout is whatever arranges an overview,
and it needs no special case in the solver at all. That is worth having: it means
the arrangement code has exactly one input shape.

The scratchpad is the case that is not degenerate, and it is where
`layoutengine.md` §7.7's minimized client lands. A minimized client is on its tag,
so it is in the client set, and it is in no visible scope, so it is not placed.
Under §1 that is expressible as: the client is a member of the tag, and a
per-client visibility state keeps it out of every visible scope. The
requirement §7.7 states, that a client can be in a client set and outside every
layout simultaneously without either implying the other, is satisfied by §1
without a special case, and that is the outcome to check this document against.

Focus is an ejection. `layoutengine.md` §7.7 notes that focusing a minimized
client kicks it out of the scratchpad, which is a mutation of membership: the
membership child for that client is deleted from the scratchpad tag. This document
owns that mutation because §5 owns membership.

## 9. Not covered here, and what is still open

Not covered, deliberately, and listed so the omission is a decision:

- What a tag looks like: names, colours, icons, ordering in a bar.
- Tag rules, and per-output tag defaults. These are `generaldesign.md` §6 promises
  that need a document of their own; they do not bear on the layout engine.
- The surface syntax for setting, unsetting and exclusively setting a tag, which
  belongs to `input.md` and `helpers.md` §11.
- Where a monitor's ordered tag list is stored, which is `monitor.md`'s to own.
  This document defines the ordering semantics; the storage is deliberately left
  there so that the list and the monitor's other per-output state are not split
  across two documents.

Open, and the ones that bear on the layout engine:

- **Stored or derived membership.** §5 recommends stored on the tag, on access
  pattern grounds. `layoutengine.md` §7.7 asks the same question about client-set
  membership generally and is the document that has to close it, because §3.2
  makes the solve deterministic and a `HashSet`-shaped derivation is a
  non-reproducible iteration order.
- ~~**Where a per-tag layout's seed lives for a multi-tag client.**~~ *Closed, and
  the question was malformed.* A layout program names no window, so there is no
  seed to place: the tag stores a layout name, the program is fetched from SHM by
  that name, and a rearrange re-solves from it. A client on three tags has one
  copy of its own state and the program is the same one, so there is nothing to
  scope per tag. `layoutengine.md` §3.1.
- **Whether a tag's own entry is a container or a value.** §2 and §5 assume
  children. If the store grows no subtree concept, this is the decision that
  forces one, and §7's three requirements are the evidence for it.

## 10. References

| reference | use |
|---|---|
| `generaldesign.md` §6 | the first-class Tag entity this document gives an identity to, and the departure from Mango's bit-position tags |
| `generaldesign.md` §7 | layout is a solver, the built-in boundary, and the per tag per monitor and floating wording §4 amends |
| `generaldesign.md` §8 | groups, clusters, window rules, and the floating layer, which §6 confirms belongs to the client |
| `configstorage.md` §0, §0.1 | the open catalog, and the identity vocabulary including the generation rule |
| `configstorage.md` §8 | `WINDOW_DEPENDENT` scope, and the `save` and reset classification |
| `configstorelayout.md` §6, §11 | the catalog entry, and the solved layout node keyed by `entry_id` and `entry_generation` |
| `layoutengine.md` §3.1, §3.5, §7.7 | the questions this document answers, and the ones it does not |
| `omni_layout.h` | `OMNI_SOLVED_NODE_OFF_REF_ID` and `OMNI_SOLVED_NODE_OFF_REF_GEN`, the entry-scoped identity the solver publishes, which §2 relies on |
| Mango | tags as bit positions in a 32-bit mask with per-monitor indexed state, which §3 rejects; and client-scoped values stored under a client's entry, which §5 and `windows.md` follow |
| Hyprland | a workspace as a movable entity that carries its clients and state between monitors, which §7 depends on |
| dwl | a monitor displaying a set of tags, which §4 sharpens into an ordered list with a head |
