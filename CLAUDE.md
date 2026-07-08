# rivegd — contributor notes (humans and AI agents)

Rive's C++ runtime + Rive Renderer as a Godot 4.7 GDExtension. This file
covers how to build, test, and avoid the traps. (Internal planning notes —
`GOALS.md` and most of `docs/` — are kept local and gitignored; only
`docs/usage.md` is published.)

## Build

```sh
tools/build_rive.sh                # stage 1: rive static libs (Linux desktop)
tools/build_rive_web.sh            # stage 1 for wasm (installs Emscripten 4.0.20)
.venv/bin/scons                    # stage 2: the GDExtension (template_debug)
.venv/bin/scons target=template_release
# web stage 2 (source the SAME emsdk stage 1 used):
source thirdparty/rive-runtime/build/dependencies/emsdk_4.0.20/emsdk_env.sh
.venv/bin/scons platform=web target=template_release threads=no
```

- Stage-1 flags and stage-2 `CPPDEFINES` **must match** (`RIVE_VULKAN`,
  `RIVE_DESKTOP_GL` / `RIVE_WEBGL`, audio, scripting). Mismatches produce
  silent import failures or link errors.
- Stage 1 must run from `renderer/` (premake5.lua location); toolset is
  clang-only; `--no-lto --with-pic` are required for a shared library.
- librive ↔ librive_pls_renderer are circular (scripting GPU hooks): both
  the SConstruct and tools/test.sh group-link (`--start-group/--end-group`).
- SCons may NOT rebuild on flag changes — when in doubt:
  `find src thirdparty/rive-runtime/utils \( -name "*.o" -o -name "*.os" \) -delete`.

## Test

```sh
tools/test.sh all                  # lint (layering) + 66-assertion unit suite
# GPU smokes need the real display (Xvfb+NVIDIA Vulkan hangs):
GODOT=path/to/godot4.7
$GODOT --path tests/project api_smoke.tscn        # 13-phase pixel-asserting
$GODOT --headless --path tests/project ordering_smoke.tscn   # merge bar
# others: render, cards (dynamic lists), scroll, reflow, fallbackfont,
# clicklistener, translucent, multitouch, multi, resize, semantics,
# audiobus, texture3d, overlay, fit, oob smokes; bench.tscn (RIVEGD_BENCH_*)
```

**Gate rule: run the affected smokes bare and check `$?`. NEVER gate a
push through a pipeline** — `cmd | grep -c X` swallows exit codes and
`grep -c` exits 1 on zero matches.

Headless `--import` SIGABRTs with any class-registering GDExtension
(upstream Godot bug, repro in `tests/upstream/`): always
`--import || true` first, then run the scene.

## Architecture (one paragraph)

`src/core/` is Godot-free (rive + stdlib; unit-tested). `src/render/` is
backend bridges (vulkan, gl) behind `RenderBridge` — no Godot types.
`src/godot/` is the only godot-cpp layer: `RiveRenderServer` (singleton;
owns ALL rive GPU state; every mutation is a `runOnce` closure on rive's
CommandQueue, pumped ONCE per drawn frame at `frame_pre_draw` —
`rt_*` methods run on the render thread only), `RiveInstance` (main-thread
controller: posts work, polls mutex-guarded mailboxes, replays
inputs/properties/watches/semantics on recreate), and the nodes
(`RiveSprite2D`/`RiveControl`/`RiveTexture`). Instance lifetime is
CommandQueue handles (create mints, release orders deletes after rt_free —
FIFO makes it safe). One refcounted file import per `RiveFileResource`.

## Hard-won gotchas (violate these and you will lose hours)

- **The pump**: `request_pump()` defers to `frame_pre_draw` when rendering;
  headless connects the signal but it NEVER fires — the headless path pumps
  immediately (keyed on DisplayServer name == "headless", NOT driver name).
  Teardown uses `request_pump_now()` (a quitting app draws no more frames).
- **CommandServer's Factory is fixed at construction** — ensure the bridge
  BEFORE constructing it, or everything imports through NoOpFactory and
  renders nothing, silently.
- rive builds **RTTI-off**: TUs deriving rive types compile `-fno-rtti`;
  never `dynamic_cast` a rive type — dispatch on `dataType()` etc.
- The queue's `""` state-machine name means the *designated default* (often
  absent); resolve the first machine's name from metadata main-side.
  Artboards with zero machines must skip instantiation (queue errors).
- VM instances must come from `ViewModelRuntime::createDefaultInstance()`
  (the factory wires property runtimes) — never hand-wrap
  `createDefaultViewModelInstance()`.
- `sm->enableSemantics()` or `semanticManager()` is null.
- Rive scroll physics needs REAL move timestamps; sample-signed fixture
  scripts never run in production-key builds (only editor-signed do).
- Web (all baked into build scripts, see docs/usage.md#web-exports):
  Emscripten **4.0.20** (what 4.7 templates actually use),
  `-sSUPPORT_LONGJMP=wasm` both stages, `--no-wasm-simd` (v128 can't cross
  dylink JS trampolines). Debug browser stacks: rebuild with
  `debug_symbols=yes`; verify imports with node
  `WebAssembly.Module.imports()` vs main-module exports (GL comes from the
  JS library, not wasm exports).
- GDScript can't call engine virtuals on GDExtension classes
  (`_gui_input()` errors) — use `grab_focus()` + `Viewport.push_input()`.
  GDScript also can't reference INTERNAL-registered classes as types.
- Editor fixtures: author via the Rive editor MCP; only COMPONENT artboards
  reliably export; the MCP can't export, place scripts, or author
  listeners/events — those are manual editor steps. Fixture provenance
  lives in `tests/project/fixtures/README.md`.
- Test honesty: identical tiling visuals defeat pixel-compares (track
  structural features like gap lines); tofu glyphs are DENSER than real
  ones; ambient animation fakes "pixels changed" — prefer signals, VM
  reads, or structural pixel features over raw frame diffs.

## Docs to keep in sync when shipping features

`README.md` (feature list), `docs/usage.md` (user-facing how-to), and
`doc_classes/*.xml` (in-editor F1 docs for any new API). Local-only:
`GOALS.md` and the design notes under `docs/` (gitignored).
