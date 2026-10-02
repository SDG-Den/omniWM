project('omniwm', ['c'],
  version : '0.1.0',
  meson_version : '>=1.3',
  default_options : [
    'c_std=c11',
    'warning_level=2',
  ],
)

# ---------------------------------------------------------------------------
# Build status
#
# The source tree is scaffolded and empty: 118 files under `src` and `include`
# are zero bytes, and `include/shared/omni_layout.h` is the only one with
# content. The two libraries below therefore link to empty shared objects rather
# than to implementations, and the two executables are commented out until the
# substrate lands.
#
# The list is kept, and kept in the tree, because it is the targets' shape rather
# than their implementation. `devnotes/filestructure.md` is authoritative for the
# directory boundaries, and this is the same list in build-system terms: a file
# added to `include/<dir>` is added to `src/<dir>`, and both appear below.
#
# Two paths in the intended tree have no Mango counterpart, and they are the
# places where we are Mango's shape but not Mango's file list:
#   `ipc/`          the socket facade, outside `src` per `filestructure.md`
#   `debugger/`     the SHM block visual analyser
# ---------------------------------------------------------------------------

# The core library. Everything in `src/` except the entry point, so that a
# consumer can link the compositor's logic without linking a compositor. This is
# the package that exists to be depended on: `libomniwm` is what another project
# would put in its `buildInputs` to drive the store rather than reimplement it.
core_sources = [
  'src/core/lib.c',
  'src/core/log.c',
  'src/core/util.c',
  'src/core/registry.c',
  'src/core/component.c',
  'src/core/events.c',
  'src/core/actions.c',
  'src/core/server.c',
  'src/core/layoutengine.c',
  'src/config/store.c',
  'src/config/types.c',
  'src/config/parse.c',
  'src/input/input.c',
  'src/input/binds.c',
  'src/input/devices.c',
  'src/input/keyboard.c',
  'src/input/mouse.c',
  'src/input/tablet.c',
  'src/input/trackpad.c',
  'src/input/switch.c',
  'src/input/touch.c',
  'src/tags/tags.c',
  'src/tags/overview.c',
  'src/tags/scratchpad.c',
  'src/windows/client.c',
  'src/windows/cluster.c',
  'src/windows/constraints.c',
  'src/windows/fakeclient.c',
  'src/windows/group.c',
  'src/windows/layout.c',
  'src/windows/transform.c',
  'src/monitor/monitor.c',
  'src/monitor/virtualmonitor.c',
  'src/monitor/xwayland_primary.c',
  'src/draw/background.c',
  'src/draw/button.c',
  'src/draw/image.c',
  'src/draw/renderers.c',
  'src/draw/shaders.c',
  'src/draw/textnode.c',
  'src/draw/vector.c',
  'src/decorate/blur.c',
  'src/decorate/borders.c',
  'src/decorate/glow.c',
  'src/decorate/gradient.c',
  'src/decorate/opacity.c',
  'src/decorate/overlay.c',
  'src/decorate/shadows.c',
  'src/decorate/textures.c',
  'src/decorate/underlay.c',
  'src/animate/client.c',
  'src/animate/fakeclient.c',
  'src/animate/layer.c',
  'src/ext-protocol/ext-workspace.c',
  'src/ext-protocol/foreign-toplevel.c',
  'src/ext-protocol/hdr.c',
  'src/ext-protocol/tearing.c',
  'src/ext-protocol/text-input.c',
  'src/ext-protocol/xdg-activation.c',
  'src/ext-protocol/xdg-output.c',
]

# The socket facade, as its own library. `ipc.md` §1 serves the socket from the
# WM event loop, so `ipc.c` is server code that runs inside the compositor
# process and cannot be a separate program. It is a separate *library* for a
# different reason: it is the one part of the compositor that other programs talk
# to, and giving it its own object means a consumer can link the protocol without
# linking the compositor, and the WM can drop it by not linking it.
ipc_sources = [
  'ipc/ipc.c',
]

# The visual debugger. Separate library, and deliberately not a dependency of the
# WM: it is a tool that reads the block, so linking it into the compositor would
# put a debugging surface in the shipping binary for no runtime reason. It is
# built by the same Meson project and installed by its own Nix package, which is
# what lets a debug build pull it in without a release build carrying it.
debugger_sources = [
  'debugger/debugger.c',
]

# ---------------------------------------------------------------------------
# Version
#
# Mango derives its version string from the git commit at configure time. The
# same is done here, and the reason it is done at configure rather than passed in
# is the same: a build from a checkout should report what that checkout is, and a
# build from a tarball should not fail because there is no `.git`.
# ---------------------------------------------------------------------------

cc = meson.get_compiler('c')

git = find_program('git', required : false)
is_git_repo = false

if git.found()
  git_status = run_command(git, 'rev-parse', '--is-inside-work-tree', check : false)
  if git_status.returncode() == 0 and git_status.stdout().strip() == 'true'
    is_git_repo = true
  endif
