# Decisions recorded 2026-09-29

These are the answers given to the five open decisions presented on 2026-09-29.
They are integrated into the owning stubs; this file is the raw record.

## 0. A general principle, stated for the whole core

The toolkit goal is not specific to rendering. It is the goal for all of the core
lib: a simple, generic feature set that cannot do *that* much by itself, but
serves as a foundation to build new features extremely quickly, so omniWM can
realistically support the massive feature set it wants. "Absolutely no feature
cheating" is therefore not the design; small generic primitives plus fast
composition is the design. This governs the unit-split decision (which units
belong in the core versus the compositor layer) and every later "should this be
in core" question.

This is a rule about what a unit implements, not about what the library links.
Corrected 2026-10-02, from a mistaken reading: `libomniwm` is the **internal**
development library omniWM itself is built from, and it links wlroots, scenefx,
and the rest of the Mango list directly. It is not a hand-off surface; the
external library, which does not exist yet, is a separate SHM-client library
whose only dependency is the block layout in `include/shared/`. The toolkit rule
decides whether a *feature* belongs in core; the dependency list is settled by
"the WM is built on these", which is a stronger answer.

## 1. B3, the scenefx extension set

No fork and no patch. The extensions are built into omniWM instead. This is an
explicit decision to break ground rather than track upstream.

Generic implementation target, corrected 2026-09-29: not a single flexible
function — a small set of simple, generic, flexible primitives, a toolkit. Super
specific, complex effects do not belong in core; the toolkit is what lets any
looks-based feature be implemented rapidly later. A preset is a mapping of
several primitives onto one config key. Feature list:

- 3d transform windows
- 2d transform windows
- above-background and above-windows full-size custom-render buffer support
- multi-layer borders
- textured borders (tile and stretch-to-fit)
- nine-patch support
- shadered border support (gradients and similar)
- custom shader effects
- custom shader animations
- animated shaders on borders
- glow and shadows (one feature underneath, exposed two ways)
- window opacity
- custom GLSL shader support
- all buffers available in SHM for direct modification

All of it in-GPU. Accepts that a different higher-level rendering library may
have to be integrated into the WM to support and extend scenefx. The render layer
is a core service, and `libomniwm` carrying scenefx and the render stack is
normal for an internal library, per §0's correction.

## 2. input.md §7.1, the mode and tag mapping

Keymodes, Mango-shaped for the user, different underneath. A binding
*optionally* carries a `keymode` field. The field may hold a list of several
keymodes, or the config key may be repeatable. A binding with no keymode is in
`default` by implication.

Latency rule: by default only listen on keys for bindings with no modifier in
the current mode. If modifiers are held, also listen on keys from bindings that
carry those modifiers in the current mode. A key with no binding is passed
through immediately without traversing the match machinery.

Client targeting is **not** a binding scope question. It is independent of mode.
(Confirmed: it is the action's argument, as §7.1's analysis and Hyprland's
`window =` dispatcher selector both imply.)

## 3. B4, the solver arithmetic width

Fixed-point internally, integer at commit. Determinism outranks architecture
breadth: the project builds for x86_64 and aarch64 is only a Nix default, and a
deterministic solver matters more than aarch64 support (aarch64 compatibility is
still wanted, just not at the cost of determinism).

The user-facing cost is absorbed in parsing, not in the block: the TOML parser
interprets float literals into fixed-point, so TOML stays easy to write. A SHM
client deals with the stored representation directly, which is the trade for
much lower-level access, and is presumed able to read the documentation.

## 4. input.md item 3, the stylus binding kind

Pursue full pen support. A distinct trigger kind for drawing tablets.

Open, and asked back: whether the same binding mechanism can also cover the
buttons on a drawing tablet and gamepad buttons, or whether those need separate
implementations. This turns on whether a standard exists. **Research pending.**
