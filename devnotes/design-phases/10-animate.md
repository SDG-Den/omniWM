# Phase 10: `animate.md`

*Not expected to be accurate about other documents until this phase starts; see
[README.md](README.md).*

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

**8. The interaction with the solver's geometry.** Phase 07 item 4, now partly
settled and no longer this phase's to decide. An animation runs between two solved
states. Phase 07 has since decided the storage half: both stages are written to
`OMNI_SECTION_SOLVED_LAYOUT` in sequence, and a `stage` byte in the section header
says which one is currently there, so the block is the single authority on live
geometry and this phase reads it rather than re-deriving it. What is left here is
the animator's own behaviour against that section, which is item 9.

**9. What a second update does while an animation is running.** Deferred here from
`layoutengine.md` §3.7 and §5 on purpose, and this is the phase that owns it.
Three answers with three different visual results, and the choice is a statement
about what a client shows while a transition is in flight rather than about
geometry:

- **retarget**: the running animation's endpoint becomes the new layout, so the
  client continues from where it is and never returns to the intermediate.
- **queue**: the second layout is held and started when the first finishes, so
  every intermediate is shown in order and a rapid sequence of focus changes walks
  through each layout rather than skipping.
- **replace**: the running animation is abandoned and a new one starts from the
  current on-screen position, which is retarget with the progress reset.

The constraint from phase 07 is that `layoutengine.md` §5's step 9 runs
unconditionally, so whatever is chosen here may not leave a pass without reaching
`OMNI_SOLVED_STAGE_ARRANGED`. That is what makes all three safe, and it is why
this item is a design question about a client-visible value rather than a hazard
in the pipeline. Related: whether the animator re-solves mid-flight, and whether
an interrupted transition leaves a client at an interpolated position that no
layout ever claimed, which is the state `layoutengine.md` §3.7 says is neither a
solved nor an arranged layout and must therefore be owned by this document.

**The scratchpad is the fourth case, and it is not a variant of the three above.**
`tags.md` §8.2 makes the scratchpad an exclusive tag with an overlay layout of its
own, so opening it changes which clients exist as visible surfaces rather than
where existing ones are. A client appearing has no previous geometry to interpolate
from and a client leaving has no endpoint to travel to, so none of retarget, queue,
or replace has anything to say about it, and an implementation that routes a
scratchpad toggle through the normal animate path will animate from a stale
rectangle or from zero, depending on which the missing case happens to fall into.
The answer is most likely that the scratchpad's overlay layout opts out of the
animate step while still writing `OMNI_SOLVED_STAGE_ARRANGED`, since §5's step 9
runs unconditionally and the byte has to be right whether or not anything moved.
That is a decision for this phase rather than for `tags.md`, and it should be
recorded as one so that "the scratchpad overlay" is a named thing the animator knows
about rather than a layout that happens to have no geometry.

## What it resolves

- Whether a timeline is a config value and what it looks like, which is the
  project's central claim: that anything is reconfigurable through the API.
- The animator's behaviour for a mid-flight update, which phase 07 deliberately
  does not answer, and the ownership of the interpolated position that is neither
  a solved nor an arranged layout.

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
- Item 9 has an answer, and `layoutengine.md` §3.7's and §5's two entries for it
  are struck rather than left describing a question this phase owns.
