# Build (devnotes/build.md)

This document is a stub, and the stub is deliberate. It exists now because one
fact has to be citable today: **how the C in this repository is compiled.** That
fact is not inferable from the tree, and three separate verification paragraphs in
`design-phases/00-reconciliation.md` had already gone stale for want of it.

The rest of the document is phase 02's build half
(`design-phases/02-substrate.md`, items **B1** to **B7**), which owns the
dependency set, the wlroots and scenefx version coupling, the exact scenefx
extensions needed for user GLSL shaders and 3D transforms, the solver's
arithmetic width, the Meson structure, and the `flake.nix` with
**`cache.nixos.org` set explicitly as the substituter**. None of that is written
ahead of the phase that decides it. §1 is the only section that exists until
then, and other documents should cite §1 rather than restate an invocation.

## 1. Compiling the ABI header

There is no compiler on `PATH` in this environment: no `gcc`, no `cc`, no
`clang`. The compiler is reached through Nix, and the invocation that works is:

```
nix run nixpkgs#gcc -- -std=c11 -Wall -Wextra -Iinclude -fsyntax-only -x c \
    include/shared/omni_layout.h
```

Run it from the repository root. As of 2026-09-27 it exits 0 with no output
against GCC 15.3.0. The gcc that command resolves to is already realised in the
Nix store, so the run is a store lookup: it fetches nothing and writes nothing to
the environment or to the working tree. The fact is recorded because the argument
this file makes about a criterion has a second half, and a check a reader has to
weigh before running it is a check that does not get run.

The flags after the `--` describe the task rather than the project's real build
flags, and each one is there for a reason:

- `-std=c11` — the header is C11.
- `-Wall -Wextra` — a clean compile means no output at all, not merely a zero exit.
- `-Iinclude` — the project include root. The header needs nothing from the tree
  today (it includes only `<stdint.h>`), so this flag is not currently load-bearing
  and is here so the command does not have to change when a header starts
  including a project header.
- `-fsyntax-only` — parse and check, do not emit. The target is a header and there
  is nothing to link.
- `-x c` — force the language. Without it a `.h` file is taken as a C++ header,
  which is a different standard with a different set of warnings, so the check
  would not be the check it claims to be.

A zero exit with no output is a pass over **every `OMNI_STATIC_ASSERT` in the
file**, because those are the header's only executable content. It is therefore
the check that the byte-level constants `configstorelayout.md` §2 is built on and
the `OMNI_STATIC_ASSERT`s in `include/shared/omni_layout.h` agree with each other,
which is the one property in the design that is verified rather than asserted.

This section is what `design-phases/00-reconciliation.md` should cite when it
claims the header compiles. Citing it rather than repeating the invocation is not
tidiness: the three copies the file carried had each drifted into a form that
could not be run at all, and a criterion that cannot be run as written reports a
pass that was never demonstrated.

## 2. What phase 02 adds

The gap was recorded here on 2026-09-27 and is now partly closed. There **is** a
build system in the tree: `flake.nix`, `meson.build`, `meson_options.txt`,
`protocols/` and `nix/`, with `nix build`, `nix run`, `nix flake check` and
`nix develop` all working. It was written in phase 01, because a test document
with no runner in the tree is a specification nothing checks.

What the build does **not** do is build a compositor. The `omniwm` executable is
disabled in `meson.build` while all 62 source files are zero bytes, so
`nix build` installs the ABI header and `nix run` runs the header check. The
section 1 invocation above is the same compile, wrapped so that `nix flake check`
performs it on every evaluation.

### 2.1 The five packages

`meson.build` defines four targets and the flake exposes five packages. The
split is not cosmetic, so it is worth stating what each one is for:

| Package | Target | Propagates | Notes |
| --- | --- | --- | --- |
| `libomniwm` | `libomniwm.so` | — | The **internal** development library omniWM itself is built from: it links wlroots, scenefx, libplacebo (§4.1), and the whole Mango list, because the WM is built on them. Not a hand-off surface. |
| `libomniwm-ipc` | `libomniwm-ipc.so` | `libomniwm` | The socket facade, `ipc.md` §7.1. |
| `libomniwm-debugger` | `libomniwm-debugger.so` | `libomniwm` | Reads the block. Nothing links it into the compositor. |
| `omniwm` | `libomniwm-ipc.so` | `libomniwm-ipc` | The compositor once `src/main.c` has a `main`. |
| `omniwm-debug` | `libomniwm-ipc.so` | `libomniwm-ipc`, `libomniwm-debugger` | The above, with ASan and the debugger in the closure. |

