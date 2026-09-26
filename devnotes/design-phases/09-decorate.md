# Phase 09: `decorate.md`

Stub. Not started. P1, stages 8 and 11.

## What it should contain

**1. Multi-layer borders, including gradients and textures.** Mango has
`noborder`, `noshadow`, `noradius` per client (`ConfigWinRule`), so borders are
already a per-client override in the reference, and layering is the extension.

**2. Shadows.**

**3. Blur.** `ConfigWinRule` carries `noblur` and `shield_when_capture`, and Mango
has a capture path that a blur has to be careful about, so the interaction with
capture is a real requirement rather than a detail.

**4. Glow.**

**5. Opacity.** Mango carries `focused_opacity` and `unfocused_opacity` as floats
per client, so the concept exists; the node model from phase 08 is what it
attaches to.

**6. Rounding.** Mango carries `noradius`.

**7. Window overlays and underlays.** Mango's `isoverlay` is per client, and
`generaldesign.md` §11 owns decoration at the high level.

**8. Dimming.** A monitor-level effect rather than a client-level one, which is
why it is here rather than in `monitor.md`: it decorates everything on an output.

**9. The three per-window override routes converging on `WINDOW_DEPENDENT` keys.**
`configstorage.md` §3 defines `WINDOW_DEPENDENT` scope and `windows.md` §9 settles
the three routes. This is the item that makes the document a specification rather
than a list of effects, because three routes into one key needs a precedence rule
and `missing-devnotes-topics.md` names global-versus-per-window precedence as the
open question.

**10. The scenefx dependency**, insofar as blur and glow need shader support.
Shared with phase 08; the requirement list is phase 08's and this phase adds to
it.

## What it resolves

- The global-versus-per-window precedence question, which is the one that actually
  blocks implementation. Everything else in the document is a description of an
  effect that can be written once the precedence is known.
- Whether a decoration override writes the block or a process-private structure.
  It writes the block, because `WINDOW_DEPENDENT` is a store scope, and that
  decision is what makes the override inspectable by a script.

## Why it is here

It reads the node model from phase 08 and the per-client fields from phase 06. Its
own output, the set of animatable decoration properties, feeds phase 10. It sits
before animate because animation needs to know what it may animate.

Its roadmap position is stage 8, which is *before* draw's stages 12 to 14. That
inconsistency is phase 08's first item and is unresolved, so this phase's position
is provisional.

## Done when

- The three override routes have a stated precedence, and a reader can answer
  "which of these three values wins" from the document.
- Every effect names the node property it sets, using phase 08's vocabulary.
- The animatable subset is marked, so phase 10 does not re-derive it.
