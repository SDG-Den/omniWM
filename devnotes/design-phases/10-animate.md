# Phase 10: `animate.md`

Stub. Not started. P1, stage 9.

## What it should contain

**1. Animatable properties on nodes.** Not a fresh list: phase 08's node model
marks the subset and phase 09 marks the decoration subset, and this phase is where
the two are reconciled into one answer. If a property is animatable, is it
animatable on every node that has it, or per node type.

**2. Timelines declared in config.** This is the design question rather than the
implementation one: a timeline is a config value, so it needs a tag, a namespace,
and a place in the store. Mango's `animation_type_open` and `animation_type_close`
are per-client strings (`ConfigWinRule`), and `isnoanimation` is a per-client
flag, so the reference has the vocabulary and none of the structure.

**3. Curves.** Easing functions as config values. A named constant or a curve
description, and which.

**4. Stagger.** Whether a delay can be relative to a sibling's position, which is
the difference between "everything at once with a delay" and a stagger.

**5. 2D versus 3D.** Mango has both a `scroller` layout with a canvas and
`layoutengine.md` §7.2 owns the canvas bounds, so this is a real question with a
real dependency rather than an abstract one.

**6. Shader-driven animation.** What a user GLSL shader may animate, and how that
interacts with 1-5. This is where the scenefx requirement from phase 02 becomes
load-bearing rather than precautionary.

**7. How a live gesture keeps animation live.** The interaction between a binding
or gesture that is still being held and a timeline that is running. This is the
item that makes the phase depend on phase 03, because it is a question about
whether a binding fires once or continuously.

**8. The interaction with the solver's geometry.** Phase 07 item 4. An animation
runs between two solved states, so it needs to know which two, and whether it
re-solves mid-flight.

## What it resolves

- Whether a timeline is a config value and what it looks like, which is the
  project's central claim: that anything is reconfigurable through the API.
- The boundary between the solver's output and a visual interpolation, which
  phase 07 needs in order to say whether the block carries an arranged layout.

## Why it is here

It reads node properties from phase 08, decoration properties from phase 09, and
solver geometry from phase 07. It is last of the three look phases because an
animation over a property nothing else declares is not a design anyone can check.

## Done when

- A timeline is a store value with a tag and a namespace, and `configstorage.md`
  §4 lists it.
- The property list is reconciled against phases 08 and 09 with no contradictions.
- The gesture-versus-timeline question has an answer, and `input.md` §7's
  key-repeat item is closed at the same time or explicitly linked to it.
