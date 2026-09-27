# The layout language

The normative reference for how a layout is written: the model, the ten rule
kinds, the keys at each level, the guards a program must pass, and the limits that
are frozen. Where `devnotes/layoutengine.md` records what was decided and why, and
`devnotes/tomlparser.md` records how the bytes are read, this file records what the
language *is*. A program written to this file is a program the engine can solve.

The companion suite is `layout-tests.md`: eighteen mock designs, one per idea worth
testing, written to find places where this language is awkward, ambiguous, or missing
a key. Its findings are the evidence behind the decisions here, and a rule in this
file that looks arbitrary usually has a numbered finding explaining it. The suite is
not itself normative; where the two disagree, this file is right and the suite is a
bug report.

Every key that has been removed is listed in §11.10 with the reason, so a config
written against an earlier draft of this language can be read rather than guessed at.

## 1. What a program is

A program is **four keys** under one prefix, and this document and
`layout-test-examples.md` are the authority on keys. `rules`, `spaces` and
`viewport` are the three the 1,138-line suite is written in throughout, and
`rearrange_on_focus` is the fourth, added because a focus change is an arrange
trigger whose answer differs per layout and belongs in the layout rather than in a
compositor-wide setting (§3.0). A suite written without the fourth key is
conforming, because the key is optional and defaults.

```
omniwm.layouts.<name>.rules              array of rule
omniwm.layouts.<name>.spaces             array of space
omniwm.layouts.<name>.viewport           the viewport, at most one
omniwm.layouts.<name>.rearrange_on_focus boolean, at most one
```

All four are optional, and each missing one falls back to a static default in the
consumer's code, independently of the others. "Optional" is a statement about
this syntax and nothing more: a program does not have to be complete to be valid,
and the block never holds a copy of a default in place of a key the author left
out. §11.1 has the per-key rule and `tomlparser.md` §11 says why the default is
code rather than a stored value.

The tag is read off the key rather than declared once, which is the same rule
`tomlparser.md` §3 states for integers. `spaces` is an `OMNI_TAG_CONSTRAINT` at
`0x32`; `rules` is an array of `option`, one per rule, because a rule is a
named-key table of its own kind's arguments; `viewport` is an `option`. Three
parts:

| part | role |
|---|---|
| `rules` | every rule, of every kind, **the one rule list the program has** |
| `spaces` | the spaces, each with a rectangle |
| `viewport` | that the solved geometry is viewed through a transform |

A program has one rule list, at program level, and it applies to all of that program's
spaces. There is no `groups` part and no `group` field; §5 explains what replaced them
and why a rule list is a program, so a program wanting a second one nests.

`spaces` and `rules` are the two things a program always has. All three of the
structural keys absent is a **blank program**: valid, and the blank canvas of
`devnotes/layoutengine.md` §7.8. The fourth key does not enter into it, because a
program that does not write `rearrange_on_focus` is a program that wants the
default `true`, and the default is indistinguishable from having written it.

## 2. The model

**Four nouns. An earlier draft used `group` for two of them, which is why the
word needed a table this badly.**

| word | means | declared by |
|---|---|---|
| program | a named layout, one prefix of `omniwm.layouts`, holding **one** rule list | `omniwm.layouts.<name>.*` |
| space | one rectangle holding zero or one occupant | `spaces[]` |
| group | a space carrying a `layout`, so it holds a nested program | the space's `layout` field |
| cluster | a runtime set of clients occupying one space as one occupant | §3.8 |

A group is a space, not a kind of space, and it obeys the parent's arrangement like any
other member. A group is also the *only* nesting mechanism: a program that needs two
arrangements in two regions writes two groups and lets each name a layout, §5. A `groups[]` key, an array of named
rule sets inside one program, is not part of the language: it duplicated nesting and
had no rectangle of its own, §5.

**A space is a slot holding zero or one occupant, and its rectangle is computed by an
arrangement unless the entry pins part of it.** An occupant is a client, or a cluster
of clients, and the second case is why §3.8 exists. There is no `capacity`; full means
something occupies it, which is a fact about the state rather than a declared number.

**A space with a `layout` holds a program.** That is the whole test, and it is an
inference rather than an assertion: the presence of the field is the claim. The
nested program has its own groups and its own spaces, and this file refers to them
only by name, so nesting is a reference to another program and never a link in this
program's data. The
direction is always the same, so following it needs no parent chain, and its depth
is the one `devnotes/layoutengine.md` §3.6 already bounds at
`OMNI_LAYOUT_MAX_NEST_DEPTH`.

**A group's rules decide everything about it.** How its members are placed, what
happens when one fills, and any relation between named members are all rules, in
one list, and the list order is significant.

**There are two rule layers, and they answer different questions.** A *program*
rule places a program's spaces: `run`, `grid`, `split`, `share` and the rest of
§3. A *client* rule places a client inside a box it has been handed: `align`,
`match`, `size`, `offset` and `snap`, §3.6. Both are lists of rules written the
same way, and both are soft weighted relations where a relation is involved.

This reverses an earlier conclusion rather than extending it. Putting the weighted
constraint system in the program and finding the relations vestigial was wrong,
because the layouts that seemed not to need one did need one. They were
vestigial *there*. The mechanism is real, and it belongs on the client, because the
thing that needs relating to something else is a client, and a group bar is a fake
client carrying three client rules that the program never hears about.

## 3. Rules

Every rule is a table. `rule` names its kind and is required. The kinds are:

| `rule` | family | trigger | meaning |
|---|---|---|---|
| `run` | arrangement | none | lay members out in a line along an axis |
| `stack` | arrangement | none | every member fills the group, coincident |
| `grid` | arrangement | none | lay members out row-major into a grid |
| `scatter` | arrangement | none | members take the rectangles their clients carry |
| `split` | occupancy | full | divide the full member's rectangle in two |
| `spawn` | occupancy | full | add a member and give it to the leaver |
| `push` | occupancy | full | move the leaver to another space |
| `float` | occupancy | full | the leaver leaves the group entirely |
| `share` | geometry | none | the subject takes a fraction of the program |
| `inset` | geometry | none | the subject's edges pull in by a distance |