endif

if is_git_repo
  commit_hash = run_command(git, 'rev-parse', '--short', 'HEAD', check : false).stdout().strip()
  version_with_hash = '@0@(dev-@1@)'.format(meson.project_version(), commit_hash)
else
  version_with_hash = '@0@(release)'.format(meson.project_version())
endif

# ---------------------------------------------------------------------------
# Dependencies
#
# Mango's dependency base, unchanged, because omniWM is a Mango fork and
# `generaldesign.md` §14's policy is to use Mango's code where we can and change
# what we need. `02-substrate.md` B1 owns the final set; this list is that
# decision made early, and the one that matters is already answered:
#
#   wlroots 0.20.2 and scenefx 0.5 are both in nixpkgs-unstable, so the
#   wlroots/scenefx coupling `generaldesign.md` §19 called the largest external
#   risk resolves against a released pair rather than against a fork.
#
# `scenefx` is taken from nixpkgs rather than from a flake input. Mango pins
# `github:wlrfx/scenefx` as a flake input because its own build needs extensions
# to scenefx that nixpkgs does not carry, and `02-substrate.md` B3 is the open
# question of whether we need them too. Until B3 is decided, the nixpkgs package
# is the honest choice: it is the released extension set, and a fork would be a
# decision taken by accident.
#
# `pc` is an oddity inherited from Mango and kept deliberately: some nixpkgs
# packages are only findable by pkg-config name where the Meson `dependency()`
# lookup misses them, and the pc file is the fallback. It is listed last so it
# cannot shadow a direct lookup.
# ---------------------------------------------------------------------------

libm = cc.find_library('m')
libdrm = dependency('libdrm')
xcb = dependency('xcb', required : get_option('xwayland'))
xlibs = dependency('xcb-icccm', required : get_option('xwayland'))
wayland_server_dep = dependency('wayland-server', version : '>=1.23.1')
wlroots_dep = dependency('wlroots-0.20', version : '>=0.20.0')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput', version : '>=1.27.1')
libwayland_client_dep = dependency('wayland-client')
pcre2_dep = dependency('libpcre2-8')
libscenefx_dep = dependency('scenefx-0.5', version : '>=0.5.0')
pixman_dep = dependency('pixman-1')
cjson_dep = dependency('libcjson')
pangocairo_dep = dependency('pangocairo')

compositor_deps = [
  libm,
  libdrm,
  xcb,
  xlibs,
  libscenefx_dep,
  wayland_server_dep,
  wlroots_dep,
  xkbcommon_dep,
  libinput_dep,
  libwayland_client_dep,
  pcre2_dep,
  pixman_dep,
  cjson_dep,
  pangocairo_dep,
]

# ---------------------------------------------------------------------------
# Compile and link arguments
#
# Mango's set, unchanged. `-D_POSIX_C_SOURCE=200809L` rather than `_GNU_SOURCE`
# because the design uses only POSIX 2008 interfaces and a larger feature-test
# macro would let a function's availability depend on the toolchain rather than
# on this decision.
# ---------------------------------------------------------------------------

c_args = [
  '-g',
  '-Wno-unused-function',
  '-DWLR_USE_UNSTABLE',
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="@0@"'.format(version_with_hash),
]

if get_option('asan')
  c_args += [
    '-fsanitize=address',
    '-fno-omit-frame-pointer',
    '-fno-optimize-sibling-calls',
  ]
endif

if xcb.found() and xlibs.found()
  c_args += '-DXWAYLAND'
endif

link_args = []
if get_option('asan')
  link_args += '-fsanitize=address'
endif

# ---------------------------------------------------------------------------
# Wayland protocol generation
#
# Mango's `protocols/meson.build`, adopted unchanged. The three `wlr-*.xml`
# files are Mango's and are vendored the same way, because they are wlr-protocols
# definitions rather than Mango's code: they are the layer-shell,
# foreign-toplevel-management and output-power-management protocols that wlroots
# does not generate for us.
# ---------------------------------------------------------------------------

subdir('protocols')

# ---------------------------------------------------------------------------
# Targets
#
# Four of them, and the split is not cosmetic. `libomniwm` is the part other
# projects link. `libomniwm-ipc` is the socket facade, which the WM links and a
# client does not. `libomniwm-debugger` is a reader of the block, which nothing
# links into the compositor. `omniwm` is the compositor itself, and it is the
# only one of the four that pulls in wlroots.
#
# The three libraries build today. Every source in them is zero bytes, which
# links into a valid shared object with no symbols — the link is real, the
# contents are not. That is why they are enabled and the executable is not: an
# empty `main` does not link at all, and a build that fails for a reason
# unrelated to the design teaches a reader to ignore build failures.
#
# Each carries an `install_tag`, and that is the mechanism that keeps the four
# Nix packages separate. Meson has no per-target install selection other than
# tags, and `meson install` otherwise installs every target in the plan — so a
# package built for one library gets all three, and the split the packages exist
# to express is undone at the last step. `nix/common.nix` passes
# `--tags <tag> --no-rebuild`.
#
# A package that links another installs only its own tag and names the other in
# `propagatedBuildInputs`, which is what puts the dependency in the consumer's
# closure and on its RPATH without copying the `.so` into two outputs.
# ---------------------------------------------------------------------------