A future external **SHM-client library** for other software is a separate
package, not one of these. Its only interface dependency is the block layout in
`include/shared/`, which is exactly why the ABI header is dependency-free
(`filestructure.md`); it will sit on top of the block and the socket, and it is
not `libomniwm` under a friendlier name.

The release/debug distinction is a closure property, which is the part that is
easy to get wrong and worth checking rather than asserting:

```
$ nix path-info -r .#omniwm       | rg -o 'omniwm[a-z-]*-nightly' | sort -u
omniwm-ipc-nightly
omniwm-nightly

$ nix path-info -r .#omniwm-debug | rg -o 'omniwm[a-z-]*-nightly' | sort -u
omniwm-debugger-nightly
omniwm-ipc-nightly
omniwm-nightly
```

A debugger that the release WM can reach is not a separate package, it is a
flag. Keeping the dependency edge in `propagatedBuildInputs` rather than in
`buildInputs` is what puts it in the closure of the debug build only.

Two things make this work and both are non-obvious enough to record:

- **`install_tag`, not `meson install` target selection.** `meson install`
  installs every target carrying `install : true`; there is no per-target
  selection other than tags. So `nix/common.nix` passes
  `--tags <tag> --no-rebuild` and `meson.build` sets `install_tag` on each
  target. Without the tags a package built for one library ends up containing
  all three, and the split is undone at the last step. `--no-rebuild` is
  load-bearing separately: without it Meson re-runs the default target and
  discards the `buildPhase`'s target selection.
- **Dependencies travel by propagation, not by copying.** `libomniwm-ipc`'s
  output contains only `libomniwm-ipc.so`; `libomniwm.so` reaches the consumer
  through `propagatedBuildInputs` and onto its RPATH. Copying it into two store
  paths would produce two files that are equal in every byte that matters and
  diverging in every byte that does not.

`libomniwm` builds and links cleanly today with every source in it empty. It has
no symbols. The link is real and the contents are not, which is the whole
reason the executable is disabled: an empty `main` does not link, and a build
that fails for a reason unrelated to the design teaches a reader to ignore
build failures.

`omniwm` currently builds `libomniwm-ipc.so` rather than an executable, so its
output is not yet a compositor and cannot be run. The `omniwm` target in
`meson.build` is commented out with the reason inline.

### 2.2 A cost of the current source expression

`nix/common.nix` sets `source = builtins.path { path = ../.; }`, which hashes the
whole repository — `devnotes/` included. The consequence is measurable: editing
this file invalidates all five packages, and `nix path-info -r .#omniwm` then
reports that three derivations will be built. It was observed while writing this
section, which is why it is written down.

This is not a bug, and nothing in the design depends on it. It is a cost worth
naming before someone rediscovers it as "why did editing a document start a
three-minute build". The fix is a filtered source that excludes `devnotes/`,
which is a small change to `nix/common.nix` and not a design decision; it has not
been made because it was not asked for.

The claims that were recorded here and still hold:

- The invocation in section 1 is the authority on the ABI header compiling. The
  derivation in `nix/abi-check.nix` duplicates those flags on purpose, and the
  duplication is the risk this paragraph exists to flag: a second copy of a
  command is a second thing to drift, so if the flags in section 1 change, both
  copies have to change.
- A clean build is not yet evidence of anything beyond the header. It is not a
  claim that the project builds, and it is not a claim that the test suite passes,
  because the test suite does not exist yet. `01-testing.md` owns the assertion
  set that will make it mean something.

`02-substrate.md` B5 is done: `nixConfig.extra-substituters` names
`https://cache.nixos.org`. B1 and B2 are closed in §3, B3 in §4 and its
subsections, B4 in §5, and B6 in §6. B7 is the one class of item that is not a
section of its own, and `licence.md` owns it. B3's primitive decomposition and its
node and pass model are closed in §4.2 and §4.3.