A group has **at most one** arrangement rule and it must come first. Occupancy and
geometry rules follow, in any order, and there may be several of each.

**The family is the trigger, and there is no `when` key.** An occupancy rule fires when
a member is full; an arrangement or geometry rule applies on every arrange. The `rule`
value already says which kind a rule is, so `when = "full"` on all four occupancy kinds
restated something the reader can infer, and a key that can only be written one way
carries nothing. §8 records where a second trigger would go.

**Rules are tried in order and the first that fits wins.** There is no `priority`
and no weight band. A later `share` cannot override an earlier `inset`, and to
change the order you move the rule. This is both the readable form and the honest
one: the duckWM weights existed to make a least-squares solve converge, and a
layout with four geometry rules does not need a solver to decide between them.

### 3.0 Whether a focus change re-arranges

A focus change is an arrange trigger, and whether it is one is a property of the
layout rather than a compositor-wide setting, because the two common answers are
both right for different layouts. A `stack` wants it: `raise` in §3.1 offsets the
focused member, so a focus change that did not re-arrange would leave the offset
pointing at the member that used to be focused. A `run` or a `grid` does not want
it, because their members' rectangles do not depend on which one is focused and a
re-arrange would be a pointless pass on every Tab.

So it is a program-level key, and it is the fourth key at that level:

| key | type | present | default | meaning |
|---|---|---|---|---|
| `rearrange_on_focus` | boolean | at most one | `true` | whether a focus change runs a pass |

`true` is the default because the case that needs it is the case that silently
breaks without it: a `stack` whose `raise` does not follow focus is wrong in a way
a user reads as a compositor bug rather than as a layout setting. A layout that
does not want the pass writes `rearrange_on_focus = false` once and stops paying
for it, which is the cheaper direction to be wrong in.

The key is at program level and not on the arrangement rule because the trigger is
a property of the pass, not of one rule. A program with two groups in different
regions may reasonably want one to follow focus and the other not, and that is
expressible by nesting: the group that should not follow focus is a space carrying
its own `layout`, and the nested program sets the key. The general case is
therefore available without the flat case needing its own rule.

Being program level also means it is one of the keys subject to §11.1's per-key
default rule, so an absent key is the compiled-in `true` and no block ever carries
a seeded copy of it.

### 3.1 Arrangement arguments

| argument | applies to | values | default | meaning |
|---|---|---|---|---|
| `axis` | `run`, `stack` | `x`, `y` | `x` | the axis the run follows |
| `gap` | `run`, `grid` | integer >= 0 | `0` | space between members |
| `leftover` | `grid` | `center`, `absorb` | `center` | what happens to the remainder |
| `columns` | `grid` | integer >= 1, or `sqrt`, §3.4 | `"sqrt"` | grid columns |
| `rows` | `grid` | integer >= 1, or `derived`, §3.4 | `"derived"` | grid rows |
| `raise` | `stack` | integer >= 0 | `0` | offset applied to the focused member |
| `reflow` | `run` | `flow`, `sticky` | `flow` | whether members are re-placed each arrange, §3.3 |
| `self` | `run` | `parent`, `content` | `parent` | whether the group sizes to its rect or to its members |

`run` places members in order along `axis` and divides the axis equally between
them. A member's portion is changed by a `share` rule, not by an argument here, and
that is not a style preference: `share` is a rule kind and not an argument to
`run`, because having both is one concept written twice under two names.
`stack` gives every member the program's whole rectangle and offsets the focused one
by `raise` along `axis`. `grid` fills row-major into `columns` by `rows`, and
`leftover` is the whole difference between a grid that centres its empty cells and a
fair one whose clients cover the group exactly. `scatter` places nothing.

`scatter` is the one arrangement that asks the client rather than computing, and that
is the whole of it. A `scatter` member needs no key to say where its rectangle comes
from, because the arrangement already said so, and a `rect` key on the member would
be that sentence written a second time on every member. A
`scatter` member also may not pin any of the four components of §4, because there is
no computed rectangle for a pin to override.

`self = "content"` makes the group as long as its members need rather than as long
as its rect, which is what makes a scroller's content longer than its viewport. It
requires a `viewport` with `pan = true`, because a group that outgrows its rect is
only useful if something can be moved to see the rest of it.

`self` has two values, not three. A third, `"unbounded"`, said a group had no
extent along its axis, which is `"content"` plus an origin, and an origin is one fact
and belongs in one place. It is `viewport.virtual_origin`, §7, and a `run`
has no way to claim it.

### 3.2 Occupancy arguments

| argument | values | default | meaning |
|---|---|---|---|
| `keep` | `new`, `old` | required | who keeps the full space, the arriving client or the incumbent |

Plus `to`, a space address, required when `rule = "push"`. `keep` is the only other
argument an occupancy rule has, and unlike the trigger it is not inferable from the
kind, because `spawn` with `keep = "new"` and `keep = "old"` are two different layouts.

`to` is an address, and there are exactly two forms of it:

| form | example | means |
|---|---|---|
| a space of this program | `master`, `right` | that space |
| `<group>.<rest of the address inside it>` | `stack.main` | the space `main` of the program nested in group `stack` |
| `main.<n>` | `main.1` | the space this program creates at position `n` |

The second form is not a longer way of writing the first, because a group is not a
destination. A space that holds a nested program is a region containing spaces, and a
client arriving at a region is handed to that program's own mapping, §6. A `push` is
how a layout says *not* to do that, so the target has to be a space the arriving client
can occupy rather than the group it sits in. Naming the group first is the scoping: an
inner name is unique only inside its own group, so `main` alone is ambiguous the moment a
program nests two of them.

Which means the answer to "why not just `stack`" is that `stack` is the one name that
cannot be a `to` in that program, for the same reason `layout` is not a `to` anywhere.
It names a place that decides where clients go rather than a place a client goes.

A dotted address is the second form only when its first component is a space carrying
a `layout`, and that is decidable from the data rather than from the shape of the
string. `stack.main` is a path, because `stack` is a group and holds a program, and
`main.1` is not, because `main` is not a group of this program and `1` is a position.
The check is therefore left to right: name a space that carries a `layout` and keep
going inside it, otherwise read `main` as the position prefix, and anything else is not
an address.

