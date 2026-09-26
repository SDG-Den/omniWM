# Phase 08: `draw.md`

Stub. Not started. P1, stages 12, 13 and 14.

This phase carries a question that is not about drawing at all, and it is the
reason the phase exists separately rather than being folded into `decorate.md`.

## What it should contain

**1. The roadmap inversion, decided first.** `README.md` puts basic decorations
at stage 8 and the scene graph at stages 12 to 14. But `decorate.md` overrides
border, shadow, blur, glow, opacity and rounding, and `animate.md` animates
"properties on nodes", and both of those need a node model. Either the stage
numbers are wrong about the dependency, or `draw.md` has to be partly written
before stage 8. **This is the phase's first item and it decides its own scope.**

**2. The scene graph node types and their animatable properties.** The core of
the document, and the thing phases 09 and 10 consume. What is a node, what
properties it carries, and which of those properties an animation may touch.

**3. Backgrounds: colour, image, shader, vector overlay, and a panning infinite
canvas.** `generaldesign.md` §19 explicitly defers the background and
infinite-canvas options to stage 13 and this document, and `layoutengine.md`'s
scroller work already assumes a canvas with bounds (`layoutengine.md` §7.2).

**4. Vector line objects**, stage 14. Mango has them, so the shape of what it did
is the reference.

**5. Text and images.** Whether these are nodes or textures attached to nodes is
not derivable from the other answers and has to be decided.

**6. How the compositor's own drawn surfaces are produced.** `generaldesign.md` §12
covers this at a high level; this document is where it becomes concrete.

**7. How externally rendered regions become nodes.** The hinge between this phase
and phase 11. `configstorelayout.md` §10 defines a region descriptor and its
pixels, and `protocols.md` owns the protocol that produces them, so the question
is what the compositor does with a region's buffer once it has one.

**8. The scenefx dependency.** `generaldesign.md` §19 calls the extension set the
largest risk to "Any Look", and user GLSL shaders are a draw requirement, not a
decorate one. Phase 02 owns the fork-or-patch question; this phase owns what it
needs from it.

## What it resolves

- The roadmap's stage ordering, which is currently inconsistent with the document
  dependency.
- What `decorate.md` and `animate.md` are decorating and animating.
- Whether an external renderer can target a node, which is what makes the
  region-based path in phase 11 useful rather than decorative.

## Why it is here

It is the first of the look phases and it is out of roadmap order, deliberately,
because it is the one they both read. If the phase 08 item 1 answer goes the other
way, this phase moves after 09 and 10 and those two phases are written first
against a node model that does not exist yet.

## Done when

- The roadmap inversion is answered and `README.md`'s stage list reflects it.
- A node type list exists, with the animatable subset marked, and phases 09 and 10
  cite it rather than inventing one.
- The scenefx requirement is stated as a requirement list phase 02 can check
  against.
