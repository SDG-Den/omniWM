# TOML config parser (devnotes/tomlparser.md)

The built-in TOML parser is a facade, not a privileged path. It runs
in-process and is a client of the same block every other surface uses
(`configstorage.md` §11), and it has no write capability that a socket client
lacks. A value written from a config file and the same value written over IPC
are byte-identical in the block, which is the property that lets a user switch
between the config file, the socket, and the block without a semantics change.

Everything here binds TOML literals to the type tags in
`configstorage.md` §4 and the wire forms in `ipc.md` §3. It adds no tags of its
own and no key-specific handling: unknown keys are stored and served verbatim,
per the open-catalog rule.

## 1. What a config file is

A config file is a sequence of `set` and `exec` operations, per
`configstorage.md` §13. The parser's whole job is to turn TOML into that
sequence of typed values. It does not decide what the keys mean, validate them
against a schema, or reject an unknown key.

A TOML document is therefore read as:

- a top-level table, whose keys are config keys, with dotted keys nesting the
  namespace (`wm.gaps` is a TOML dotted key or a nested table, and both spell
  the same config key);
- arrays of tables, which are config keys whose values are `array`;
- values whose type comes from the key's registration where one exists, and from
  the literal otherwise.

Tables that are not addressed by any component are still parsed and stored. A
user may keep their own state in the block under their own namespace, and the
parser has no opinion about it.

## 2. Type binding

| TOML | tag | note |
|---|---|---|
| boolean | `bool` | direct |
| integer | declared by the key | see §3 |
| float | `f64`, or declared tag | see §3 |
| string | `string` | see §4 for suffixed forms |
| array | `array` | `elem_type` from the elements; see §5 |
| inline table | `tuple` | `field_types` from the field order; see §6 |
| table | `tuple` | same as inline table |
| array of tables | `array` of `tuple` | |
| local date, local time, local datetime, offset datetime | `datetime` | see §7 |
| local date-time | `datetime` | interpreted in local time, see §7 |

TOML's date and time types all land on the single `datetime` tag, which stores
nanoseconds since the Unix epoch in UTC. The four TOML forms differ only in how
much information they carry, and the missing parts are filled by TOML's own
rules: a bare local date is midnight local, a bare local time is today local,
and a local date-time is that wall-clock time in the machine's zone.

Four tags are not inferred from the TOML value shape, because the TOML shape
does not determine them: the tag comes from the key, the same rule §3 already
states for integers.

| key holds | tag | note |
|---|---|---|
| a binding table | `binding` | `args` is an array of strings, absent is none |
| a client rule table | `client_rule` | `rule`/`against`/`edge`/`axis`/`width`/`height`/`by`/`region` |
| a layout program | `constraint` | the tag is `0x32`, named `constraint` in `configstorage.md` §4 |
| a window rule table | `map` | a rule block, `if` and `then`, at `omniwm.window_rules.<name>` |
| a space, or a rule inside one | no tag of its own | a space is an `option` (`0x25`) and a rule is a named key of it |

There are no `layout`, `layout_rule` or `space` tags. An earlier draft of this
table listed all three, and they named nothing that exists: the store has no
`layout` tag, a program is `constraint`, and a space and a rule are the two
levels inside a `constraint` value rather than values in their own right
(`configstorage.md` §4, `omni_layout.h` section 7). A TOML table for a space is
therefore an `option` table and a rule is a key within it, and the tag a table
binds to is the one §3 declares for its key, not a per-level tag invented here.

A binding, a layout program, a client rule table and a window rule table are all
tables, and §6 would
otherwise bind each to a `tuple` whose `field_types` come from field order. That is
wrong for all of them: the fields are named, and a positional encoding would make a
layout file unreadable and a reordering silent. So each binds to its own composite
tag with the field names preserved, and the tag itself is declared by the key per §3.

The layout keys live under one prefix, `omniwm.layouts.<name>`, and the two rule
namespaces are `omniwm.clients.<set>.rules` and `omniwm.snaps.<set>.rules`
(`layoutlanguage.md` §3.6.1). So a whole program is four keys and nothing else:
three structural ones and the fourth (`rearrange_on_focus`), a plain boolean
rather than a table, which is why it writes inline while the structural keys
write as themselves. `.rules` and `.spaces` are `[[...]]` arrays, and
`.viewport` is an inline table holding a two-field option:

```toml
[[omniwm.layouts.delta.rules]]
rule = "run"
axis = "x"
gap = 8

[[omniwm.layouts.delta.rules]]
rule = "push"
keep = "old"
to = "side"

[[omniwm.layouts.delta.spaces]]
name = "main"

[[omniwm.layouts.delta.spaces]]
name = "side"
layout = "vertical_stack"

[omniwm.layouts.delta.viewport]
offset = [0, 0]
ratio = 1.0

[omniwm.layouts.delta]
rearrange_on_focus = false
```