**The `flake build` check is deferred to implementation, 2026-10-10.** The
Done-when clause in `02-substrate.md` that `flake build` succeeds on a machine
with no ambient Nix configuration is not run during this design phase. A build
over a tree whose sources are all zero bytes proves nothing about the design, and
a design phase delivers design, so the check belongs to the substrate
implementation, where `libomniwm` has symbols to link and the `omniwm`
executable target exists. The flake itself is complete and the substituter is set
(B5); what is deferred is running the build, not configuring it.

B5 is worth a second sentence. The flake sets `nixConfig.extra-substituters`, and
Nix does not honour flake config from an untrusted tree, so every command prints
`ignoring untrusted flake configuration setting 'extra-substituters'` until it
is passed `--accept-flake-config`. The setting is the right one to have in the
flake and the wrong one to rely on; the flag is what makes it take effect, and
both are in use.

## 3. The dependency set, and the wlroots/scenefx pair (B1, B2)

**B2 is closed: wlroots 0.20.2 and scenefx 0.5.0, taken as a matched pair.**
`generaldesign.md` §19 records the tracked wlroots release as unresolved and the
coupling to scenefx 0.5 as a hard constraint rather than a preference. Both are
released and both are in nixpkgs-unstable, so the pair resolves against released
software rather than against a fork. `flake.nix` pins `wlroots_0_20` rather than
the moving `wlroots` attribute, because the two name the same version today and
will not stay that way.

The base list is Mango's, unchanged, and it is the same list in `meson.build`'s
`dependency()` calls and `nix/common.nix`'s `buildInputs`:

| library | version | optional |
|---|---|---|
| libinput | 1.31.3 | no |
| libxcb | 1.17.0 | no |
| libxkbcommon | 1.13.2 | no |
| PCRE2 | 10.48 | no |
| pango | 1.57.1 | no |
| cjson | 1.7.19 | no |
| pixman | 0.46.4 | no |
| wayland | 1.26.0 | no |
| wayland-protocols | 1.49 | no |
| wlroots | 0.20.2 | no |
| scenefx | 0.5.0 | no |
| libGL | 1.7.0 | no |
| libdrm | 2.4.134 | no |
| libxcb-wm | 0.4.2 | yes, `xwayland` feature |
| xwayland | 24.1.13 | yes, `xwayland` feature |

XWayland is the only optional dependency, and its optionality is a build feature
rather than a call-site `#ifdef`: `meson_options.txt`'s `xwayland` feature drives
the two package entries and the `-DXWAYLAND` compile define together, so a build
without it has no half-enabled XWayland path.

**B1's addition, decided 2026-10-10: libplacebo.** The base list above stays
Mango's, unchanged; libplacebo is the one addition, and it is a consequence of
§4.1 rather than a second item to decide. The exact pin belongs where the
dependency is added — the `dependency()` call in `meson.build` and the
`buildInputs` entry in `nix/common.nix` — not in a table that is defined as
Mango's list. It is non-optional for the same reason scenefx is: the render layer
is a core service and `libomniwm` is the internal library omniWM is built from,
so the render layer's dependencies are the library's normal shape.

**B1's part that is closed: all five packages carry the whole list, and that is
the correct long-term shape.** `libomniwm` is the internal development library
omniWM itself is built from, not a hand-off surface, so it links wlroots, scenefx,
and the rest of the list directly; the WM is built on them. The properly narrow
surface is the separate SHM-client library that does not exist yet, whose only
interface dependency is the block layout in `include/shared/`. §2.1 and
`nix/default.nix` describe the five packages that way, and the "part other
projects link" phrasing is corrected there.

**B1 is now closed.** The addition is libplacebo, named above; §4.1 chooses it and
states what it is for. The earlier "still open" state was whether anything is
added on top of Mango's list, and the render layer is what added it.

## 4. The render layer and the scenefx extension set (B3)

**Decided 2026-09-29: no fork and no patch; the extensions are built into
omniWM.** This is a deliberate decision to break ground rather than track
upstream, and it is forced by scenefx's shape rather than chosen against it:
scenefx has no extension point, its installed public pass API is a closed set of
draw calls, and its shader and compile helpers live in an uninstalled header.
There is no maintained patch to apply and nothing to patch against, so extending
it means owning the render layer that uses it rather than modifying it.