`split` divides the full member's rectangle in two along the program's `axis` and
makes two members, the keeper taking one and the leaver the other. It creates no
group, and the result's shape is carried by the two rectangles. `spawn` adds a
member to the group and gives it to the leaver, so the member count grows and a
`flow` group re-places while a `sticky` one does not. `push` moves the leaver to
`to`, which must already exist, and changes no shape; if `to` is full, the rule of
the group owning `to` fires in turn, so a push into a spawning group terminates by
spawning. `float` removes the leaver from the group and it becomes a floating space
positioned by its own client, belonging to no group.

`to` may name a space inside a nested program, which is how a master pushes into the
stack it contains, and the address is read left to right rather than by its shape,
§3.2.

### 3.3 `flow` and `sticky`

`reflow = "flow"` is the default: the arrangement places every member on every
arrange, so a member that leaves lets the rest close up. A grid, a stack, a master
and stack, a deck and a scroller all want this, because their member counts change
and their geometry is a function of the count.

`reflow = "sticky"` means the arrangement places a member **once**, when it is
created, and the member keeps that rectangle, scaling proportionally with the group
on resize. This is dwindle, and §11 says what it costs.

A `split` rule therefore requires a `sticky` `run`, because the two halves of a
split cannot be re-derived from the member list without the history that produced
them, and the history is the parenting this design does not have.

### 3.4 Counts

A count is either a bare integer or a bare string, and the TOML type decides which,
not the content:

```toml
columns = 3        # exactly three
columns = "sqrt"   # derived from the member count
```

`sqrt` is the member count's square root, rounded up, minimum 1. `derived` is
`ceil(members / the other axis)`, valid only on whichever of `columns` and `rows` is
not itself derived.

This is the only key in the file that takes two forms, and it earns the exception:
a count is nearly always a fixed integer, so a fixed integer should be written
without ceremony. The alternative is an inline table, which is what the third
drafts used, and it collided with the rule-kind key `rule` for no benefit. Under
`devnotes/tomlparser.md` this is the same situation as §11's bare-numeric-string
question, resolved the same way: by TOML type, with both forms declared here.

Neither form is arithmetic, so no expression syntax is implied;
`devnotes/tomlparser.md` §11 records that the config language exposes none.

### 3.5 Geometry arguments

| argument | applies to | values | default | meaning |
|---|---|---|---|---|
| `subject` | both | space name | required | the space being placed |
| `axis` | `share` | `x`, `y`, `xy` | `xy` | the axis the relation applies to |
| `of` | `share` | number > 0 | required | the fraction, so `0.3` is three tenths |
| `by` | `inset` | integer >= 0 | required | the inset distance in pixels |
| `edges` | `inset` | array of `left`, `right`, `top`, `bottom` | all four | which edges pull in |

Each geometry rule takes the subject's **current** rectangle and adjusts it. The
arrangement rule placed it first; these refine it. That is what lets `share` and
`inset` compose as share-then-inset instead of competing, and it is the reason a
geometry rule is stated in terms of the program's rect only where it must be.

- `share` sets the subject's extent on `axis` to `of` of the program's, which is why
  it is the only geometry rule that measures the whole rect.
- `inset` pulls the subject's current edges in by `by`.

**A subject is named by at most one geometry rule.** Two rules naming the same
subject is a load error, not a silent no-op, because a space has one rectangle and
letting the second writer be ignored would make the language depend on list order in
a way no author could see from the file. Rules about *different* subjects compose
freely and may appear in any order.

This is the one place where "first that fits wins" from §3 is deliberately *not*
how precedence works. Precedence there selects between rules describing the same
event; here it would select between two rules describing the same space, and
silently discarding one of them would be the wrong answer.

## 3.6 Client rules

A program places spaces. It does not place clients inside a space. A client that
occupies a space has the space's rectangle, and when that client is a cluster the
rectangle is the cluster's box and the members divide it between them. The rules
that do the dividing are on the clients.

A client rule is a table with a `rule` naming its kind. Five kinds:

| `rule` | arguments | meaning |
|---|---|---|
| `align` | `against`, `edge` | pin one edge or centre to the reference's same |
| `match` | `against`, `axis` | take the reference's extent on an axis |
| `size` | `width`, `height` | a fixed extent, overriding the reference |
| `offset` | `against`, `by` | a gap from the reference |
| `snap` | `against`, `region` | pin to a named region, §3.7 |

| argument | values | default | meaning |
|---|---|---|---|
| `against` | `client`, `viewport`, `output` | `client` | what the relation is measured from |
| `edge` | `left`, `right`, `top`, `bottom`, `center_x`, `center_y` | required | which edge or centre is pinned |
| `axis` | `x`, `y` | required | which extent is taken |
| `width`, `height` | integer >= 0 | none | a fixed extent in pixels |
| `by` | integer | `0` | a gap in pixels |
| `region` | the ten regions of §3.7 | required | where a `snap` puts the client |

Three client rules are a top bar, and this is the whole of a group bar:

```toml
[[omniwm.clients.bar.rules]]
rule = "align"
against = "client"
edge = "top"

[[omniwm.clients.bar.rules]]
rule = "match"
against = "client"
axis = "x"

[[omniwm.clients.bar.rules]]
rule = "size"
height = 50
```

`against = "client"` is the default and means the client this one is clustered with,
which after §3.8 is the group's virtual client. The bar is a fake client and it
takes part in layout and in focus exactly as a real window does, per
`generaldesign.md` §13, so nothing in this language marks it as anything else.

### 3.6.1 The two namespaces, and how a rule set is reached

Client rules live in two namespaces, because a set is reached in two ways and a
single flat namespace would not say which:

| namespace | path | reached by | what the name means |
|---|---|---|---|
| clients | `omniwm.clients.<set>.rules` | the fake client action that creates the surface | the set *is* the surface's identity |
| snaps | `omniwm.snaps.<set>.rules` | a drag region or a keybind, by name | the set is a *destination* a move can be sent to |

A fake client reaches its set the way `generaldesign.md` §13 says it is driven, which
is by an action rather than by a field on the surface: the action that creates a
surface names the set, so one action per surface kind and the set name is its
argument. There is deliberately no `kind` or `role` key on the set, because a field's
presence was the claim and naming the inferred category asserted it a second time,
and putting the name in the namespace lets a reader tell the two mechanisms apart
without a key that repeats what the path already said.

