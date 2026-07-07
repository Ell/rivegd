# rivegd — Project Goals

**Mission:** be the defacto Rive integration for Godot — the one you'd reach for by default, with the fidelity of Rive's official engine runtimes and the ergonomics of a built-in Godot node.

Design rationale and evidence live in [`docs/implementation-strategy.md`](docs/implementation-strategy.md) (architecture, verified findings, risks) and [`docs/ux-design.md`](docs/ux-design.md) (developer experience). This file is the durable statement of *what we're building and how we'll know it's done*.

---

## G1. Rendering: full-fidelity, GPU-native

- **G1.1** Embed the Rive Renderer itself (`rive::gpu::RenderContext`) — never CPU rasterization, never draw-command translation. Pixel output matches the Rive editor, including renderer-exclusive features (Vector Feathering).
- **G1.2** Run on Godot's own graphics device with zero-copy composition: render to an offscreen target, surface via `Texture2DRD` (RD renderers) / native GL texture (Compatibility). No readbacks in the frame loop.
- **G1.3** Backend coverage tracking Godot's driver matrix: Vulkan (Windows/Linux/Android), Metal (macOS/iOS), D3D12 (Windows opt-in), GL/GLES3 (Compatibility), WebGL2 (web export).
- **G1.4** Correct, explicit frame synchronization: Rive flushes on Godot's render thread before the scene consumes the target (there is no engine-provided ordering guarantee — we own this).
- **G1.5** Scale: one shared `RenderContext` per device batching all artboards; dozens of live artboards in a UI scene without frame-time cliffs.

## G2. Platforms

- **G2.1** Tier 1: Windows, Linux, macOS (editor + export).
- **G2.2** Tier 2: Android, iOS (static-link export path).
- **G2.3** Tier 3: Web via dlink wasm export templates, single-threaded-safe, Emscripten version in lockstep with Godot's templates.
- **G2.4** Graceful degradation everywhere: headless/server builds load files and advance state machines without a GPU (logic-only), for tests and dedicated servers.

## G3. Asset pipeline & editor experience

- **G3.1** `.riv` files import automatically as raw-byte resources; decode is deferred to runtime against the live render context (the RenderContext *is* the rive::Factory — import-time GPU decode is wrong by construction).
- **G3.2** FileSystem-dock thumbnails and an inspector preview (artboard/state-machine pickers, live scrub/playback) rendered by the real renderer — WYSIWYG.
- **G3.3** Hot reload: re-export from the Rive editor updates a running editor preview (and a `--debug` game) in place, preserving matching input values.
- **G3.4** Drag-and-drop instantiation: `.riv` → viewport creates a configured node. ✅ (drop overlay on the editor main screen — inert except during .riv drags; RiveControl for Control scenes, RiveSprite2D otherwise, undo-aware, drop-point positioned)
- **G3.5** Node configuration warnings for stale references (missing artboard, deleted input, scripts present but scripting disabled).
- **G3.6** Out-of-band assets (images, fonts, audio) resolve automatically: referenced assets are found by rive's export convention (`<name>-<id>.<ext>` next to the `.riv`), decoded through the CommandQueue, and registered in its global asset registry before import. ✅ (verified: walle.riv's referenced PNGs render; `get_asset_descriptions()` exposes the manifest)

## G4. Runtime API: a native Godot citizen

- **G4.1** Node types: `RiveSprite2D` (Node2D), `RiveControl` (Control), plus `RiveTexture` for any texture slot (3D materials, TextureRect, shaders, particles) — interactive in 3D via UV-space pointer injection (`send_pointer_uv`).
- **G4.2** State machine inputs and view-model (data binding) properties are **real Godot properties** — inspector-editable (checkboxes/sliders/trigger buttons/color pickers/enum dropdowns), animatable by AnimationPlayer, tweenable.
- **G4.3** Script callbacks: signals (`rive_event`, `state_changed`, `property_changed`) ✅ and targeted Callables (`on_event(name, callable)`, `on_property(path, callable)`, returning the Callable for unsubscribe) ✅. Awaiters are trivially `await node.rive_event` in GDScript. `animation_finished`/`input_changed`/`rive_audio_event` signals not yet distinct.
- **G4.4** Data binding is first-class: typed, path-addressed view-model properties, two-way, with change signals. This is the primary Rive↔game contract, and coverage means **every** view-model type Rive can author, each mapped to an idiomatic Godot type:

  | Rive VM type | Godot mapping | Status |
  |---|---|---|
  | number | `float` (set/get/watch/inspector) | ✅ |
  | boolean | `bool` (set/get/watch/inspector) | ✅ |
  | string | `String` (set/get/watch/inspector) | ✅ |
  | color | `Color` (set/get/watch/inspector) | ✅ |
  | trigger | `fire_property_trigger()` + watchable (signal when fired) | ✅ |
  | enum | `String` value + inspector dropdown from the file's enum values | ✅ |
  | nested viewModel | slash paths (`"a/b/c"`) into nested properties; `replace_view_model` instance swapping | ✅ |
  | list | append/remove_at/swap/clear + per-item property read/write; watchable size | ✅ |
  | image | assign a Godot `Image`/`Texture2D` via `set_property` (PNG-encoded, decoded through the context factory) | ✅ API (no artboard-bound image fixture exists to verify visually) |
  | artboard | `set_artboard_property(path, artboard_name)` | ✅ |

  Inspector exposure follows the same rule: if Rive can author it, it shows up (scalars as fields today; enums as dropdowns, triggers as buttons, lists/images via the inspector plugin).