The nested `layout` field on a space is the nesting mechanism, §5 of
`layoutlanguage.md`: a space carrying a `layout` is a group and one without is not,
so there is no `groups` array to write and no second rule list to declare. That
`side` space above is a group holding the whole `vertical_stack` program, and its
rules live under `omniwm.layouts.vertical_stack.rules` rather than here.

This is the cross-document consequence `layoutengine.md` §3.3 flagged, and it is now
answered rather than deferred: whatever a constraint language ends up being, a TOML
user has to be able to write it, and it cannot be a `tuple`. `layoutlanguage.md` is
normative for the keys, `configstorage.md` §4 for what is stored once they parse, and
this section only has to say that the tables bind to named-field composite tags
rather than to positions.

## 3. Numeric typing comes from the key

A TOML integer is 64-bit signed, and the block has `i8` through `i64` plus the
unsigned widths. A literal `8` therefore does not determine its own type, and
inferring the narrowest width that fits is wrong: `gaps = 8` and `gaps = 300`
would then be different types, and anything reading that key would have to
handle both.

The type comes from the key's registration instead. A component declares each
option's tag in its descriptor (`helpers.md` §3.1), and the parser looks the key
up before decoding:

- key is registered: decode to the declared tag. A literal that does not fit it
  is a line failure.
- key is not registered: no declared type exists, so a float literal becomes
  `f64` and an integer literal becomes `i64`.

This is the same rule the socket applies to a `set` that omits `type`
(`ipc.md` §4), which is what keeps the two surfaces consistent. A key's stored
type is therefore stable as its value changes, and a consumer can rely on the
declared width.

A user who wants a specific width regardless of registration uses the explicit
table form, which carries the tag:

```
wm.gaps = { type = "u32", value = 8 }
```

The table form is the one escape hatch, and it is checked against the declared
type for a registered key exactly as `ipc.md` §4 requires. It exists so a
config author can be explicit, not so they can work around a component's
declaration.

## 4. Strings, and the two suffixed forms

A plain TOML string is `string`, and that is the default for everything.

Two forms are recognised by suffix, because both are common enough in a
compositor config that requiring a table for them would be hostile:

```
500ms     duration
2s        duration
100us     duration
500ns     duration
1500      duration, defaulting to ms
```

An unsuffixed numeric string is a `duration` in milliseconds, which is the
convention other window managers use and is granular enough to be an integer
without being overbearing. A `duration` is nanoseconds in the block and is
carried as a wide integer over IPC (`ipc.md` §3.1), so a value written from a
config file and the same value read over the socket agree exactly.

The table form remains available for an explicit unit and for a value that does
not fit the shorthand:

```
wm.animation = { type = "duration", value = { "$u64": "1500000" } }
```

A string that is not a valid suffixed duration is an ordinary `string`. There is
no failure here, because guessing wrong would make `wm.title = "1500"` unusable
as a title.

## 5. Arrays

An array is `array` with one `elem_type`, so the elements must agree. A
heterogeneous array has no representation and is a line failure with the line
number, rather than a coercion that would silently discard a value.

An empty array has no elements to infer from, so `elem_type` must be given:

```
wm.key_order = { type = "array", elem_type = "string", value = [] }
```

This is why `elem_type` is explicit in the wire form (`ipc.md` §3.2): an empty
array is representable only because the element type travels with it.

## 6. Tables

A table or inline table is a `tuple` whose `field_types` come from the field
order, with dotted keys flattened into the same namespace as any other key:

```
wm.border = { width = 2, color = "#3d6bff" }
```

becomes a `tuple` with `field_types` `["u32", "rgba8"]` once each field is bound
to its registered type. As with arrays, a table nested inside an array is a
`tuple` inside an `array`, and the discriminators make that unambiguous.

The rule above has the exception §2 states, and it is not a small one: a table
whose key is declared composite binds to that composite tag with its field names
preserved, not to a `tuple`. The keys are `binding`, `constraint`, `client_rule`
and `map`, and they cover every table whose fields are named rather than
positional. A positional `tuple` is the default, not the answer for a table whose
field names the user wrote down.

A table is a value, not a container of independent writes: assigning a table is
one commit replacing the whole value, not a merge. A user who wants a merge
writes the fields they mean to change, and the whole-value replacement is what
makes a config reload's effect predictable.

## 7. Datetimes

All four TOML date and time forms become `datetime`, stored as nanoseconds since
the Unix epoch in UTC and normalised on the way in, so two datetimes naming the
same instant from different offsets are the same value. Over IPC a `datetime` is
an RFC 3339 string with an explicit offset, and the encoder emits UTC with `Z`.

