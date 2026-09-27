# Phase 05: `tags.md` finalization

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md).*

Stub. Not started. The document exists as a prototype; this phase is what it does
not cover.

`missing-devnotes-topics.md` owes four things: what a tag looks like, tag rules,
per-output tag defaults, and the surface syntax for setting tags.

## What it should contain

**1. What a tag looks like.** Names, colours, icons, ordering in a bar. This is
the user-facing part and the one the other three depend on, because a rule
assigns properties and the defaults are per output. `tags.md` §1 is explicit that
this is not covered and that the omission is a decision.

**2. Tag rules.** Mango's `ConfigTagRule` (`parse_config.h:184-201`) has an id, an
id wildcard, a layout name, four monitor matchers, `mfact`, `nmaster`, three
scroller options and `no_render_border`. Which of those survive is a real
question: a tag rule in Mango creates a tag, and in omniWM a tag is an entity that
exists whether or not a rule made it.

**3. Per-output tag defaults.** Depends on phase 04's matching vocabulary.

**4. The surface syntax for setting, unsetting and exclusively setting a tag.**
`tags.md` §9 assigns this jointly to `input.md` and `helpers.md` §11, so this
phase has to produce half of it and coordinate the other half. `input.md` §7.1's
scoping question is entangled: if a binding is scoped to a tag, then the syntax
for setting a tag is what a binding most often needs.

**5. The two questions `tags.md` §9 used to carry, now closed by phase 00 and
recorded here so this phase does not reopen them.** *Stored versus derived
membership* is closed as **stored** on the tag (`tags.md` §5, `wm.monitor.<id>.tags`),
on access-pattern grounds, and `layoutengine.md` §3.2's determinism requirement is
the reason; §7.7 agrees by pointing at the same decision. *The surface form of the
swap's keypath arguments* is closed as **two full keypaths**: `ipc.md` §4 writes
them as `wm.monitor.<m>.tag.<n>` and the operation is a content swap in one grouped
commit. Both were struck from `tags.md` §9 when they were decided. What remains for
this phase is the bar-side surface in item 1, which is a gap rather than an open
decision.

**6. The three store operations `configstorage.md` §14.1 now owes tags.** These are
not design questions for this phase to answer, because `tags.md` §5 and §7 have
already answered them: a tag is a container, so the store needs a subtree delete, a
subtree exchange, and generation-correct deletion. They are listed here because this
is the phase that makes them concrete, and the **subtree exchange is the one to
think about first** — it is not a special case of the delete, because a delete may
free each frame as it goes while an exchange frees nothing at all, since every
child of one tag is still live in the other. An implementation that has a subtree
delete and assumes the swap can be built from it finds this out at the point where
it cannot free.

**7. Whether the tag model this phase is completing is still the one that was
written down.** `tags.md` §3 was reversed in the reconciliation pass: a tag was
documented as belonging to no particular monitor and is now identified by the pair
(monitor, number), mirroring was reframed from a given-up feature into the protocol
violation the per-monitor identity makes unrepresentable, and the scratchpad
became configurable between one shared tag and one per monitor (`wm.scratchpad.
shared`, default `true`) with a layout of its own. Phase 05 should not re-derive
those; it should check that the document is internally consistent with them, and
the specific things worth re-reading are `tags.md` §3.1 (the invariant is *at most*
one monitor, not exactly one, which is what lets the shared scratchpad exist) and
`§8.2` (the scratchpad takes no part in §4's union rule, which is the one place
§8 contradicts the pattern §8.1 sets).

## What it resolves

- The last four rows of the `tags.md` row in `missing-devnotes-topics.md`.
- `layoutengine.md` §7.7's membership question, which is a determinism question
  and therefore a layout question, not a tag question. That one is listed in
  phase 07 as well, and the answer has to be the same in both.
- The user-facing half of the tag-setting syntax, which phase 03 and
  `helpers.md` §11 both reference.
- The three store operations of item 6, so `configstorage.md` §14.1 has
  specifications behind it rather than only a list of names.

## Why it is here

`tags.md` exists, so this is a completion rather than a document, but it cannot
start before phase 04 because per-output defaults need the matching vocabulary,
and it cannot finish before phase 03 because the setting syntax is entangled with
binding scope. It gates phase 06, because `windows.md` §9's window rules reference
tags. The dependency with phase 04 now runs the other way from the one this stub
used to state: `tags.md` §9 used to leave the storage of a monitor's tag list to
`monitor.md`, and it no longer does, so phase 04 reads this phase and the two must
agree on the `wm.monitor.<id>.tags` shape.

## Done when

- A tag is describable to a user: what it is, what it looks like, how it is set.
- `layoutengine.md` §7.7 is closed with a pointer to a decision made here.
- `input.md` §7.1 and `tags.md` §9 agree on which document owns the setting
  syntax, and neither says "and `helpers.md` §11" without saying which half.
- A tag is named with a monitor in every user-facing surface, including the filter
  and seed forms, so there is no surviving spelling that takes a bare number.
- The scratchpad's shared and per-monitor configurations are both described in
  terms that the same code can implement, with no branch outside `§3.1`'s
  at-most-one-monitor invariant.