That reasoning is about fake clients, and `windows.md` §9.5 is where a real client
reaches the same namespace, so the two are worth telling apart rather than leaving a
reader to assume the table covers both. **A fake client has nothing to match**: the
action that creates it already names the set, and matching a compositor-drawn surface
against its own name would be a comparison that can only succeed. **A real client is
bound by a window rule matching it**, one named child per set on the client's entry,
because there is no creating action to name a set on its behalf. The syntax is
unchanged either way: what lands on the client is a `0x33` record from
`omniwm.clients.<set>.rules` in both cases, and the matcher selects *which* sets, not
what is in one. That is why `0x33` needs no matchers
(`configstorage.md` §4) and why the block encoding is the open ABI question there.
A set may be reached by more than one rule, so the order in which two bound sets
resolve the same client is the rule list's name order, sorted, with the naming doing
the work (`windows.md` §9.7) rather than a `priority` key: the language states no
priority, for the same reason it states no `role`.

A snap set is the "named area" that `layoutengine.md` §7.6 records wayfire needing,
reachable by drag and by keybind. A keybind or a drag region names the set, and the
rules apply to whichever client is being moved, so a snap set describes a place
rather than a thing. A destination has no parts, and in the suite that shows up as
shape: all twelve `snaps` sets hold exactly one `snap` rule and all five `clients`
sets hold three or four. The shape is a checkable consequence of the split rather
than a rule of the language, and every set in the suite now matches it. The one that
did not was `clients.canvas_marker`, a single snap against `viewport` and therefore a
canvas-relative `center` destination that `snaps.snap_center` already provides for
`output`. It was filed as a client because it was named as furniture, which was a
naming question and not a mechanism one, so it is now `snaps.snap_canvas_center` and
the shape is uniform.

The two share their keys, their solver and their weights, and nothing else. Nothing
in either namespace can be reached from a program, and a program cannot name a set.

Each rule is one soft equation and the set is solved together, which is where the
weighted least-squares solver earns its keep. This is why the program layer dropped
`priority` and the client layer keeps weights: a bar's three
rules cannot conflict with each other, so a priority between them would order
nothing, while a snap against a neighbour and a snap against the output can
conflict, and the joint solve is what decides where the client lands.

What that decide is not a winner, and it is worth being exact because the earlier
text here said the weight "decides which the user meant". Both of those snaps are
`OMNI_SOLVER_WEIGHT_CLIENT_RULE` (`layoutengine.md` §3.2, which is where both
weights live), so they carry the same weight and the solve compromises between
them; the weight separates a client relation from a *program* relation, not one
client rule from another. A client
holding both has asked for two positions at once, and the only way to get one of
them outright is to write one rule rather than two.

## 3.7 Snapping

`rule = "snap"` places a client against a region of a reference, and the regions
are the ones a Wayfire user expects:

| `region` | extent |
|---|---|
| `left` | left half, full height |
| `right` | right half, full height |
| `top` | top half, full width |
| `bottom` | bottom half, full width |
| `top_left` | top-left quarter |
| `top_right` | top-right quarter |
| `bottom_left` | bottom-left quarter |
| `bottom_right` | bottom-right quarter |
| `center` | middle half on both axes |
| `full` | the whole reference |