The target is a rendering **toolkit**, not an effect library. Each feature below
is built from a small set of simple, generic, flexible primitives, and a preset
is a mapping of several primitives onto one config key. The core does not
implement super-specific complex effects; it ships the toolkit, so any
looks-based feature can be implemented rapidly on top of it later. The list is
what the toolkit must be able to express, not a set of one-off code paths:

- 3d transform windows
- 2d transform windows
- above-background and above-windows full-size custom-render buffer support
- multi-layer borders
- textured borders, tiled and stretch-to-fit
- nine-patch support
- shadered border support (gradients and similar)
- custom shader effects
- custom shader animations
- animated shaders on borders
- glow and shadows, one feature underneath and two ways to reach it
- window opacity
- custom GLSL shader support
- all buffers available in SHM for direct modification

All of it is in-GPU. That accepts that a different higher-level rendering
library has to be integrated into the WM to support and extend scenefx. §4.1
chooses that library and states the boundary. §4.2 decomposes the list above into
the primitive set and maps presets onto it, and §4.3 writes the node and pass
model that uses both.

### 4.1 The render stack (decided 2026-10-10)

**The anchor is wlroots' own renderer, and the external cross-GPU layer is
libplacebo.** The decision is deliberately two layers rather than one, because
compatibility with scenefx is not negotiable and libplacebo is not the thing that
guarantees it. scenefx renders through `wlr_renderer`; every node, effect and
composited surface the toolkit produces is drawn on the same renderer, in the
same GL context, so nothing the toolkit adds can drift from scenefx's own draws.
libplacebo sits beside that renderer as the source of shader work the wlroots
renderer does not express: arbitrary GLSL passes, render-to-texture effect
chains, and the compute-path primitives behind the effects list in §4.

**libplacebo is chosen because it is the one candidate that is a native C library
and cross-GPU and able to use the context scenefx already has.** It is
LGPL-2.1-or-later, which is compatible with this project's GPL-3.0
(`licence.md`). Its OpenGL backend does not create a context: `pl_opengl_create`
uses the context that is current on the calling thread, which is the EGL context
scenefx renders in. The interop between the two is therefore at the texture and
framebuffer level, and libplacebo names the exact mechanisms:

- `pl_opengl_wrap` imports an external GL texture or FBO into a `pl_tex`, and
  `pl_opengl_unwrap` returns the GL texture and FBO behind a `pl_tex`, so a
  surface can move between the scene graph and a libplacebo effect chain without
  a copy at the API level. Ownership does not transfer, so a scenefx-owned
  texture wrapped for an effect is not destroyed by libplacebo.
- `egl_display` and `egl_context` in `pl_opengl_params` are the fields that
  enable dmabuf import and export, which is the path for buffers the compositor
  receives from clients.
- `make_current` and `release_current` are the callbacks that make the resulting
  `pl_gpu` thread-safe; they carry the same synchronization the rest of the
  compositor's GL use needs, so libplacebo never serializes against another
  thread's context binding by accident.

The alternatives that were surveyed and rejected, and the reason each fails the
same bar: bgfx and Dawn are not native C (a C++ core behind a C header, and a
Chromium build respectively); wgpu-native is a Rust core and Vulkan-first on
Linux; sokol_gfx has no Vulkan backend. Each is cross-GPU in its own right, but
none of them both is a native C library and can be handed the context scenefx
already owns, and the anchor decision above makes that combination the
requirement rather than a preference.

**Where libplacebo is used is per pass, and §4.3 is where that line is drawn.**
§4.2 and §4.3 complete the decomposition of §4's list into the primitive set and
the mapping from presets onto them. The design is set up so that a minimal build
uses the wlroots renderer alone and libplacebo is reached only by effects that
need it; that keeps the "overkill now, minimal use upfront" shape the decision
was made for, without making every draw go through a second GPU abstraction.

Two consequences that are already settled:

- **B1 follows from B3.** The render layer is a core service with its own
  dependency set, and `libomniwm` being internal means carrying scenefx and
  whatever the renderer needs is the library's normal shape.
- **The scaffold takes scenefx from nixpkgs, not from Mango's
  `github:wlrfx/scenefx` flake input.** Mango pins that input because it needs
  extensions nixpkgs does not carry. omniWM takes the nixpkgs `scenefx` because
  it takes the released package and builds the extensions itself on top; the
  comment in `meson.build` says so, because a reader finding nixpkgs scenefx and
  assuming the extension question was answered would be worse than not finding
  the question at all.

