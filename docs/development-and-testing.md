# rivegd — Development & Testing Design

Companion to [`GOALS.md`](../GOALS.md).

> **Status (July 2026) — design vs reality.** This document is the original
> test-architecture design; the shipped reality covers its intent with a
> leaner shape:
> - **Tier 0** ✅ as designed: Catch2 + NoOpFactory, 66 assertions
>   (`tools/test.sh unit`), layering lint.
> - **Tiers 1–2** (draw-stream, native goldens): not built; their job is
>   covered by the pixel-asserting Godot smokes below.
> - **Tier 3** ✅ as smoke *scenes* rather than gdUnit4: 17 scenes under
>   `tests/project/` — `api_smoke` (13 phases: inputs, pointer, data
>   binding, hot reload, texture, enum/trigger, lists incl. item reads,
>   keyboard/focus, audio, gamepad, behavioral Luau, targeted callables,
>   fit pixels), `ordering_smoke` (O1–O4 event-ordering guarantees — the
>   merge bar), `render_smoke` (content-asserting), plus feature smokes:
>   `cards` (dynamic lists), `scroll`, `reflow` (Yoga), `fallbackfont`
>   (CJK/RTL), `clicklistener`, `translucent` (hit testing), `multitouch`,
>   `multi` (instance independence), `resize` (state preservation),
>   `semantics` (accessibility), `audiobus`, `texture3d` (raycast → UV →
>   listener), `overlay`, `fit`, `oob` (referenced assets), `web_smoke`
>   (runs in-browser). GPU smokes run on a real display (Xvfb+NVIDIA
>   Vulkan hangs); logic smokes run headless.
> - **Tier 4** partial: thumbnails exercised; headless `--import` crashes
>   with any class-registering extension (upstream bug, `tests/upstream/`,
>   CI works around with `|| true`).
> - **Tier 5** ✅: `bench.tscn` (`RIVEGD_BENCH_COUNT/FIXTURE/ARTBOARD/
>   SIZE`, `RIVEGD_BATCH_SIZE`); reference numbers in GOALS/README.
> - **Tier 6 (web)** partial ✅: the export pipeline works and `web_smoke`
>   verifies load + render + data binding in Chrome (driven manually via
>   Playwright); the CI browser harness and lockstep gate are still to
>   build. Emscripten pin: **4.0.20** (what Godot 4.7 templates actually
>   use — not the version in Godot's CI workflow file).
> - **CI**: GitHub Actions (build + unit + ordering + C# mono job) and a
>   GitLab mirror — leaner than the matrix below, which remains the
>   target state. The central problem: a GDExtension that drives a GPU renderer is, naively, only testable by clicking around in the editor. The architecture below is arranged so that **most code never needs Godot to be tested, and nothing needs a physical GPU** — CI runs everything on software rasterizers.

---

## 1. Code structure: three layers, dependency-clean

```
src/
  core/      # Godot-FREE C++. Depends only on rive-runtime + stdlib.
             #   - RiveFile/Artboard/StateMachine/ViewModel handle tables & lifetimes
             #   - event queue draining, sleep/wake logic, time scaling
             #   - fit/alignment transforms, pointer-space math
             #   - .riv metadata enumeration (artboards, inputs, view models)
  render/    # Backend bridges behind our own narrow interface (RiveGpuBridge):
             #   vulkan/ gl/ metal/ d3d12/ — each creates a RenderContext +
             #   wraps a target texture. No Godot types here either; the
             #   bridge receives raw handles (VkDevice, GLuint, ...).
  godot/     # The ONLY layer that includes godot-cpp:
             #   nodes (RiveSprite2D, RiveControl), RiveTexture, resource
             #   loader/importer, editor plugin, inspector plugin,
             #   signal marshalling, get_driver_resource plumbing.
```

Rule enforced by include-guards in CI: `core/` and `render/` must not include `godot_cpp/`. The `godot/` layer stays thin — it translates types and forwards; logic lives below, where it's unit-testable.

## 2. Repository layout

```
rivegd/
  GOALS.md  docs/  LICENSE  README.md
  addons/rive/              # the shippable addon (checked in, bins gitignored)
    rive.gdextension        #   reloadable = true, compatibility_minimum = 4.7
    bin/<platform>/         #   built artifacts land here
    editor/                 #   GDScript editor UX (preview panel, drag-drop)
  src/                      # (as above)
  thirdparty/
    rive-runtime/           # git submodule, pinned commit
    godot-cpp/              # git submodule, pinned to 4.7 branch
  tools/
    build_rive.sh           # stage-1 wrapper (flags: text, layout, audio=external, scripting)
    test.sh                 # entry point: unit | draw | golden | godot | bench | all
    check_layering.sh       # include-guard lint
  tests/
    fixtures/*.riv          # tiny deterministic fixtures + generation notes
    unit/                   # Tier 0: Catch2, links core/ + rive static libs
    draw/                   # Tier 1: SerializingFactory draw-stream regression
    golden/                 # Tier 2: native GPU harness + per-backend goldens/
    project/                # Tier 3: Godot test project (gdUnit4), addon symlinked
    bench/                  # Tier 5: benchmark scenes + budget JSON
  demo/                     # demo project (also the manual test bed), addon symlinked
  SConstruct                # stage-2: builds src/ + links stage-1 libs
  .github/workflows/
```

`addons/rive` is symlinked into `tests/project` and `demo` so one build serves all consumers. `dev_build=yes` SCons profile + GDExtension **hot reload** (`reloadable = true`, supported since Godot 4.2) gives the core loop: edit C++ → `scons` → the open editor reloads the extension in place. `compile_commands.json` is emitted for clangd.

## 3. The test pyramid

### Tier 0 — C++ unit tests (no Godot, no GPU)
Catch2 suite over `src/core/`, using rive-runtime's **`NoOpFactory`** (`utils/no_op_factory.cpp` — the same tool Rive's own unit tests use) to load real `.riv` fixtures and advance state machines with zero GPU. Covers: handle table lifetimes (double-free, free-during-async-load), event drain ordering, sleep/wake transitions, transform math, metadata enumeration.
**Sanitizers:** ASAN+UBSAN always in CI; a TSAN job stress-tests the CommandQueue client/server model (rapid create/advance/destroy from a fake game thread vs. server thread).

### Tier 1 — draw-stream regression (deterministic, no GPU)
rive-runtime's **`SerializingFactory`** records the draw command stream. We advance fixtures by fixed deltas and snapshot the serialized stream. This catches "the animation/state machine now draws something different" *bit-exactly*, with none of golden-image flakiness — it's the workhorse regression tier for runtime upgrades (pinned-submodule bumps get reviewed as draw-stream diffs).

### Tier 2 — native GPU goldens (no Godot)
A small harness (modeled on rive-runtime's `tests/gm` + `imagediff`) that creates a headless context per backend — **our `render/` bridge is exercised directly**, exactly as Godot would drive it — renders fixtures to an offscreen target, and compares PNGs with perceptual tolerance. CI backends: Vulkan on **lavapipe** and GL on **llvmpipe** (Linux runners), Metal on macOS runners. Goldens are stored per-backend; the diff tool uploads failing triptychs (expected/actual/diff) as CI artifacts.

### Tier 3 — Godot integration tests (headless engine, software GPU)
A `tests/project` Godot project using **gdUnit4**, run via `godot --headless` in CI:

- **Logic suites** (`--rendering-driver dummy` where possible): resource loading, property reflection (`_get_property_list` matches fixture metadata), signal emission (`rive_event` payload dictionaries, `state_changed`, awaiters), Callable subscriptions and unbind, pause/`speed_scale`/`process_mode` semantics, node free during async load, hot-reload resource swap.
- **Input suites**: synthesized `InputEventMouse*`/`InputEventKey` into `RiveControl`, asserting listener-driven state changes and focus-chain traversal.
- **Render suites**: run with `--rendering-driver vulkan` (lavapipe) and `opengl3` (llvmpipe); capture `viewport.get_texture().get_image()` after fixed advances; compare against Tier-2-style goldens with tolerance. This is the end-to-end proof the Texture2DRD bridge and frame synchronization actually work — the one thing lower tiers can't see.
- **Determinism discipline**: fixed `advance(delta)` via manual process mode, vsync off, no wall-clock time in fixtures.

### Tier 4 — editor/import tests
`godot --headless --import` over a fixture project validates the importer end-to-end (resources appear, metadata cached, thumbnails generated without crash); a scripted editor smoke-run (`--editor --quit-after`) guards against editor-plugin regressions; class-reference XML is built and link-checked.

### Tier 5 — benchmarks (tracked, gated)
`tests/bench` scenes (the 50-artboard HUD from GOALS success criterion #4, a data-binding churn scene, a text-heavy scene) run N frames headless with lavapipe, emit frame-time JSON, and compare against checked-in budgets with a regression threshold. Real-GPU numbers come from a manual workflow on developer machines; CI catches *relative* regressions.

### Tier 6 — web build tests (browser harness)

Web is the one platform where "it built" proves nearly nothing — the failure modes live in dlink loading, Emscripten ABI drift, and browser GL paths. Dedicated harness (`tests/web/`), driven by **Playwright** against a Godot web export of the test project served locally:

- **Lockstep gate (build-time):** CI job diffs our Emscripten version/flags against the pinned Godot export templates' build metadata; any drift fails before a browser ever launches. This is the cheapest, highest-value web test.
- **Load smoke:** export `tests/project` with dlink templates, load in headless Chromium, assert the extension initializes, a `.riv` loads, a state machine advances (results reported from GDScript via `JavaScriptBridge` → page console → Playwright).
- **Render goldens in-browser:** the Tier 3 screenshot suites re-run in the browser, canvas captured via Playwright, compared with per-path goldens. Run **twice**: once default (PLS path on Chromium/ANGLE) and once with `--disable-webgl-extensions`-style forcing of the **MSAA fallback** — both of Rive's WebGL2 interlock modes stay covered without needing Safari/Firefox runners (WebKit/Firefox via Playwright are a stretch goal for real cross-browser coverage).
- **Single-thread semantics:** the Tier 3 logic suite (events, callables, pause/speed) re-runs in the browser to prove the CommandServer's same-thread degradation is behaviorally identical.
- **Size budget:** the exported wasm size (full and slim variants) is checked against budgets in `tests/bench/budgets.json`; growth beyond threshold fails CI — this keeps the feature-flag slim build honest over time.

Web tiers run on every PR that touches `render/gl`, build config, or submodule pins, and nightly otherwise (browser jobs are the slowest in the matrix).

## 4. Fixtures & goldens policy

- Fixtures are **tiny, purpose-built `.riv` files**, one concern each (one state machine, one listener, one view model...), checked in with a `fixtures/README` recording the Rive editor version and regeneration steps. Borrowing from rive-runtime's MIT-licensed test assets is fine where they fit.
- Golden updates are **explicit**: `tools/test.sh golden --update` regenerates locally; CI never auto-updates; the PR diff shows the triptychs.
- Every rive-runtime submodule bump runs Tiers 0–3 and reviews draw-stream diffs before merge — this is the sustainability mechanism behind GOALS G6.3.

## 5. CI matrix (GitHub Actions)

| Job | What |
|---|---|
| build | Linux/Windows/macOS × {debug, release} → addon artifacts |
| unit | Tier 0 + ASAN/UBSAN; weekly TSAN stress |
| draw + golden | Tiers 1–2 on Linux (lavapipe/llvmpipe), Tier 2 Metal on macOS |
| godot | Tier 3–4 against pinned Godot 4.7.x headless binary |
| compat | Tier 3 logic suite against each supported Godot minor (per GOALS G6.3) |
| bench | Tier 5 with budget gate |
| web | Tier 6: emscripten lockstep gate on every PR; browser harness (load smoke, PLS+MSAA goldens, single-thread logic, wasm size budget) on render/build-touching PRs + nightly |
| lint | clang-format, clang-tidy on `src/`, gdlint on addon scripts, `check_layering.sh` |

Cross-compile jobs for Android/iOS are build-only until Phase 3, then device farms are a stretch goal (rive-runtime's own browserstack scripts are precedent). Wasm graduates from build-only to the full Tier 6 browser harness in Phase 4 — the lockstep gate runs from the moment the wasm build job exists.

## 6. Developer workflow, day to day

```sh
git clone --recurse-submodules ... && cd rivegd
./tools/build_rive.sh linux          # stage 1 (cached; rerun on submodule bump)
scons dev_build=yes                  # stage 2 → addons/rive/bin/
godot --path demo -e                 # develop; scons again → hot reload in editor
./tools/test.sh unit                 # inner loop (seconds)
./tools/test.sh all                  # pre-push (minutes, mirrors CI)
```

Debugging: launch Godot under lldb/gdb with the dev build (assertions + symbols); `RIVE_GD_TRACE=1` env enables a frame-annotated trace log (context creation, flush timing, barrier transitions) — the first tool to reach for on synchronization bugs; RenderDoc captures work out of the box since we submit on Godot's queue.

## 7. What each phase must prove (test-first gates)

- **Phase 0 gate:** Tier 0 green on all desktop platforms in CI — proves the two-stage build and ABI story *before* GPU work.
- **Phase 1 gate:** Tier 2 Vulkan goldens + a Tier 3 render suite screenshot — proves context bootstrap and frame sync (the #1 risk) under CI, not just on a dev box.
- **Phase 2 gate:** full Tier 3–4 suites — the API surface is now contract-tested.
- **Phase 3 gate:** per-backend Tier 2 goldens (Metal/D3D12/GL) green.
- **Phase 4 gate:** full Tier 6 web suite green — lockstep check, dlink load smoke, in-browser goldens on *both* WebGL2 paths (PLS and forced-MSAA), single-thread logic parity, and wasm size within budget. Web is not "done" until the browser harness says so, not the compiler.
- **Phase 5 gate:** Tier 5 budgets locked; compat matrix green; success-criteria audit against GOALS.