With `against = "output"` the reference is the monitor and these are the ten Wayfire
zones. With `against = "client"` the reference is a neighbour and `left` means "to
the left of that client", which is Wayfire's snap to a window. With `against =
"viewport"` it is the output unless the program carries a transform, in which case
the region is applied before the transform.

A snapped client is floating. It keeps a space, because a space is how a client has
an identity inside a layout, but the space's rectangle is replaced by the region's
and its group's arrangement rules are not applied to it. That is the path a floating
client already takes, so the animation endpoint, the saved record and the focus order
need no special case, and `rule = "float"` in a program is the same idea one level
up.

Snapping gets no space-side key on purpose. A snap belongs to a client at a moment,
not to a layout, and putting it in a program would mean a program's geometry changes
because a window moved.

## 3.8 Clusters and virtual clients

A cluster is a set of clients the layout treats as one. `generaldesign.md` §8
defined a cluster as a movement relationship whose members each keep their own place
in the parent layout, and that definition cannot express a group bar: if each member
keeps its own place then the bar is placed by the parent wherever the parent likes
and nothing puts it against its group's edge. So a cluster occupies one space, and
the difference from a group is where the membership comes from, not whether a
child layout exists.

| | group | cluster |
|---|---|---|
| membership | declared, static, the spaces of the program | runtime, the clients of a set |
| occupies | one space of its parent | one space of its parent |
| internal layout | its own program | its members' client rules, §3.6 |
| chrome of its own | none | none |

The solver resolves clusters before it maps anything, so a program only ever sees one
client per cluster:

1. Each member is asked for its **minimum box**, from its own client rules. A bar
   with `size = { height = 50 }` needs 50 pixels tall and nothing wide.
2. The cluster's **virtual client** has the union of its members' minimum boxes.
3. The program places virtual clients as its spaces, exactly as it places clients,
   because to the program they are clients.
4. The members are placed **inside** that space's rectangle by their client rules,
   and `against = "client"` now resolves, because the reference exists.

Step 1 before step 3 is the part that is not obvious, and it is why a bar makes its
group 50 pixels taller rather than overlapping it. It is also why a rule measuring
against a client contributes no minimum: it is waiting for a box only step 3 can
supply.

`devnotes/layoutengine.md`'s eight-step pipeline becomes nine, with the resolution
first:

    update -> resolve clusters -> map -> solve -> relax
           -> write inactive endpoint -> trigger -> animate -> arrange/activate

A cluster whose members cannot be satisfied inside the box step 3 gives it is a soft
failure: the weighted solve returns the least-bad placement and reports which rules
were unsatisfied, which is the answer the program layer already gives. Clustering a
client with itself, or forming a cycle of clusters, is a load error naming the
client, because there is no virtual client to compute.

## 4. `spaces`

An array of tables.

| field | values | default | meaning |
|---|---|---|---|
| `name` | string | required | unique within the program, bare, no trailing index |
| `x`, `y`, `w`, `h` | integer | `0` | **present pins that component**, §4.1 |
| `order` | integer >= 0 | `0` | rank, for `push` termination, §6.2 |

So a space is a name, whether it holds a nested program, an optional static rectangle,
and a rank. There is no `group` field on a space: a space either is a group, by
carrying a `layout`, or is a member of its program, and a `group` field on the space
said which of those it was without adding anything, §5.

### 4.1 Pinning, and why presence is the claim

`x`, `y`, `w` and `h` are optional and pin the component they name. The arrangement
places the space and then each pinned component replaces the computed one.

**Presence is the claim, not the value.** `x = 0` pins the origin to 0, and it is
*absence* that means "the arrangement decides". A key that defaulted to 0 and were
read as "0 means unspecified" could not express a space in the corner, which is the
one thing a static rectangle is usually for. This is the same rule the `layout` field
uses to mean "this space is a group", so it introduces no new convention: in this
language a field is a claim by being there.

Pinning is per component, not per rectangle, so the three cases that are actually
wanted each get one:

| written | result |
|---|---|
| nothing | the arrangement places the space, which is the normal case |
| `w = 300` | computed origin and height, static width: a sidebar |
| `x = 0`, `y = 0`, `w = 200`, `h = 40` | a fixed rectangle: a HUD corner |

A partially pinned rectangle is legal and the pinned components win, so a space with
a static origin and a computed size sits where it was put and grows to fit. It is not
a constraint and it is not checked for consistency with the arrangement, because the
whole point is to disagree with the arrangement.

A pin is a component of the space, not a client rule, so it survives a client joining
or leaving, and it is in the program's saved form rather than the client's. That is
the difference between a static layout decision and a decoration override, and it is
why the key is here and not in §3.6.

### 4.2 What a pin is not

An earlier draft made every one of these `0` in every program, and the zeros stood
for "the arrangement decides". That is the opposite of what the keys now mean, and
the ambiguity is why they were worth removing before they were worth keeping: read as
a value, a default of 0 and a pin of 0 are the same thing and a space in the corner is
unreachable. Read as presence, a program states geometry only where it means to, and
`stale capability tokens` in the check is what keeps the zeros from creeping back.

It is the same defect as the `hundredths` and `pixels` fields an earlier draft used,
which were the arrangement's output written in its input, and the difference is that a pin
is the author overriding the arrangement on purpose rather than restating it by
accident.

There is no `kind` field. A space with a `layout` is a group and a space without one
is not, which is the entire distinction and it is inferable, so asserting it separately
would be a second place for the two to disagree. A group is still an ordinary member of
its parent's arrangement, which is why the parent's `run` and `share` place it like
anything else and why a group is a space and not a kind of space.

There is no `role` field either, and there is no need for one. A `role = "chrome"`
marking compositor furniture was carried for a while, on the reasoning that a
layout might want to treat a bar differently from a window. `generaldesign.md` §13
already says a fake client takes part in layout and in focus exactly as a real window
does, so a bar *is* a window to this language and the key could only ever have
described a difference that is not permitted. What a bar needs is not a different
kind of space but three client rules, §3.6.

When a space's occupant is a cluster rather than a client, the space's rectangle is
the cluster's box from §3.8 and the members subdivide it. The space keeps one
rectangle either way, which is why the program needs to know nothing about this.

A group still has a rectangle: that rectangle is its place in *its* parent, placed by
the parent's rules like any other member. Being a group says nothing about how big it
is, and says nothing about the program inside it either, which sizes itself from the
rectangle it is given. `layoutengine.md` §3.6 says the same and says why it matters:
moving a group changes its rectangle and nothing about how its members are arranged
inside it.

**A declared space has a bare name, and a created space has an index.** The index
belongs to the system, so the author does not write it. A space created by `split` or
`spawn` is named `<group>.<n>`, where `n` is the position the new space takes in its
group, the declared members holding the positions before it. A group with one declared
space therefore creates `main.1` first, and a group with none creates `main.0`. The
implicit group is named `main`.

Earlier drafts made the author write the index on the declared space too, so a
program said `name = "main.0"` and the system agreed by starting at 1. That put the
system's numbering in the author's file for no gain, and worse, it made an address
ambiguous: `right.0` could be a space the author named or the first space a group
created, and nothing in the string said which. A declared name is now bare, and guard 3
forbids a trailing `.<digits>` on one, so the two forms cannot collide and an address
is read by looking at the data rather than at the punctuation.

Generated names are still a cost of a flat structure, and it is the same cost as before:
nothing in the data says a created space descends from anything, only which group it is
in and what position it holds.

## 5. Groups

A group is a space with a `layout`, and it is the only nesting mechanism here. A
program has exactly **one** rule list, at program level, and it applies to every space
in that program.

**A program that needs two arrangements in two regions writes two groups**, each
naming a layout that has its own rule list, and the parent places them with its own
`run` and `share` like any other members:

```toml
[[omniwm.layouts.three_sides.rules]]
rule = "run"
axis = "x"

[[omniwm.layouts.three_sides.rules]]
rule = "share"
subject = "main"
of = 0.5

[[omniwm.layouts.three_sides.spaces]]
name = "main"
layout = "dwindle"