`draw.md` and `animate.md` both consume this section and inherit its answer.

### 4.2 The primitive set

The work §4 named as still owed was the decomposition of its list into primitives
and the mapping from presets onto them. This is that decomposition. The set is
nine primitives: six that draw pixels and three that make the draw reusable.

| primitive | what it is | what it carries from §4 |
|---|---|---|
| **Target** | a writable buffer with a lifetime | the layer buffers of item 3, and the offscreen target every effect needs |
| **Source** | a pixel input behind one handle | items 3, 5, 6, 14 |
| **Transform** | where a source's quad lands: 2D affine or 3D view-projection | items 1, 2, 12 |
| **Geometry** | the vertex set a source is drawn with | items 4, 5, 6 |
| **Shader pass** | a GLSL program, source to target, with uniforms | items 7, 8, 9, 10, 13 |
| **Blend** | how a pass output combines with its target | item 11 |
| **Pass chain** | an ordered, nameable list of passes | the composition unit a preset is |
| **Driver** | the per-frame uniform source | items 9 and 10, and every animation |
| **Node hook** | a node type and the passes it emits | how drawing meets the scene graph |

**Target** is a buffer the toolkit may write. Its lifetime is one of four. A
*frame* target is valid for one frame and is returned to a pool after its last
reader, where it may be aliased with other frame targets. An *offscreen* target
lives for the span of one effect. A *named* target is full output size and
addressed by name, which is what an above-background or above-windows buffer
(item 3) is. A *persistent* target survives across frames as a two-slot ring,
which is what a feedback trail needs. A target has a size, a format, and an
owner, and it carries no user-visible meaning.

**Source** is a pixel input behind one handle. It has four provenances: a client
surface, imported as a dmabuf; a generated raster, which is a solid colour, a
gradient ramp, an image, a nine-patch atlas, text, or a vector line; another
target, which is any render-to-texture result; and an external region buffer
(`generaldesign.md` §16). One handle for all four is what keeps a shader and a
trail from branching on where the pixels came from. Item 14 is a property of the
handle rather than a primitive of its own: a source that is CPU-authored is also
an SHM buffer a client may write.

**Transform** is where a source's quad lands in its target. It is a 2D affine
(translate, scale, rotate, opacity) or a 3D view-projection, which is item 1's
rotation and perspective. Opacity is a scalar transform, which is why item 12
needs no primitive of its own. A *camera* is a view-projection shared by a group
rather than held per node, and it is one of the additions below.

**Geometry** is the vertex set a source is drawn with: a quad, a rounded
rectangle with per-corner radius, a ring (one layer of a border), a nine-patch
(nine quads over one atlas), a tiled quad (wrapped UVs), or a general mesh (vector
lines and distortion). Items 4, 5 and 6 are geometry rather than effects: a
multi-layer border is several rings, a textured border is one quad with a UV
mapping, and nine-patch is a geometry over one atlas.

**Shader pass** is a GLSL program that reads sources and writes a target with
uniforms. It carries items 7, 8, 9, 10 and 13, and it is the one primitive that
may reach libplacebo (§4.1) when the wlroots renderer cannot express a pass. Item
7's gradient is the cheap case: a ramp is a generated Source and needs no shader,
so "gradients and similar" is the point where a shader becomes the answer.