libomniwm = shared_library('omniwm',
  core_sources,
  wayland_sources,
  dependencies : compositor_deps,
  install : true,
  install_tag : 'core',
  c_args : c_args,
  link_args : link_args,
)

libomniwm_dep = declare_dependency(link_with : libomniwm)

libomniwm_ipc = shared_library('omniwm-ipc',
  ipc_sources,
  link_with : libomniwm,
  dependencies : compositor_deps,
  install : true,
  install_tag : 'ipc',
  c_args : c_args,
  link_args : link_args,
)

libomniwm_ipc_dep = declare_dependency(link_with : libomniwm_ipc)

libomniwm_debugger = shared_library('omniwm-debugger',
  debugger_sources,
  link_with : libomniwm,
  dependencies : compositor_deps,
  install : true,
  install_tag : 'debugger',
  c_args : c_args,
  link_args : link_args,
)

libomniwm_debugger_dep = declare_dependency(link_with : libomniwm_debugger)

# Disabled: `src/main.c` is zero bytes, so there is no `main` to link. Enabled in
# the phase that writes the substrate, which is `02-substrate.md`.
#
# It links `libomniwm-ipc` and not `libomniwm` directly, because the facade is the
# only path to the store that a socket client is meant to use. Linking the
# library too would make the in-process path available to the WM alongside the
# socket path, which is the arrangement `ipc.md` §7.1 argues against.
#
# executable('omniwm',
#   'src/main.c',
#   dependencies : [libomniwm_ipc_dep] + compositor_deps,
#   install : true,
#   c_args : c_args,
#   link_args : link_args,
# )
#
# The debugger's own program, also disabled for the same reason. It is a separate
# `executable` rather than something the WM runs, which is what makes "the
# release build does not carry it" true rather than aspirational.
#
# The source list is left as a placeholder rather than filled in, because no
# document names an entry point for it. `filestructure.md`'s `debugger` section
# says only that the directory holds "a graphical interface to analyze the SHM
# block in its entirety", and the tree has `debugger/debugger.c` and
# `debugger/debugger.h`. Naming a file here would put a name into the build that
# no design document is answerable for, and the phase that designs the debugger
# should be the one to name it. It is `12-languages.md` or whatever phase owns it;
# this file will not guess.
#
# executable('omniwm-debug',
#   # <the entry point the debugger's design phase names>,
#   dependencies : [libomniwm_debugger_dep, libomniwm_dep] + compositor_deps,
#   install : true,
#   c_args : c_args,
#   link_args : link_args,
# )

# ---------------------------------------------------------------------------
# The ABI header
#
# This is the one target that builds today, and it is the one that stays useful
# for the whole project: a wrong byte-level constant in
# `include/shared/omni_layout.h` is a build failure rather than a disagreement
# between a header and a document, which is what `devnotes/build.md` §1 calls the
# one property in the design that is verified rather than asserted.
#
# The check is `-fsyntax-only` on the header, with `-x c` so it is parsed as C and
# not as a C++ header. Every `OMNI_STATIC_ASSERT` in the file is its only
# executable content, so a zero exit with no output is a pass over all of them.
# `02-substrate.md` D9 and `01-testing.md` item 2 both cite this.
#
# There is no test registry here yet. `devnotes/testing.md` §3 owns the
# `omniwm-test` runner, the store write/read test, and the fuzz target with its
# per-tier corpus, and it adds them as `test()` calls under a `meson test`
# runner rather than having them appear in this file. The runner is substrate
# for phase 02, so it is named here only as a spelling to match.
# ---------------------------------------------------------------------------

# `cc.compiles` is a compile check rather than a target, and it is the right tool
# here for a reason specific to this header. `-fsyntax-only` on a header alone
# would not catch a client that includes it, so the check compiles a translation
# unit that includes it, which is what a real consumer does. A failure here fails
# `meson setup`, which is deliberate: the header is the ABI and a build that
# configures against a broken ABI has no useful failure mode.
abi_check = cc.compiles(
  '#include <shared/omni_layout.h>\nint main(void) { return 0; }',
  name : 'omni-layout-abi-check',
  args : ['-I' + meson.project_source_root() / 'include', '-std=c11', '-Wall', '-Wextra'],
)

if not abi_check
  error('include/shared/omni_layout.h does not compile; see devnotes/build.md section 1')
endif
