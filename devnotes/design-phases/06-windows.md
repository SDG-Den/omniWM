# Phase 06: `windows.md` finalization

Stub. Not started. The document exists as a prototype and calls itself one.

`windows.md` §1 says it is a scaffold built to `d5a0e1e` so the parts that must
agree with the store, `tags.md` and the solver have one written shape, and §14
lists what it did not cover. This phase is the prototype becoming a specification.

## What it should contain

**1. The surface syntax for anything in it.** The whole document is currently
reachable only through Mango's config grammar. `tomlparser.md` §3 has no binding
for a window rule table, which `audit-2-findings.md` already flagged, so this
belongs here as much as in phase 00's item 1.

**2. The client protocol surface.** `generaldesign.md` §13 makes fake clients
layout participants. What a client can be told, and what it can be asked, is not
written anywhere.

**3. The five open decisions of `windows.md` §13.**

- Whether membership walk order is catalog order for good. §4 takes `entry_id`
  order because it is reproducible, and it has not been checked against a sparse
  client-id space where ascending order is a scattered walk.
- Whether a focus change causes a pass. `layoutengine.md` §3.7 owns it; §7 records
  that the focused client is an input to a stack and a monocle, and that Mango
  makes the trigger conditional on the layout, which is evidence the answer is per
  layout.
- Whether a fake client can take the keyboard focus. The honest reading of
  `generaldesign.md` §13 is that it is about participating in a focus *order* and
  not about being able to take the keyboard.
- Whether the client kind set is closed. `windows.md` §10 takes Mango's single tag
  as the shape and leaves the members open, because our kinds are decided by what
  participates differently in layout, focus and stacking, and that list has never
  been written down.
- What a cluster contributes to stacking and focus beyond moving as one. §7 and §8
  take Mango's raise-the-whole-group rule by analogy, which is an analogy and not
  a finding, because Mango has no cluster.

**4. `windows.md` §14's not-covered list**, which is separate from §13's decisions.

## What it resolves

- Whether a prototype is a specification. The distinction matters because
  `layoutengine.md` and `tags.md` both cite `windows.md` as authority, and a
  document that disclaims being one cannot be the authority for a record layout.
- The client kind list, which is needed by phase 07: the solver partitions clients
  by kind, so an open kind set is an open solver input.
- The window-rule syntax, which phase 00's `tomlparser.md` item needs.

## Why it is here

`windows.md` exists but is not authoritative, and two later phases cite it as
though it were. It gates phase 07, because the solver consumes clients and needs
the kind set and the focus-trigger answer. It follows phase 05 because §9's window
rules reference tags.

## Done when

- The prototype disclaimer at `windows.md` §1 is either removed or replaced with a
  statement of what is now frozen.
- All five §13 decisions are closed, with the analogy-based ones marked as
  derived rather than ported.
- A client kind list exists, and `layoutengine.md` §3.2's partition cites it.
