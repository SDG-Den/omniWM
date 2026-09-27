# Phase 06: `windows.md` finalization

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md).*

Stub. Not started. The document exists as a prototype and calls itself one.

`windows.md` §1 says it is a scaffold built to `d5a0e1e` so the parts that must
agree with the store, `tags.md` and the solver have one written shape, and §14
lists what it did not cover. This phase is the prototype becoming a specification.

## What it should contain

**1. The surface syntax for anything in it.** The whole document is currently
reachable only through Mango's config grammar. `tomlparser.md` §2 now has a
binding for a window rule table, at `omniwm.window_rules.<name>` as a `map`, which
closes the gap `audit-2-findings.md` flagged; what is left for this phase is the
rule syntax itself, which is item 5 below.

**2. The client protocol surface.** `generaldesign.md` §13 makes fake clients
layout participants. What a client can be told, and what it can be asked, is not
written anywhere.

**3. The one decision of `windows.md` §13 that this phase still owns.** The other
four are closed and the closures are in the document, so this phase does not
re-derive them and should not treat them as open:

- ~~Membership walk order.~~ **Ascending `entry_id`, and it is the design.** §13
  records why: it is total, stable across restarts, cheap on the solve's hot path,
  and free of insertion history, which is what makes two clients reaching the same
  set walk identically. The scattered-walk worry is accepted rather than deferred,
  because the walk is over one tag's membership children and so is bounded by how
  many clients a tag holds rather than by how many exist.
- ~~Whether a focus change causes a pass.~~ **Per layout, as a program-level key.**
  `layoutlanguage.md` §3.0's `rearrange_on_focus`, defaulting to `true`, and Mango's
  conditional re-arrange is the evidence for making the trigger per layout.
- ~~Whether the client kind set is closed.~~ **Closed at seven.** `windows.md` §10
  has the table. Mango's six protocol kinds are kept because they describe what the
  client talks to the compositor about, `GroupBar` is dropped because ours is a
  placed participant rather than chrome, and `FAKE_CLIENT` is ours.
- ~~What a cluster contributes to stacking and focus beyond moving as one.~~
  **Nothing.** A cluster constrains geometry and grouping only: its members stay
  individually focusable and individually interactive, they do not raise as a unit
  because there is no unit to raise, and they do not become a single focus target.
  A group is the other thing, and the difference is now stated in both documents
  rather than left to be inferred from Mango.

**3a. The one that is still open, which §13 item 3 deliberately deferred here with
the shape fixed.** A fake client *participates* in focus, and focusability is a
per-client property defaulting to **non**-focusable (`windows.md` §10). What this
phase decides is whether that default should ever be the other value for some kind,
which needs the input and focus design this stub does not have. The reason it is
worth deciding rather than accepting is that the default is what most clients get,
so a kind that *should* be focusable and is not will be a silent omission rather
than a configuration error, and the document is better off saying which kind, if
any, that is.

**3b. Scratchpad visibility, and that it is not membership.** The reconciliation
pass drew a line this phase has to implement rather than re-argue: for the
scratchpad, **focus is visibility** and hiding into it is membership, and
**minimisation is a third thing again**. Two documents depend on that being one
distinction and not three, and an implementation that collapses them will appear to
work until a user minimises a client and finds it tagged.
**4. `windows.md` §14's not-covered list**, which is separate from §13's decisions.

## What it resolves

- Whether a prototype is a specification. The distinction matters because
  `layoutengine.md` and `tags.md` both cite `windows.md` as authority, and a
  document that disclaims being one cannot be the authority for a record layout.
- The client kind list, which is needed by phase 07: the solver partitions clients
  by kind, so an open kind set is an open solver input. **Now closed at seven**,
  so phase 07 has a fixed input rather than an open one.
- The window-rule syntax. The table binding is closed (`tomlparser.md` §2, `map` at
  `omniwm.window_rules.<name>`); the `if` and `then` keys inside it are this
  phase's.
- The per-client focusability default of 3a, and the visibility/membership/
  minimisation distinction of 3b, both of which the reconciliation pass left to
  this phase.

## Why it is here

`windows.md` exists but is not authoritative, and two later phases cite it as
though it were. It gates phase 07, because the solver consumes clients and needs
the kind set and the focus-trigger answer. It follows phase 05 because §9's window
rules reference tags.

## Done when

- The prototype disclaimer at `windows.md` §1 is either removed or replaced with a
  statement of what is now frozen.
- The four already-closed §13 decisions are still closed and are cited as
  decisions rather than re-opened, and the §13 heading no longer says the document
  "carries" them as open.
- 3a is answered, or is explicitly accepted as the default with the reason written
  down. Leaving it unstated is the one outcome that fails this phase, because the
  default is what almost every client gets.
- 3b is written as an implementation note, not only as prose in `tags.md`, so the
  three operations are distinguishable by name.
- A client kind list exists, and `layoutengine.md` §3.2's partition cites it.
