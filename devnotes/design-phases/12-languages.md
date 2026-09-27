# Phase 12: `languages.md`

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md).*

Stub. Not started. P2, stages 15, 16, 17 and 18.

The lowest-risk phase in the plan, and it is last because of that rather than
because it is blocked. Its two inputs are finished: `ipc.md` is written, and
`helpers.md` §6 has the action contract, the argument schema and the binding
encoding. This is the phase where the design is most likely to succeed, so it is
the phase whose outcome says the most about whether the rest was right.

**This phase does not build part of the window manager.** Its outputs are separate
programs — a C client library, a Python library, and interpreters for Mango and
Hyprland configs — written after the project is otherwise complete, to prove the
claims and inspire users. They have no cross-dependency with the compositor in
either direction; the only thing they share is that both expect the SHM to exist.
The compositor stays generic, which is the claim these programs exist to test, so
nothing here may become a reason to change the block. See item 6 and
`generaldesign.md` §17.

## What it should contain

**1. The C reference client library, over `include/shared/`.** This is the one
that matters and the one that is a design commitment rather than a convenience: the
ABI header is the contract, so a C client is a first-class consumer of the block
rather than a program that goes through the socket. `filestructure.md` makes
`include/shared` the ABI surface, and this is what that is for.

**2. The Python reference scripting library.** Stage 15. Its design question is
whether it speaks the socket protocol or mmaps the block, and the answer decides
how much of the store's semantics have to be reimplemented in a memory-safe
language. The block is shared memory with a futex commit protocol, so an in-process
client is a different thing from a socket client in a way that has to be decided
rather than defaulted.

**3. The Mango interpreter**, stage 16. The reason the project exists, per
`README.md`: "the goal is to at least provide a reference implementation for this".
Its input is `protocols.md` and the Mango config grammar, and its output is a
running compositor configured by a Mango config. The design question is what
Mango concepts have no omniWM equivalent, and `input.md` §7.1's mode mapping is
the sharpest instance.

**4. The Hyprland interpreter**, stage 18. The second interpreter, and the one
that tests whether the abstraction is real: if the mapping from Hyprland's config
to omniWM's model is mechanical given the Mango one, the model is right; if it
needs per-concept work, something is missing.

**5. Why a first-class library in another language needs no compositor change.**
This is the load-bearing argument of the whole project, and `README.md` states it
as a goal rather than a proof. The document has to make it an argument.

**6. ~~Whether Lua remains an initial deliverable.~~ Closed, and the answer covers
more than Lua.** `generaldesign.md` §19 has it: **no interpreter and no library is
a deliverable of the window manager.** They are example implementations made after
the project is otherwise complete, to prove the claims and inspire users, and they
are separate deliverables in their own right. The reason is the load-bearing one
and this phase should state it rather than inherit it: the compositor stays generic,
and a language-specific component inside it in order to serve a language-specific
library would be counter to the entire point, because it would make the surface
Lua-with-a-wrapper instead of language-neutral. So the answer to "is Lua in" is that
nothing is in, which is a stronger and simpler position than the one this item was
written to ask about.

The second half of the old item is still worth keeping, because it is a warning
rather than a question. A `list_layouts` reply that can only show one layout was
already fixed once by making layout names free-form (`layoutengine.md` §11), so the
precedent stands: a language gap shows up as an artificial limit somewhere in the
design. The corollary for this phase is that **no such limit may be introduced in
the other direction either** — the block's surface must not grow a feature because
it happens to be awkward for one demonstration program.

## What it resolves

- Whether the API-driven premise is a design or a slogan. This phase is the test
  of it.
- The in-process versus over-socket question, which `ipc.md` §1.5 already
  anticipates by saying the socket's absence degrades the project to the block
  alone.
- Whether Lua is in or out. **Out, along with every other interpreter and library**,
  on the grounds in item 6 rather than on grounds of effort or maturity.

## Why it is last

Nothing depends on it, and it depends on almost everything. It is also the phase
most worth doing late on purpose: an interpreter written against a finished design
finds the design's gaps, and writing it against a half-finished one only finds
the half that was written.

**Being last is the decision, not a scheduling accident**, and this phase is where
that is least obvious, because it is the phase that most looks like part of the
product. It is not. These are the "check out what my WM can do" pieces, they are
separate programs with no cross-dependency in either direction, and the only thing
they share with the compositor is that both expect the SHM to exist. The compositor
does not know they exist and does not change because of them, which is the claim
they exist to test.

## Done when

- The C library's surface is the ABI plus the store's semantics, and no third
  thing.
- The Python library's in-process versus socket decision is stated with its cost.
- The Mango interpreter runs a Mango config on a compositor built from the
  previous eleven phases, or names precisely what prevented it.
