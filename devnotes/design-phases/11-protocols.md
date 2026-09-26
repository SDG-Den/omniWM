# Phase 11: `protocols.md`

Stub. Not started. P2, stages 16 and 18.

The lowest-urgency document that is not blocked, because its two largest inputs,
`ipc.md` and the region descriptor in `configstorelayout.md` §10, are already
written.

## What it should contain

**1. The Mango protocol set, brought in wholesale.** The same decision
`generaldesign.md` §14 made for input, and the same wording problem: "wholesale" is
the phrase in `missing-devnotes-topics.md` and the policy is now reference-and-
adapt. Which Mango protocols come across, and which are Mango-specific and have no
omniWM equivalent, is the first question.

**2. Which wlroots managers the compositor instantiates.** A concrete list, and it
is a list rather than a principle because each manager is a dependency with a
version, and phase 02 owns the version.

**3. The region-based external rendering path.** `configstorelayout.md` §10 defines
a region descriptor, its u16 dimensions, its generation, and its producer and
consumer active bits. The protocol is what fills it. The design question is
whether a region is a first-class protocol or a convention layered on something
else, because the first costs a protocol and the second costs a convention that
every external renderer has to know.

**4. The later Wayland buffer-injection protocol for GPU-path external renderers.**
Stage 18. This is the one genuinely new protocol in the document and it is the
reason the row is P2 rather than P3: without it, an external renderer that wants
the GPU path has no way in, and the project's extensibility premise depends on
there being one.

**5. How an externally rendered region becomes a node.** Phase 08 item 7. The
protocol produces a buffer and phase 08 decides what the compositor does with it.
These two must not be decided separately.

## What it resolves

- Whether the project has its own protocols or adopts Mango's, which affects
  licence provenance in phase 02 and compatibility with the Mango interpreter in
  stage 16.
- Whether an extension can render through the GPU, which is the difference
  between an extensibility surface and an extensibility surface for software
  renderers.

## Why it is here

It reads the region descriptor from the store and the node model from phase 08. It
feeds phase 12, because the Mango interpreter is a client of these protocols and
`generaldesign.md` §18 pairs stages 16 and 18 with this document.

## Done when

- There is a protocol list, and each entry is marked ported, adopted, or
  deliberately dropped.
- The region question is answered: first-class protocol or convention, with the
  cost stated.
- The buffer-injection protocol has enough of a shape that a client library could
  be written against it.
