# Phase 04: `monitor.md`

Stub. Not started.

Stage 5, P1. This stub used to open by saying `tags.md` §9 deliberately left the
monitor's ordered tag list storage to this document. It no longer does: `tags.md` §5
now assigns the list to `wm.monitor.<id>.tags` and keeps both the semantics and the
storage, because a promise to hand storage to a document that has not been written
is how a tag list ends up owned by nobody. The handoff is therefore **reversed**,
and this document reads `tags.md` rather than the other way round.

## What it should contain

**1. Output hotplug, and how it reconfigures layout.** What happens to a
monitor's tags, its clients, and its layouts when an output appears or
disappears. This was framed as the reverse of "a tag is movable between monitors
carrying its clients and its state", and that framing is now wrong: `tags.md` §3
makes a per-monitor tag's monitor part of its identity, so a tag cannot move and
hotplug is not the reverse of a move. The real cases are narrower and worth
writing out: a monitor's tags are destroyed with the monitor, a client's membership
in them goes with them, and what the user is left with depends on whether
`tags.md` §7's swap ran first. That last part is a policy question — a compositor
may want to migrate a disappearing monitor's clients onto another output rather
than destroy them — and it is a real decision this phase has to make rather than
an inference. Note that migrating them is a sequence of `swap_tags` calls, not a
move, so the operation already exists and the question is only whether to perform
it and when.

**2. Per-output configuration, and the matching vocabulary.** Matched by name,
make, model and serial, which `parse_config.h:164-182` (`ConfigMonitorRule`) also
uses. The interesting part is precedence: a config naming both a serial and a make
is two rules, and which one wins is not derivable. `input.md` §5.1 hits the same
question for input devices, where Mango matches name or `vendor:product:name`
first and then type (`keyboard.c:33`), so the two documents should agree on the
shape of the answer even though the subjects differ.

**3. The monitor's ordered tag list, read rather than defined.** This was the
handoff and it is closed. `tags.md` §4 owns the ordering semantics and §5 owns the
storage at `wm.monitor.<id>.tags`, with a tag at
`wm.monitor.<id>.tag.<n>`. What is left for this document is the half that is
genuinely the monitor's: whether the list is a value or a child list, how it is
seeded at hotplug, and how a monitor's other per-output state and its tag list stay
consistent with each other. The dependency now runs monitor-after-tags, so this
phase should cite `tags.md` rather than being cited by it.

**4. Virtual monitors.** Mango's `isoverview` and its `MON_PREV`/`MON_NEXT` sentinels
are the reference. Whether a virtual monitor is a tag instead is a live question,
because `tags.md` §8 already makes overview and scratchpad tags.

**5. Xwayland integration, and its tag, layout and decoration parity.** The
parity requirement is the substance: an X11 client's tag membership, its layout
slot, and its decorations all have to come from the same sources as a Wayland
client's, or the compositor has two truths.

## What it resolves

- Where a monitor's tag list lives. **Closed in `tags.md` §5** at
  `wm.monitor.<id>.tags`, so this is no longer a deliverable of this phase; what
  remains is the consistency between that list and the monitor's other per-output
  state, which is a genuine part of this document.
- Per-output rule precedence, which `input.md` §5.1 also needs and which should
  not be decided twice.
- Whether virtual monitors are monitors or tags, which is currently answered
  differently by `tags.md` §8 than by Mango.

## Why it is here

It is stage 5 and it gates phase 05 on one side: `tags.md` owes per-output tag
defaults, and those are meaningless without the matching vocabulary. But phase 05
now gates it on the other side, because `tags.md` §5 defines the shape this
document has to be consistent with. So this phase has a genuine two-way
relationship with phase 05 and the matching vocabulary (item 2) is the part that
can be written first, since it is the part that does not depend on the tag list.
It does not need the input document, so it can run in parallel with phase 03.

## Done when

- This document cites `tags.md` §4 and §5 for the ordering semantics and the
  storage shape, rather than the two documents each citing the other.
- The hotplug policy is written down: whether a disappearing monitor's clients are
  destroyed or migrated by a sequence of swaps, and what happens to its tags in
  either case.
- The per-output rule precedence answer is written once and referenced from
  `input.md` §5.1 rather than restated.
- The X11 parity requirement is stated as a requirement, so stage 5's
  implementation cannot satisfy it by accident.