[[omniwm.layouts.three_sides.spaces]]
name = "side"
layout = "vertical_stack"
```

**A group with no members is dissolved, and the language does not prevent it.** A
space holds zero or one occupant, so a group can be empty, and an empty group is
removed rather than left on screen holding nothing. Its space goes the way any other
member's does and the parent re-solves around the gap.

That is why there is no key saying what an empty group draws, and why there is no
fallback for a group bar whose `against = "client"` reference has gone. The state is
transient, because the group stops existing before anything is drawn with it. A
silent fallback would be the wrong answer twice over: it would give a rule a second
meaning it did not ask for, and it would paper over the case where the reference is
missing for a reason the author did cause.

**A group that must stay visible while it is empty is given something to hold**, and
the something need not be a window. A fake client is a full participant, per
`generaldesign.md` §13: it occupies a space, the program places it, it is a member of
whatever group holds it, and it is solved like any other client. So a placeholder
member keeps a group present, and the border it wants is a client rule set rather
than anything new: `omniwm.clients.frame` is a bottom-aligned rule two pixels tall
four pixels off the edge. The group bar needs no rule of its own, because the
placeholder is a member and so the group's virtual client is the placeholder's box,
and `against = "client"` resolves exactly as it does for a bar on a real window.

Whether a layout does this is its author's business. A user layout that does not
dissolves when it empties, and nothing warns it first. A built-in layout that has to
survive being empty ships the placeholder with it, which is the only difference
between the built-ins and a user's, and it is a difference in what is shipped rather
than in what the language allows.

A half that is a dwindle and a side that is a stack, in one program, with no key
anywhere saying "half" or "side". The nested layout is a child solver instance with its
own constraint set and inherits nothing from its parent.

**There is no `groups` key, and here is why one is not needed.** It would be an array
of named rule sets inside one program, with a space's `group` field saying which set
it was in.
It duplicated nesting one level down, and it had no rectangle: the geometry rules
measure against "the group's rect" and a `groups[]` entry had none, so every group in a
program claimed the whole program and the author had to carve them apart with
geometry rules that had no other purpose. Both of its uses in the test suite were a set of columns with one member
each, which is `run` plus `share` or a pinned `w`, written flat. A rule list is a
program, so a program that wants a second one nests.

## 6. Mapping

A client with no space of its own is offered to the program's spaces in declaration
order. It takes the first space no client occupies; if there is none, the program's
occupancy rule fires and, if that rule creates a space, the client takes the new one. A client named explicitly, by a key action
or a spawn mode, skips this and goes to the space it was named for, which is the
case that makes `keep = "new"` meaningful.

### 6.1 Which member is the target

When a client is directed at a group, or several members are full, the target is
the **first full member in declaration order**, filtered to the group.
`devnotes/layoutengine.md` §3.2 requires solving to be a pure function of committed
state, and a target chosen by hash order would break that.

### 6.2 Termination

`split`, `spawn` and `float` cannot cycle: each adds a member or removes one, and a
program has finitely many once every group is full. Only `push` can cycle.

**A `push` is valid only if the target's `order` is strictly less than the pushing
group's lowest member `order`.** Checked at load. Rank strictly decreases along
every push, so a chain runs at most `max(order)` steps. Master pushing to stack is
`0 < 1`; stack pushing back to master is a load error naming the layout.

### 6.3 When no rule applies

If a client is directed at a full member and the group has no occupancy rule, or has
one that does not create a member, the client is not placed and the layout reports
it. A group with a closed member list and no occupancy rule is a load
error, because it describes a layout that deadlocks rather than one that is merely
unconfigured.

## 7. `viewport`

A table, at most one, on the program.

| field | values | default | meaning |
|---|---|---|---|
| `axis` | `x`, `y`, `xy` | `xy` | the scrollable axis |
| `extent` | `parent`, `unbounded` | `parent` | whether the view is clamped to the group |
| `virtual_origin` | table of `x` and `y` | none | required when `extent = "unbounded"` |
| `pan` | boolean | `false` | the view may be translated |
| `zoom` | boolean | `false` | the view may be scaled |

`extent` is the file's only claim to be unbounded and the only place an origin is
stated. A canvas is therefore `viewport.extent = "unbounded"` with a
`virtual_origin`, plus `run` with `self = "content"` if it tiles, or `scatter` if it
does not. The scroller is `extent = "parent"` with `self = "content"`.

**The pan and zoom values are not in this file.** `pan` and `zoom` declare that a
transform exists and on what axis; the offset and the scale are compositor state
held per tag. `devnotes/layoutengine.md` §3.3 reached this already.

## 8. Frozen limits, and what is deliberately absent

- **No expressions and no arithmetic.** A count is an integer or one of two named
  rules.
- **No `priority`, no weight bands.** List order is the weight, except among
  geometry rules, where two rules about one subject are an error rather than an
  ordering, §3.5.
- **No `when`.** The family is the trigger and `full` is the only trigger, so a second
  one is the single place this language would need a new key. It goes here and on the
  four occupancy kinds. A rule kind per trigger is not the alternative, because
  `split` and `split_on_close` is combinatorial in the number of triggers and reads
  worse than the key did.
- **No `kind` on a space.** A `layout` field is the claim.
- **No `members` and no `group` field.** The spaces in a program are its membership,
  and a space that nests is a group by carrying a `layout`, §5.
- **One rule list per program.** A second arrangement means nesting, not a second
  array of rules beside the first.
- **No parent links.** The cost is in §11.
- **No addressing of created spaces** except by the generated name of §4.
- **No user code, no includes, no conditionals.** A layout is data.
- **No transforms other than `raise`.**
- **No rule chaining.** `push` targets a space, not a rule.
- **One arrangement rule per group**, no inheritance, no override.
- **No colour, border or rounding.** Those are `draw.md` and `decorate.md`.
- **No client-side keys in this file.** `align`, `match`, `size`, `offset` and
  `snap` are on the client, §3.6, and a program never names one. This file places
  spaces; the client layer places clients.
- **No snapping at the program level.** A snap belongs to a client at a moment,
  §3.7, so a program's geometry does not change because a window moved.

Load-time validation of a program, and **these checks belong to the layout engine,
not to the store**, because each one is a question about meaning rather than about
bytes. `configstorage.md` §12 has no semantic tier and does not get one back: the
store guarantees a program can be *decoded* and refuses with a named code if it
cannot, and every check below is the consumer's decision about whether a decoded
program makes sense. That is also why a failed check is a load error naming the
layout rather than a refused write, since nothing about the write was malformed.
The engine runs this list when a program is read, and `tomlparser.md` §8's
per-line failure policy is what a failure looks like through a config file:

1. `rule` is one of the ten kinds, and only the arguments belonging to it are
   present.
2. The first rule of the program is an arrangement rule, and there is at most one.
3. Every `name` is unique within the program and no `name` ends in `.<digits>`, so a
   declared name cannot collide with a generated one.
4. Every `layout` names a program that exists, no program transitively contains
   itself, and nesting depth is at most `OMNI_LAYOUT_MAX_NEST_DEPTH`.
5. Every `to` and `subject` is a space address read left to right, §3.2: a
   single component is a declared space of this program, a longer one is `main`
   followed by one decimal position, and anything else must name a space carrying a
   `layout` and continue inside that program. A group on its own is not an address,
   because a group is a region of spaces and not a place a client goes.
6. Every `to` has a strictly lower `order` than the pushing space.
7. The `of` fractions of the program's `share` rules sum to at most 1.
8. A member of a `scatter` arrangement pins none of `x`, `y`, `w`, `h`, §4.1.
9. `of` is a positive number, and `by`, `gap` and `raise` are non-negative
   integers, and each count is an integer >= 1 or one of the two named rules.
10. A `split` rule's group has a `sticky` `run` arrangement with an `axis`.
11. `virtual_origin` is present exactly when `viewport.extent = "unbounded"`, and
    `self = "content"` implies a `viewport` with `pan = true`.
12. A group with a closed member list has an occupancy rule.
13. No subject is named by two geometry rules, §3.5.

The client layer is guarded separately, because it is validated when a client is
attached rather than when a program is loaded, and because its errors name clients
rather than programs:

1. `rule` is one of the five client kinds and only its own arguments are present.
2. `against` names a reference that exists: a clustered client, or `viewport` or
   `output`, which always do.
3. `edge` and `axis` are present exactly on `align` and `match`, and `region`
   exactly on `snap`, and at least one of `width` and `height` on `size`.
4. `region` is one of the ten of §3.7.
5. Every member of a cluster is placed inside the cluster's box by a satisfiable
   set of rules, or the unsatisfied ones are reported, §3.8.
6. No client is clustered with itself and no cluster set contains a cycle.

## 9. Storage

The stored form is this text, as a `constraint` composite value. The 16-byte record
of `include/shared/omni_layout.h` is no longer implied by anything: there is no
fixed-width record in this language, because every argument is either a small
enumeration, a number, or a name. A program is a table, and it is stored as one.

That reverses the fixed-width record that `devnotes/layoutengine.md` and the
header used to imply, and the header has been changed to match: `OMNI_TAG_CONSTRAINT` at `0x32` is a
composite value rather than a record array, and its payload is
`constraint := array of space` with a space being an `option` and a rule a named
key of that option (`configstorage.md` §4, `omni_layout.h` section 7). The
"would need to become" above was written before that change and is now a
description of the tag as it stands.

## 10. Consequences worth recording

**The block becomes readable without a schema.** A program is a table of small
values and names, so `save` produces something a user can edit by hand, which
`devnotes/layoutengine.md` §4.6 requires and which a record array could not
provide at any width.

**The duckWM lineage is now confined to two rule kinds.** `share` and `inset` are
its relations, they are solved as soft weighted
constraints, and nothing else in the language is one. The weighted solver survives
as the implementation of those two and stops being the architecture.

**A rule is not a constraint and a constraint is not a rule, but they are both
rules.** Keeping one list means an author does not learn two vocabularies, and it
means the arrangement and the overflow behaviour can be written in the order they
are read rather than in separate blocks whose interaction was never specified.

## 11. Complete key reference

Every key in the language, in one place. Generated against the sections above, so
a key that appears in a program but not here is a defect in this file.

### 11.1 Program level

| key | type | present | meaning |
|---|---|---|---|
| `rules` | array of tables | at most one, may be empty | **the one rule list**, §3 |
| `spaces` | array of tables | at most one, may be empty | §4 |
| `viewport` | table | at most one | §7 |
| `rearrange_on_focus` | boolean | at most one | whether a focus change runs a pass, §3.0 |

There is no `groups` key and no `group` field on a space. A program with none of the
four is a blank program, and a program wanting a second rule list nests a group, §5.

All four keys are each optional, and each one that is missing or blank is resolved
independently by the consumer from a static default compiled into the code. "At most
one" is therefore the real cardinality: a program that omits all three structural
keys is the blank canvas of `layoutengine.md` §7.8, a program that writes only
`rules` has spaces it did not ask for, and neither is an error. The defaults are
not values, they are the absence of a value, and that is the whole of the rule:
nothing is ever written into the block to stand in for one, and no consumer writes
a default back. A key present
but malformed is a different case and is a parse error, not a fallback; see
`tomlparser.md` §11.

### 11.2 A group

A group is not a table and has no keys of its own: it is a space carrying a `layout`,
so the only `omniwm.layouts.<name>.*` key that makes one is the space's `layout`, §11.7.
Its rules live in the program it names, at that program's program level.

### 11.3 Rule, common keys

| key | values | present | meaning |
|---|---|---|---|
| `rule` | the ten kinds in §3 | required, first key | which kind of rule this is, and its family, and its trigger |
| `keep` | `new`, `old` | required on the four occupancy rules | who keeps the full space |

There is no `when` key. The `rule` value carries the family, the family carries the
trigger, and `full` is the only trigger there is, §3.

### 11.4 Rule, arrangement arguments

| key | applies to | values | default |
|---|---|---|---|
| `axis` | `run`, `stack` | `x`, `y` | `x` |
| `gap` | `run`, `grid` | integer >= 0 | `0` |
| `leftover` | `grid` | `center`, `absorb` | `center` |
| `columns` | `grid` | integer >= 1, or `sqrt` | `sqrt` |
| `rows` | `grid` | integer >= 1, or `derived` | `derived` |
| `raise` | `stack` | integer >= 0 | `0` |
| `reflow` | `run` | `flow`, `sticky` | `flow` |
| `self` | `run` | `parent`, `content` | `parent` |

`scatter` takes no arguments. `raise` is the only transform in the language.
`share` is a rule kind, not an argument to `run`, §3.1.

### 11.5 Rule, occupancy arguments

| key | applies to | values | default |
|---|---|---|---|
| `to` | `push` | a space address, §3.2 | required |

The other three occupancy rules take no argument beyond `keep`.

### 11.6 Rule, geometry arguments

| key | applies to | values | default |
|---|---|---|---|
| `subject` | `share`, `inset` | space name | required |
| `axis` | `share` | `x`, `y`, `xy` | `xy` |
| `of` | `share` | number > 0 | required |
| `by` | `inset` | integer >= 0 | required |
| `edges` | `inset` | array of `left`, `right`, `top`, `bottom` | all four |

`axis` is the one key that appears in three families, with a different default in
each. That is deliberate rather than sloppy: it always means "which axis", and
splitting it into `axis` and `direction` would have added a word to say the same
thing.

Every geometry rule reads its subject's current rectangle, so rules about different
subjects compose in any order. A subject named by two geometry rules is a load
error, §3.5.

### 11.7 Space

| key | values | present | meaning |
|---|---|---|---|
| `name` | string | required | unique within the program, bare, no trailing index, §4 |
| `layout` | program name | optional | **present means this space is a group**, §5 |
| `x`, `y`, `w`, `h` | integer >= 0 | optional, `0` | **present pins that component** |
| `order` | integer >= 0 | optional, `0` | rank for `push` termination |

There is no `kind` key. A space with a `layout` is a group and one without is not.
There is no `role` key either: a fake client is a client, §3.6. There is no `rect`
key either, because `scatter` already says the client brings the rectangle, §3.1.

`x`, `y`, `w` and `h` are pinned by being present, not by their value, §4.1. `w = 0`
is a zero-width space and an absent `w` is a computed one, which is the same rule the
`layout` field follows.

### 11.8 Viewport

| key | values | default |
|---|---|---|
| `axis` | `x`, `y`, `xy` | `xy` |
| `extent` | `parent`, `unbounded` | `parent` |
| `virtual_origin` | table of `x` and `y` | none, required when `extent = "unbounded"` |
| `pan` | boolean | `false` |
| `zoom` | boolean | `false` |

`extent = "unbounded"` is the only unbounded claim in the file.

### 11.9 Keys that appear at more than one level

| key | levels | note |
|---|---|---|
| `axis` | arrangement, geometry, viewport | three defaults, one meaning: which axis |
| `x`, `y` | space pin, `virtual_origin` | the same two names for the same two things, §4.1 |
| `layout` | space | also the name of the whole program, so `omniwm.layouts.<layout>` and a `layout = <name>` field |

### 11.10 Keys that are not in this language, and why

Recorded so that a config or a draft written against an earlier version of this
language can be read rather than guessed at, and so that a key's absence reads as a
decision rather than an oversight. The `removed in` column of earlier drafts is gone
because those drafts are not part of the set; what is worth keeping is the reason.

| key | why |
|---|---|
| `capacity` | a space holds one client; full is read off the state |
| `kind` | a `layout` field is the claim; nothing needs asserting twice |
| `members` | the `spaces` array is the membership; a second rule list nests a group, §5 |
| `priority` | list order is the weight, and it reads better |
| `pattern` | an arrangement is a rule like any other |
| `constraints` | the geometry rules are rules, in the same list |
| `value`, `rule` | a count is a bare integer or a bare string, §3.4 |
| `share` as a `run` argument | it is a rule kind, and one concept has one name, §3.1 |
| `unbounded` | `viewport.extent` is the only unbounded claim, §7 |
| `virtual_origin` on `run` | the origin belongs to the viewport, §7 |
| `self = "unbounded"` | `"content"` plus an origin, and the origin is one fact |
| `extent` on a group | replaced by `self`; `extent` survives only in `viewport` |
| `ref` | the solved layout carries `entry_ref` identity, not the program |
| `on_overflow` | `rule` plus `when = "full"` reads as itself |
| `on_full` | same |
| `role` | a fake client is a client, §3.6 |
| `rect` | `scatter` already means the client brings it, §3.1 |
| `when` | the family is the trigger, and `full` is the only trigger, §3 |
| a trailing index on a declared `name` | the index is the system's, and it made an address ambiguous, §4 |
| `groups`, `groups[].name`, `groups[].rules`, `group` | a rule list is a program, so a second one nests; and a `groups[]` entry had no rectangle, §5 |
| `fill` | it set a member to the group's rect, which only ever mattered when a group had one member, and there was never more than one |
| `beside` | it reordered members that `run` already places in declaration order, and its gap was `run`'s `gap` argument |
| `operand` | it belonged to `beside` only, so removing `beside` removed it |
| `hundredths`, `pixels` | the unit is implied, and sizing is a `share` rule or a client `size` |

### 11.11 Client rule keys

Not part of this file's program, and listed here so that the whole vocabulary is in
one place rather than two. §3.6, and the two namespaces are §3.6.1.

`omniwm.clients.<set>.rules.*`, a fake client's own rules, reached by the action
that creates the surface:

| key | applies to | values | default |
|---|---|---|---|
| `rule` | all | `align`, `match`, `size`, `offset`, `snap` | required, first key |
| `against` | `align`, `match`, `offset`, `snap` | `client`, `viewport`, `output` | `client` |
| `edge` | `align` | `left`, `right`, `top`, `bottom`, `center_x`, `center_y` | required |
| `axis` | `match` | `x`, `y` | required |
| `width`, `height` | `size` | integer >= 0 | none, at least one required |
| `by` | `offset` | integer | `0` |
| `region` | `snap` | `left`, `right`, `top`, `bottom`, `top_left`, `top_right`, `bottom_left`, `bottom_right`, `center`, `full` | required |

`omniwm.snaps.<set>.rules.*`, a named destination, reached by a drag region or a
keybind, and holding one `snap` rule and nothing else:

| key | applies to | values | default |
|---|---|---|---|
| `rule` | all | `snap` | required, first key |
| `against` | `snap` | `output`, `client`, `viewport` | `output` |
| `region` | `snap` | `left`, `right`, `top`, `bottom`, `top_left`, `top_right`, `bottom_left`, `bottom_right`, `center`, `full` | required |

The one difference in the shared keys is `against`'s default. It is `client` in the
clients namespace because a bar's whole job is to relate to the group it is in, and
it is `output` in the snaps namespace because a destination that defaulted to its
co-client would make `snap_left` mean "the left half of whatever happens to be
beside me", which is `snap_beside` with a worse name. The ten region names are the
same in both.

`against = "client"` means the client this one is clustered with, which is the
virtual client of §3.8 and not necessarily a client that exists by name. A `snap`
against `client` is Wayfire's snap to a window; against `output` it is one of the ten
monitor zones.
