# Missing devnotes topics

The design documents that `generaldesign.md` promises but that do not exist yet.
`generaldesign.md` owns the high level for every subsystem below; each row names
the document that will own the detail, what that document must answer, the
`README.md` roadmap stage that delivers it, and its priority.

Priority follows the `helpers.md` §9 bands, rounded to the subsystem that lands
first: P0 is required before any window appears, P1 is the core look and feel,
P2 is the extensibility surface, P3 is the advanced input and language work.

The two empty placeholders `layoutsystem.md` and `looks.md` are not topics of
their own. Their content is split across the tags, windows, and layout-engine
rows, and across the draw, decorate, and animate rows respectively, so a single
subsystem document does not have to cover a whole promise.

| topic | document | what it must answer | stage | priority |
|---|---|---|---|---|
| tags | `tags.md` | **written, partial.** Covers the Tag entity as a catalog entry, its client set, per-tag layout, the monitor's ordered tag list and primary rule, membership storage, and that floating and other window state belong to the client rather than the tag. Not covered, and still owed: what a tag looks like, tag rules, per-output tag defaults, and the surface syntax for setting tags | 7 | P1 |
| windows | `windows.md` | the client lifecycle; the floating layer; focus and stacking policy; groups as nested layouts with titlebars; clusters as movement relationships; window identity that decoration overrides key on; window rules and their precedence; compositor-drawn fake clients as layout participants | 5, 8 | P0 |
| layout engine | `layoutengine.md` | the constraint record on a client; the soft-with-priority model and how the solver degrades; the constraint language a user writes; built-in layouts as constraint presets; how a layout is selected per tag; nested layouts for groups, clusters and scratchpads; the arrange pass | 6 | P0 |
| draw | `draw.md` | the scene graph node types and their animatable properties; backgrounds as colour, image, shader, vector overlay and panning infinite canvas; vector line objects; text and images; how the compositor's own drawn surfaces are produced; how externally rendered regions become nodes | 12, 13, 14 | P1 |
| decorate | `decorate.md` | multi-layer borders including gradients and textures; shadows; blur; glow; opacity; rounding; window overlays and underlays; dimming; the three per-window override routes converging on WINDOW_DEPENDENT keys; global versus per-window precedence | 8, 11 | P1 |
| animate | `animate.md` | animatable properties on nodes; timelines declared in config; curves; stagger; 2D versus 3D; shader-driven animation; how a live gesture keeps animation live; the interaction with the solver's geometry | 9 | P1 |
| input | `input.md` | one catalog key per binding; matching across keyboard, mouse buttons, wheel, touchpad gestures, tablet and stylus; gesture recognition on top of the wlroots signals; invoking the action registry; focus-follows-mouse and click-to-focus; tablet pressure, tilt, absolute pointing and annotation mode | 4, 10 | P0 |
| monitor | `monitor.md` | output hotplug and how it reconfigures layout; per-output configuration matched by name, make, model and serial; virtual monitors; Xwayland integration and its tag, layout and decoration parity | 5 | P1 |
| protocols | `protocols.md` | the Mango protocol set brought in wholesale; which wlroots managers the compositor instantiates; the region-based external rendering path; the later Wayland buffer-injection protocol for GPU-path external renderers | 16, 18 | P2 |
| languages | `languages.md` | the C reference client library over `include/shared/`; the Python reference scripting library; the Mango and Hyprland interpreters; why a first-class library in another language needs no compositor change; whether Lua remains an initial deliverable | 15, 16, 17, 18 | P2 |
| build | `build.md` | the build system; the dependency set and the wlroots and scenefx version coupling; the exact scenefx extensions needed for user GLSL shaders and 3D transforms, and whether they are a maintained patch or a fork | 3 | P0 |
| testing | `testing.md` | the store write and read test; the ABI static-assert header; the store fuzz target and its corruption corpora; a layout and constraint solver test; a gesture and binding match test | 1 | P0 |
| licence | `licence.md` | GPL-3.0 as recorded in `architecture-audit.md` §8, the licence file, and how it affects the Mango-derived and scenefx-derived portions | 3 | P3 |

Rows P0 and P1 are the compositor itself and are what stages 1 to 14 deliver.
Rows P2 and P3 are the extensibility surface and the project-level decisions,
and are what stages 15 to 18 and the early documentation deliver.
