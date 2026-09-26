# Phase 03: `input.md` finalization

Stub. Not started. The document exists; this phase is what it still owes.

`input.md` was written as a porting map. The design work left in it is four
items, and one of them is the largest remaining question in the input subsystem.

## What it should contain

**1. The mode and tag mapping, and this is the phase's substance.**
`input.md` §7.1 sets out four candidates with their costs and does not choose:

- drop scoping entirely
- scope to a tag
- scope to a client
- keep a mode string, renamed

Mango's `mode` is a 28-byte string with two sentinels, `common` and `default`,
switched at runtime by an action. omniWM has no global mode, so the question is
what a binding is *scoped to*. What is already settled and constrains the answer:
`generaldesign.md` §14.4 puts `mode_id` in the index key, so the answer is a u32
and not a string compare on the hot path.

**2. The device-rule tag and namespace.** `input.md` §5.1 places Mango's
`ConfigDeviceRule` as a match-and-apply record, and §7.2 notes it has no tag.
`client_rule` at `0x33` cannot be reused, because its fields are layout
vocabulary. A new tag number is needed and it is not chosen.

**3. The stylus binding kind.** Mango has `tablet.c` and the full `tablet_v2`
protocol, and no binding struct to copy, so this is fresh work rather than a
port. It needs pressure and tilt thresholds, absolute versus relative pointing,
and an annotation mode.

**4. Whether `binds.c` is one component or five**, which §7 leaves open. Five
trigger kinds with per-kind `enable_key` is a real option and nothing needs it at
stage 4.

Two smaller items ride along: whether key repeat should fire bindings at all, and
where a binding's `spec` field is rendered given that `ipc.md` has no `get binds`
equivalent.

## What it resolves

- The last substantive open decision in `generaldesign.md` §14, which the document
  itself lists as owed.
- The `binding` record's byte offsets, which `configstorelayout.md` §4 defers
  precisely because stage 10 grows the header. Once the stylus kind is designed,
  the offsets can be frozen rather than deferred again.
- Whether the action registry's argument schema covers a binding argument
  uniformly, which `helpers.md` §6.2 assumes.

## Why it is fourth

It reads `helpers.md` §6 and §7 and `configstorelayout.md` §4, so it needs phase
00. It does not need any of the missing documents, and nothing later needs it
except through the `binding` offsets. It sits after the build phase because the
stylus design is easier to settle knowing what the tablet plumbing will link
against.

## Done when

- One of the four scoping options in `input.md` §7.1 is chosen, and the three
  rejected ones are recorded with the reason, the way `input.md` §7.1 already
  records Mango's `tc` field landing as an `entry_ref`.
- A tag number exists for a device rule and `omni_layout.h` carries it.
- The `binding` record has a final header, and `configstorelayout.md` §4's
  "offsets are still deferred" note is replaced with the offsets.