**Blend** is how a pass output combines with the target it lands on: over, add
(item 11's glow), multiply, and the decay lerp a feedback trail uses. Blend is a
property of a pass, not a draw of its own.

**Pass chain** is an ordered, nameable list of passes, and it is the composition
unit. A preset is a chain bound to one config key, which is the sentence in §4
made concrete: the core ships the primitives, and a looks-based feature is a chain
over them rather than a code path.

**Driver** is the per-frame uniform source: the clock, the pointer, and any
animatable node property, plus a delayed sample of a property (below). It is what
makes a shader animation (item 9) and a property animation (`generaldesign.md`
§10) the same mechanism, and it is the seam phase 10 writes against.

**Node hook** is `omni_node_type_register(type_id, properties, draw_fn, pass_fn)`
(`helpers.md` §2.2). A node declares the properties it carries and the passes it
emits, so a client surface, a border, and a compositor widget are drawn by the
same machinery. §4.3 is the model.

Every item on §4's list is one of these or a chain over them:

- **3D and 2D transform windows** are the two Transform kinds on a node.
- **above-background and above-windows buffers** are a named Target filled by a
  capture and read as a Source.
- **multi-layer borders** are a ring Geometry per layer, each sampling its own
  Source.
- **textured borders, tiled and stretch-to-fit** are Geometry's UV mapping over
  one Source.
- **nine-patch** is nine-patch Geometry over one atlas Source.
- **shadered borders** are a Shader pass over a border Geometry, with a ramp
  Source as the degenerate case.
- **custom shader effects, custom shader animations, and animated shaders on
  borders** are a Shader pass, the same pass with Driver-fed uniforms, and the
  same over a border Geometry.
- **glow and shadows** are one chain, two presets: capture, blur, offset, tint,
  and a Blend.
- **window opacity** is a scalar Transform.
- **custom GLSL** is the Shader pass.
- **SHM buffers** are the Source handle's CPU-authored case.

**Five primitives named by this design.** Three are general and two are for
trails. Each has a feature that cannot be written without it.

- **Mask.** A per-pixel clip that multiplies a pass by a Source. Geometry gives a
  shape, not per-pixel coverage, and the case that forces the distinction is a
  blur behind a rounded window: the blur has to be clipped to the same rounded
  shape, and a geometric ring does not clip a shader. Shaped windows, spotlight
  and vignette, reveal transitions, and a reflection's fade all reach for it.
- **Subtree capture.** A first-class pass that renders a node and its children
  into a Target. Item 3 and glow and shadows already imply render-to-texture, but
  naming capture as an op is what lets a group be treated as one Source: group
  opacity, overview and switcher cards, crossfades between two captured states,
  and the source a trail or a reflection reads.
- **Camera and projection.** A view-projection shared by a group, beyond the
  per-node 3D Transform. Per-node 3D gives item 1; a shared camera and a z-order
  are what a 3D layout (a cube, a cylinder, a carousel) needs.
- **Persistent target.** A Target lifetime that survives frames. It is a lifetime
  on the primitive above rather than a new kind of thing, and it is what an
  afterimage is built from.
- **Feedback blend.** A Blend mode that lerps a new pass output over the target
  with a decay factor. It is the other half of an afterimage.

**The delayed property sample** is a sixth capability, but it sits on the Driver
rather than on a primitive: `value(property, t - n)`. It is what makes a geometry
trail, the node drawn at its own past transforms, a chain rather than a fork, and
it is why the Driver is a general property source rather than only a clock.

**Trails and afterimages, decided 2026-10-10.** The built-in is feedback. A
persistent target is a two-slot ring; each frame the node or subtree is drawn into
the scratch slot, the scratch is blended over the persistent slot with the decay
factor, and the persistent slot is composited to the output. Feedback covers
afterimages, motion blur, ghosting, and recursive video-style effects with one
lifetime and one blend mode, and it needs no per-node history. The geometry trail
is the flavour the delayed sample enables: the node's own Source drawn K times at
K past transforms with decreasing alpha. Both are chains, so neither is
special-cased.

One cost is stated so it is not a surprise: a live feedback target rewrites its
whole region every frame while it decays, so it defeats partial damage for that
region. That is a per-effect cost the user opts into by naming the chain, not a
property of the toolkit.

**Two chains ship shared, decided 2026-10-10.** **Blur** and **gradient** are
shipped chains rather than effects each feature invents. Glow, shadows, and
backdrop blur all resolve to the one blur chain, and gradient serves shadered
borders and backgrounds. A shipped chain is registered like any other preset,
through `omni_effect_register` (`helpers.md` §2.2), and a component may register
more.

### 4.3 The node and pass model

A frame is a directed acyclic graph of passes. A node's pass function emits the
passes for that node; the frame builder orders the graph, pools the targets, and
runs it. This model is written here rather than left to phase 08 because phase
08's node list and phases 09 and 10 all consume it, and because it is the
vocabulary §4.2 is expressed in.

**The node type.**

```
struct omni_node_type {
    uint32_t                    type_id;
    const struct omni_property *properties;  /* animatable subset marked */
    size_t                      n_properties;
    void (*draw_fn)(const struct omni_node *node,
                    struct omni_pass_builder *out);
    void (*pass_fn)(const struct omni_node *node,
                    struct omni_pass_builder *out);
};
```

`draw_fn` emits the passes that produce the node's own pixels. `pass_fn` emits
the node's configured effect chain around that content: it captures the `draw_fn`
passes into a target, runs the chain the config names, and composites the result.
Keeping the two apart is what lets any node be given any chain without the node
knowing which one, and it is why a client surface, a border, and a widget share
the effect path. A node carries identity, geometry, and its property values; it
does not carry pixels. The pixels are a Source, read from the block or from a
capture.

**The pass.**

```
enum omni_pass_kind {
    OMNI_PASS_DRAW,     /* draw a source with a geometry and a transform */
    OMNI_PASS_SHADER,   /* run a shader over its sources */
    OMNI_PASS_CAPTURE,  /* draw a node subtree into the target */
    OMNI_PASS_BLEND,    /* combine a source with the target under a blend */
    OMNI_PASS_MASK,     /* multiply the target by a source */
};

struct omni_pass {
    enum omni_pass_kind        kind;
    struct omni_target        *target;
    const struct omni_source  *source;    /* DRAW, SHADER, BLEND, MASK */
    const struct omni_node    *node;      /* CAPTURE */
    const struct omni_shader  *shader;    /* SHADER */
    struct omni_geometry       geometry;  /* DRAW */
    struct omni_transform      transform; /* DRAW */
    enum omni_blend_mode       blend;     /* BLEND */
    const struct omni_uniform *uniforms;  /* SHADER, filled from the Driver */
    size_t                     n_uniforms;
};
```

A pass is data. It names its target and its sources and carries no state of its
own, which is what makes a chain a value a preset can hold and a test can compare.

**Target lifetimes and pooling.** A target's lifetime is frame, offscreen, named,
or persistent (§4.2). The builder computes each target's last reader from the
graph and returns a frame or offscreen target to the pool at that point, so a
chain that needs three intermediates at once pays for three and a chain that needs
them in sequence pays for one. Named and persistent targets are not pooled: they
are addressed by name and outlive the frame.

**One op set, two backends.** A pass carries a backend hint. A pass the wlroots
renderer can express runs there; a pass that needs arbitrary GLSL or a compute
blur runs on libplacebo, on the same GL context, with the target and its sources
wrapped at the boundary by `pl_opengl_wrap` and unwrapped by `pl_opengl_unwrap`
(§4.1). A preset is written once and the backend is chosen per pass, which is the
minimal-build shape §4.1 chose: the wlroots renderer alone until a pass needs
more.

**Sources.** One handle covers the four provenances of §4.2. A shader, a trail,
or a blend reads a source without knowing whether it is a dmabuf, a raster, a
prior target, or an external region. The SHM case is the same handle with a
CPU-authored backing store.

**Chains and presets.** A chain is registered with
`omni_effect_register(name, primitive_ids[], params_schema)` (`helpers.md` §2.2).
The name is a config key; the primitive list is the chain; the schema is what a
config value fills, which is the uniforms and the geometry. A preset is therefore
data all the way down: config names a chain, the chain names primitives, and the
frame builder runs them.

**Capture and layers.** Capture is `OMNI_PASS_CAPTURE`, a pass whose input is a
node and whose output is a target. A named layer is a capture whose target is a
named one, read afterward as a source. This is the single mechanism behind item 3
and behind group opacity, cards, and crossfades.

**Feedback.** A persistent target is a two-slot ring. The decay blend is
`OMNI_BLEND_DECAY`, which reads the scratch slot and the persistent slot and
writes the persistent slot. Trails need no code beyond the target lifetime and the
blend mode.

**Damage.** The union of the target regions a frame's passes touch is the frame's
damage. A live persistent target forces its region damaged every frame while the
effect is active, which is the cost §4.2 states.

**Determinism and testing.** A chain expands to passes as a pure function of
config, the node's properties, and the injected clock (`testing.md`), so the
expansion is a unit test with no GPU, and only the draw itself is a wlroots
harness case. This is the same seam `testing.md` §2 draws for the rest of the
matrix.

**What this asks of scenefx.** The requirement list `08-draw.md` item 8 checks
against, and it is short because §4's decision was to build the extensions rather
than patch scenefx: a node type that can carry a custom pass, and a render-pass
entry point that accepts the op set above. Both are ours to add. scenefx's blur,
shadow, and corner-radius stay used underneath, and a pass that composes them is
no different from one that does not.

## 5. The solver's arithmetic width (B4)

**Decided 2026-09-29: fixed-point internally, integer at commit, CPU.** Every
reference window manager, Mango, Hyprland, wayfire, Halley and ShojiWM, runs the
solver on the CPU with floating point internally and integer at the commit
boundary, so CPU was never in doubt. Fixed-point over float is the deliberate
divergence, chosen for cross-architecture bit reproducibility: a deterministic
golden-file solver cannot rest on float. Determinism outranks aarch64 breadth,
which is only a Nix default and is still wanted, just not at the cost of the
solver being reproducible. `testing.md` §5.3 is where the determinism is
asserted, and it is asserted at the commit surface.

The user-facing cost is absorbed in the parser, not in the block.
`tomlparser.md` §3 interprets a TOML float literal into the fixed-point
representation, so TOML stays easy to write, while a SHM client reads the stored
representation directly. That is the trade for much lower-level access, and it
comes with the documentation that goes with it.

**Decided 2026-10-10: Q32.32 in `int64_t`.** The value is a signed 64-bit integer
read as a fixed-point number with 32 fractional bits: a range of about ±2^31 and a
resolution of 2^-32. It is the same on x86_64 and aarch64 because the type and the
shift are the same on both and nothing on the path touches floating point, so the
cross-architecture bit reproducibility the choice was made for is a property of
the type rather than a hope. It is already an integer, so the commit boundary does
not convert it: the commit stores the `int64_t`, and a SHM client reads that same
integer. `tomlparser.md` §3 absorbs the one lossy-looking step by interpreting a
TOML float or duration literal as a scaling by 2^32 and a round to the nearest
representable value, so a literal the representation cannot hold exactly is a
parser diagnostic rather than a silently different solve. `layoutengine.md` §3.2
consumes it. Width, range and precision are closed.

## 6. The Meson structure (B6)

**Mango's structure is in the tree and configures.** `meson.build` names all 62
intended translation units in `core_sources`, with `ipc_sources` and
`debugger_sources` as the two smaller groups and the `executable()` call
commented out. The list is `filestructure.md`'s tree in build-system terms, so it
is a second place that has to change when the tree does; the two drifting apart
produces a build that silently omits a file rather than a build that fails, which
is why `scaffolding.md` records the same list rather than leaving it as a comment
in a build file.

What the build already settled is the package split and its mechanism, in §2.1:
four `install_tag` values keep the five Nix packages separate, and dependencies
travel by propagation rather than by copying a `.so` into two outputs.

**The general principle governing what belongs in the core**, stated
2026-09-29: the core library is a simple, generic feature set that cannot do much
by itself but serves as a foundation to build new features extremely quickly, so
omniWM can realistically support the massive feature set it wants. A unit that
implements a super-specific complex behaviour belongs in the compositor layer or
in a component on top of the core, not inside it. This is a rule about what a
*unit* implements, not about what `libomniwm` links: wlroots and scenefx are
dependencies of the internal library, not features of it, and §3's correction
stands independently.

**Decided 2026-10-10: the core library contains everything the WM needs to
function, and features extend it by registration.** This refines the 2026-09-29
principle above rather than reversing it: the core is the whole of what omniWM
needs to run, not a thin generic layer the compositor privately fills in, so what
B6 was asking about is a split of *registration* rather than of ownership. Core
functionality is exposed through components. `registry.{c,h}` holds the registry
itself, and `component-registry.h` is a thin header that includes every
component's public header, so the fixed table in `core/server.c` names all the
register functions in one place. A component's own code lives entirely in its own
file, and adding one line to that table registers it, so a component cannot be
half-wired and "why is my component not running" has one answer. The 62-unit list
in `meson.build` is the core's own build under this rule. The scope of the
registry functions, and a worked component that demonstrates the whole path with
mock functions, are the remaining design work of this item and of D1, and both
land in a document rather than in the tree.