- **G4.5** Time is Godot time: `process_callback` (idle/physics/**manual** `advance(delta)`), `speed_scale`, `Engine.time_scale`, standard `process_mode` pause inheritance. Godot Timers/Tweens compose with no special casing.
- **G4.6** Efficiency defaults: `pause_when_hidden`, settled state machines sleep until an input or pointer wakes them.
- **G4.7** Input & focus: `RiveControl` forwards pointer/keyboard/gamepad input into artboard space; Rive's FocusManager bridges to Godot's focus chain so Rive-authored UI participates in tab order and controller navigation.
- **G4.8** Full C# parity via ClassDB registration; idiomatic C# events.
- **G4.9** Lifetime safety by construction: GDScript-facing objects hold opaque handles (CommandQueue model); freeing a node/resource can never use-after-free a Rive object.

## G5. Content features

- **G5.1** Text: Rive's HarfBuzz/SheenBidi engine (`WITH_RIVE_TEXT`), symbol-renamed to coexist with Godot's own HarfBuzz; data-bound strings preferred, legacy text runs supported; plays well with `tr()` and translation remaps. Fallback fonts: ✅ `RiveFileResource.add_fallback_font(FontFile|bytes)` global registry (a feature rive-unity lacks).
- **G5.2** Layout: `WITH_RIVE_LAYOUT` (Yoga) — responsive artboards resize with Control layout. ✅ `fit = Layout` resizes the artboard to the node (Yoga reflow); full fit-mode set (contain/cover/fill/fit-width/fit-height/none/scale-down/layout) + 9-anchor alignment on all three nodes.
- **G5.3** Audio: external-audio-engine mode; Rive audio routes through Godot's audio server via `RiveAudioStream` on any `AudioStreamPlayer`/bus. ✅ (shared engine + per-node `audio_bus` routing via dedicated engines)
- **G5.4** Luau scripting in `.riv` files (`WITH_RIVE_SCRIPTING`): scripted drawables/path effects/interpolators/data converters just work — sandboxed in Rive's VM, signature-verified (never the test keypair). ✅ enabled AND behaviorally verified: an editor-signed script (authored via the Rive MCP, `tests/project/fixtures/rivegd_fixtures.riv`) runs its Luau in the Godot runtime, driving a view-model round-trip the smoke asserts. Project-setting kill-switch pending. Luau is inside-the-artboard behavior; GDScript/C# is game logic; data binding + events are the handoff.
- **G5.5** Events with custom properties surface as Dictionaries; audio events surface distinctly.

## G6. Build, distribution, sustainability

- **G6.1** Two-stage build: pinned rive-runtime submodule built via its premake scripts (`--with_rive_text --with_rive_layout --with_rive_audio=external --with_rive_scripting`, per-platform), linked by godot-cpp SCons into the GDExtension. ABI knobs (CRT, stdlib, NDK, Emscripten) matched deliberately.
- **G6.2** CI matrix producing all platform binaries; addon layout (`addons/rive/`) ready for the Godot Asset Library and GitHub releases.
- **G6.3** Versioning honesty: `compatibility_minimum = 4.7`; per-Godot-minor compatibility CI (forward binary compat is *not* guaranteed by Godot); deliberate, pinned rive-runtime upgrades.
- **G6.4** MIT licensed end-to-end (rive-runtime and its vendored deps are MIT/permissive).
- **G6.5** Docs & examples: in-editor class reference, and a demo project covering menu UI (focus/controller nav), HUD (data binding), character sticker (events), 3D screen (`RiveTexture`).

## G7. Quality: testable by construction

Design in [`docs/development-and-testing.md`](docs/development-and-testing.md).

- **G7.1** Layered code (`core/` and `render/` are Godot-free) so most logic is unit-testable without an engine, and no test tier needs a physical GPU (lavapipe/llvmpipe in CI).
- **G7.2** Five-tier pyramid: C++ unit tests on rive's `NoOpFactory` (+ ASAN/UBSAN/TSAN), deterministic draw-stream regression via `SerializingFactory`, per-backend native GPU goldens, headless-Godot integration suites (gdUnit4, incl. screenshot goldens through the real Texture2DRD path), editor/import smoke tests.
- **G7.3** Benchmarks with checked-in budgets gate performance regressions (backs success criterion #4).
- **G7.4** Every phase has a test-first CI gate; rive-runtime submodule bumps are reviewed as draw-stream/golden diffs (backs G6.3).
- **G7.5** Fast inner loop: SCons dev builds + GDExtension hot reload into a symlinked demo project; sanitizer and trace-log (`RIVE_GD_TRACE`) debugging paths documented.
- **G7.6** Web builds are tested in real browsers, not just compiled: Playwright harness covering dlink load, both WebGL2 render paths (pixel-local-storage and forced-MSAA fallback), single-thread behavior parity, an Emscripten/template lockstep gate, and a wasm size budget.

## Roadmap (gap-driven, from docs/comparison-rive-unity.md)

Ordered by value; derived from the July 2026 feature comparison against
rive-unity (Rive's first-party Unity runtime):

1. **Out-of-band assets** (G3.6) — referenced images/fonts/audio resolve
   through Godot; rive-unity ships this. Buildable now.
2. **Platform matrix** — Metal (macOS/iOS), D3D12, Android GLES3, WebGL2.
   The defining gap (rive-unity ships 6 backends); hardware/toolchain-gated,
   RenderBridge is ready for it.
3. **Input polish** — multitouch; listener-aware hit testing (block pointer
   events only where a Rive listener is — Unity's "Translucent").
4. **Verification fixtures** for scrolling/virtualization + N-slicing
   (authored via the Rive editor MCP).
5. **Ergonomics** — per-node audio bus routing, DPI/layout scale modes.
6. **Later** — panel compositor (many artboards, one texture), procedural
   rendering API, RenderTexture→image binding, texture compression
   (roadmap for rive-unity too).

Already ahead of rive-unity (keep green): keyboard/focus/text input,
gamepad, ordering guarantees, headless, behavioral script verification.
(Unity ships scripting too as of v0.4.1 — parity there, not a lead.)
Also tracked from the official feature-support matrix: file-handle caching
(partial for us), fallback fonts (parity-❌), semantics/AccessKit bridge
(first-mover opportunity).

## Non-goals (scope discipline)

- **N1** No authoring: we render/drive `.riv` files; the Rive editor is the authoring tool. No .riv writing/patching.
- **N2** No custom CPU rasterizer or canvas-API translation fallback — targets without a supported GPU path get logic-only behavior (G2.4), not degraded rendering.
- **N3** No Godot→Luau scripting channel; no unsigned-script execution.
- **N4** No dedicated 3D node initially — `RiveTexture` covers 3D use.
- **N5** No Godot 3.x / GDNative support.

## Success criteria ("defacto" test)

1. A designer imports a `.riv`, wires a menu with zero code (inspector + signal connections), and it looks identical to the Rive editor.
2. A gameplay programmer binds view-model properties and events in <10 lines of idiomatic GDScript or C#.
3. One project exports to Windows, macOS, Linux, Android, iOS, and web with Rive content working on each, from the same addon.
4. A scene with 50 live artboards holds frame budget on a mid-range phone (sleeping/batching doing its job).
5. Script-bearing `.riv` files (Luau) from the current Rive editor load and behave correctly with scripting enabled — and fail safe with it disabled.
6. Upgrading Godot minor or rive-runtime version is a routine, CI-verified bump — not an event.

## Phased delivery

| Phase | Goals unlocked |
|---|---|
| 0 — skeleton + build proof | G6.1, G2.4 (headless logic), G7.1–G7.2 foundations |
| 1 — desktop Vulkan render | G1.1–G1.4 (Vulkan), G4.1 partial |
| 2 — pipeline + API | G3.*, G4.* complete on desktop |
| 3 — platform breadth | G1.3 (Metal/D3D12/Android/GL), G2.1–G2.2 |
| 4 — web + features | G2.3, G5.* |
| 5 — hardening + release | G1.5, G6.2–G6.5, G7.3 budgets locked, success criteria audit |
