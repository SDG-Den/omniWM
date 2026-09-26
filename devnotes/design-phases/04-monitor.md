# Phase 04: `monitor.md`

Stub. Not started.

Stage 5, P1. `tags.md` §9 deliberately left the monitor's ordered tag list
storage to this document, so there is an existing handoff to pick up.

## What it should contain

**1. Output hotplug, and how it reconfigures layout.** What happens to a
monitor's tags, its clients, and its layouts when an output appears or
disappears. `tags.md` §4 makes a tag movable between monitors carrying its
clients and its state, so unplugging is the case where that has to work in
reverse.

**2. Per-output configuration, and the matching vocabulary.** Matched by name,
make, model and serial, which `parse_config.h:164-182` (`ConfigMonitorRule`) also
uses. The interesting part is precedence: a config naming both a serial and a make
is two rules, and which one wins is not derivable. `input.md` §5.1 hits the same
question for input devices, where Mango matches name or `vendor:product:name`
first and then type (`keyboard.c:33`), so the two documents should agree on the
shape of the answer even though the subjects differ.

**3. Where the monitor's ordered tag list is stored.** This is the handoff.
`tags.md` §4 owns the ordering semantics and §9 says the storage is deliberately
left here, so the list and the monitor's other per-output state stay in one place
rather than being split across two documents.

**4. Virtual monitors.** Mango's `isoverview` and its `MON_PREV`/`MON_NEXT` sentinels
are the reference. Whether a virtual monitor is a tag instead is a live question,
because `tags.md` §8 already makes overview and scratchpad tags.

**5. Xwayland integration, and its tag, layout and decoration parity.** The
parity requirement is the substance: an X11 client's tag membership, its layout
slot, and its decorations all have to come from the same sources as a Wayland
client's, or the compositor has two truths.

## What it resolves

- Where a monitor's tag list lives, which `tags.md` §9 is blocked on.
- Per-output rule precedence, which `input.md` §5.1 also needs and which should
  not be decided twice.
- Whether virtual monitors are monitors or tags, which is currently answered
  differently by `tags.md` §8 than by Mango.

## Why it is here

It is stage 5 and it gates phase 05: tags.md owes per-output tag defaults, and
those are meaningless without the matching vocabulary. It does not need the
input document, so it can run in parallel with phase 03.

## Done when

- `tags.md` §9's "where a monitor's ordered tag list is stored" has a pointer here
  rather than being open.
- The per-output rule precedence answer is written once and referenced from
  `input.md` §5.1 rather than restated.
- The X11 parity requirement is stated as a requirement, so stage 5's
  implementation cannot satisfy it by accident.
