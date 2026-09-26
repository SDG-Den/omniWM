# Phase 05: `tags.md` finalization

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

**5. The two open questions `tags.md` §9 already carries.** Stored versus derived
membership, where the recommendation is stored on access-pattern grounds and
`layoutengine.md` §7.7 has to agree because §3.2 makes the solve deterministic.
And the tag-move operation, which `generaldesign.md` §19 explicitly hands to this
document.

## What it resolves

- The last four rows of the `tags.md` row in `missing-devnotes-topics.md`.
- `layoutengine.md` §7.7's membership question, which is a determinism question
  and therefore a layout question, not a tag question. That one is listed in
  phase 07 as well, and the answer has to be the same in both.
- The user-facing half of the tag-setting syntax, which phase 03 and
  `helpers.md` §11 both reference.

## Why it is here

`tags.md` exists, so this is a completion rather than a document, but it cannot
start before phase 04 because per-output defaults need the matching vocabulary,
and it cannot finish before phase 03 because the setting syntax is entangled with
binding scope. It gates phase 06, because `windows.md` §9's window rules reference
tags.

## Done when

- A tag is describable to a user: what it is, what it looks like, how it is set.
- `layoutengine.md` §7.7 is closed with a pointer to a decision made here.
- `input.md` §7.1 and `tags.md` §9 agree on which document owns the setting
  syntax, and neither says "and `helpers.md` §11" without saying which half.