A `datetime` is 8 bytes and uses `value_inline` like any other 8-byte scalar, so
it costs no arena frame. A local date-time with no offset is interpreted in the
machine's current zone, which is TOML's own rule and is also why such a value is
not portable between machines; a config that must mean the same instant
everywhere writes an offset.

## 8. Parse and commit behaviour

The parser is a `reload` implementation, so it inherits the transaction
guarantee from `ipc.md` §6. The file is parsed into a private staging area and
fully bound to typed values before anything is published; a file that fails to
parse never begins a commit and the live block is untouched. On success the
whole file is published as one grouped commit with a single `commit_id` and a
`COMMIT_END`, so a reader sees the complete old state or the complete new one.

Line failures are tolerated and the rest of the file loads. The config fails per
line, never as a whole: a failing line is logged with its line number, counted in
the response, and not applied. The one whole-file failure is exceeding
`OMNI_CONFIG_MAX_OPS`, which is a structural limit and is reported as
`CONFIG_TOO_LARGE` with the count.

The guarantee covers the block. A config line that runs a program, an `exec`
action, has effects outside the transaction, and those are not rolled back if a
later line fails.

## 9. Output

The parser is also the `save` implementation, and its output is TOML, so a saved
config is reloadable. `save` writes every live entry with `WINDOW_DEPENDENT` and
`EPHEMERAL` both clear (`configstorage.md` §13), which is the same classification
a soft reset uses.

Output preserves the declared type of each key, so a `u32` writes as a bare
integer and reloads as the same `u32` through the key's registration. Values
whose type came from the literal rather than a registration, such as an
unregistered `f64`, round-trip to the same tag. A value that cannot be expressed
in TOML at all, such as a `blob` or an `option(None)`, is written in the
explicit table form, which is why that form exists in the input direction too.

## 10. Relationship to other facades

- The parser is a facade in exactly the sense `configstorage.md` §11
  describes: in-process, over the block, a client rather than a peer. §12 is the
  guard rules and is a separate concern, applied to whatever the parser writes.
- It reuses the typed-value encoder and decoder from `ipc.md` §3 verbatim. There
  is no second value format, which is the reason a value does not change meaning
  when it arrives through a different surface.
- The socket, the in-process helper, and this parser all write the same entries
  through the same commit path, so a grouped commit from a config file and a
  grouped commit from the socket are indistinguishable to a watcher.
- A program that would rather not use TOML at all does not interact with this
  parser. It writes to the block directly, which is the extension mechanism
  (`helpers.md` §8.1).

## 11. Open items

- The full TOML grammar is not restated here. This document defines the binding
  to the type system and the commit behaviour; the specification itself is
  upstream and the parser is expected to conform to it except where this
  document says otherwise, which is only in the type binding and the two
  suffixed forms.
- **A missing or blank key falls back to a static default, and the fallback is
  per key.** This is now closed rather than open, and it is worth stating why it
  is a parser concern and not a store one. The four program keys
  (`layoutlanguage.md` §1) and a space's `layout` field are the units, and each
  is independent: a program with `rules` and `spaces` and no `viewport` gets the
  static identity viewport, a program with no `rearrange_on_focus` gets the static
  `true`, a program with `viewport` and no `spaces` gets no spaces, and no absence
  is an error. Two properties follow, and both matter more than the rule itself.
  - The defaults live in code, not in the block. A consumer that finds a key
    absent applies the compiled-in default for that key; the block never carries a
    seeded copy of it, never re-applies a default into an entry, and never
    records that a default was in force. This is the direct consequence of
    `configstorage.md` §12's decision that the store holds no defaults: a default
    written into the block would be indistinguishable from a user's explicit
    value, so a later change to the default could not reach anyone who had not
    overridden it, and `save` would write out a value the user never chose.
  - The fallback is per key, not per program. There is no "the program is
    incomplete" state, and no failure mode where one missing key discards the
    others. A partially specified program is the normal case: `layout-test-examples.md`
    is written in that form throughout, and a program that wants the defaults for
    everything omits all four and gets the blank canvas of
    `layoutengine.md` §7.8.
- A key present but malformed is **not** a fallback case and is not covered by the
  rule above. Blank is blank; a `spaces` array containing a table that is not a
  space is a parse error for that line, and it is reported as such per §8 rather
  than silently replaced by the default. The distinction matters because a
  fallback is a decision about a key the user did not write, while replacing a
  malformed key would discard something the user did write and did not mean.
- Whether a bare unsuffixed numeric string should be a `duration` or an ordinary
  `string` is decided here in favour of `duration`, on the grounds that the
  overwhelmingly common intent for a bare number in a compositor config is a
  millisecond duration. A user who needs the string `"1500"` writes the table
  form.
- Arithmetic on `datetime` values, if a later design wants a config to compute
  an interval, is not in scope here. `duration` and `datetime` are both integers
  in the block, so the arithmetic is possible, but the config language exposes no
  expression syntax and this document does not add one.
