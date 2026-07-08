# rivegd

**Rive for Godot** — a GDExtension integrating [Rive](https://rive.app)'s official C++ runtime and GPU renderer (the Rive Renderer, running natively on Godot's graphics device) into Godot 4.7+. Runs on desktop Vulkan and **in the browser** (WebGL2).

```gdscript
var file: RiveFileResource = load("res://ui/menu.riv")   # no import step
$RiveControl.file = file                                 # pointer listeners just work
$RiveControl.set_bool_input("muted", true)               # state machine inputs
$RiveControl.set_property("stats/health", 0.75)          # data binding (all VM types)
$RiveControl.on_event("purchase", func(p): buy(p.sku))   # targeted callbacks
$Grid.list_append("items", "CardVM")                     # dynamic lists reflow live
```

## What works today

| Area | Status |
|---|---|
| Rendering | Rive Renderer on Godot's Vulkan device (zero-copy RD textures) and WebGL2 (web export, browser-verified); one flush per frame, fence-ring-aware batched submissions |
| Platforms | Linux (editor + export), **Web** (`platform=web` wasm side module, dlink) — macOS/Windows/mobile tracked in [`GOALS.md`](GOALS.md) |
| Nodes | `RiveSprite2D`, `RiveControl`, `RiveTexture` (any texture slot, interactive on 3D meshes via `send_pointer_uv`) |
| Fit & layout | full fit-mode set (contain/cover/fill/…/**layout** — real Yoga reflow), 9-anchor alignment, `layout_scale` (DPI recipes); live reflow during Control resize with **state-machine state preserved** across texture swaps |
| Assets | `load("res://foo.riv")` directly; hot reload; out-of-band (referenced) assets auto-resolve by sibling-file convention; one shared import per resource; FileSystem thumbnails; inspector preview; drag-drop a `.riv` onto the 2D viewport to instantiate |
| State machines | inputs as inspector properties, trigger fire-buttons, `state_changed`, event-ordering guarantees (O1–O4) pinned by a dedicated suite |
| Events & callbacks | `rive_event(name, props)`, `loaded`, `property_changed` signals + targeted `on_event(name, cb)` / `on_property(path, cb)` |
| Data binding | **every** VM type incl. per-item list read/write and color watch; dynamic lists drive rive-native layout (append → cards appear, verified) |
| Input | pointer (multitouch, per-finger ids), listener-aware hit testing (`hit_test_behavior = Translucent`: clicks over empty regions fall through), keyboard + focus, text input, gamepad batching, interactive **scroll** (drag a rive Scroll-constrained list) |
| Text | Rive's HarfBuzz/SheenBidi engine (RTL verified); **fallback fonts** (`RiveFileResource.add_fallback_font`) — a feature rive-unity doesn't expose |
| Scripting | Luau VM compiled in; editor-signed scripts **run** (behaviorally verified in CI and in-browser) |
| Audio | Rive audio through Godot's mixer; per-node bus routing (`audio_bus = "UI"`) |
| Accessibility | `accessibility_enabled` mirrors rive's semantic tree into Godot's AccessKit — **first Rive game runtime with screen-reader support** |
| C# | full API via ClassDB, verified headless in CI |
| Perf | settled artboards sleep (500 static cards ≈ free); 500 *continuously-animating* 128px artboards at 13 ms avg / 14.6 ms p99 (RTX 4090); 50 heavy artboards ≈ 10.3 ms |
| Editor | in-editor class reference (F1), configuration warnings, live preview, demo project |
| Tests | 66-assertion unit suite + 14 pixel-asserting smoke scenes (api, ordering, render, cards, scroll, reflow, fallback-font, click-listener, translucent, multitouch, multi-instance, resize, semantics, audio-bus, 3D, overlay, web) |

**Not yet:** macOS/iOS (Metal), Windows (D3D12), Android (GLES3 — the GL bridge targets it; build pending); desktop Compatibility cannot render (Godot creates GL 3.3, rive's desktop floor is 4.2 — [details](docs/usage.md)); declarative keyboard listeners (blocked upstream — the Rive editor only authors pointer listeners today).

## Try it

```sh
git clone --recurse-submodules https://github.com/Ell/rivegd
cd rivegd
tools/build_rive.sh                          # stage 1 (clang, ninja, uuid-dev, glslang-tools, libvulkan-dev)
python3 -m venv .venv && .venv/bin/pip install scons
.venv/bin/scons                              # stage 2
godot --path demo                            # showcase scene (overlays.tscn: health bars + dialogue)
```

Web export: `tools/build_rive_web.sh`, then `scons platform=web target=template_release threads=no` (see [`docs/usage.md`](docs/usage.md#web-exports) — three non-obvious Emscripten flags are baked in).

## Documentation

- [`docs/usage.md`](docs/usage.md) — **how to use the extension** (nodes, data binding, overlays, dynamic lists, 3D, web, accessibility, fallback fonts)
- [`demo/`](demo/) — runnable examples (API showcase + game-overlay patterns)
- [`GOALS.md`](GOALS.md) — goals, status, roadmap, success criteria
- [`docs/comparison-rive-unity.md`](docs/comparison-rive-unity.md) — feature-by-feature vs Rive's first-party Unity runtime (we exceed it in four areas)
- [`docs/implementation-strategy.md`](docs/implementation-strategy.md) — architecture & research evidence
- [`docs/development-and-testing.md`](docs/development-and-testing.md) — repo layout, test tiers, CI
- [`CLAUDE.md`](CLAUDE.md) — build/test commands and hard-won gotchas for contributors (human or AI)

## How it compares

Measured against **rive-unity** (Rive's own first-party runtime): parity or better on every feature row of Rive's official support matrix — and rivegd additionally ships fallback fonts, accessibility, keyboard/text/gamepad input, and behavioral script verification, which rive-unity doesn't ([full comparison](docs/comparison-rive-unity.md)). Prior Godot attempts split into CPU-raster (Skia + pixel copies — an approach Rive itself moved away from) and GPU-native; rivegd differentiates on frame-sync rigor, breadth of *verified* behavior, and editor UX. If you need macOS/D3D12 today, [RiveGD](https://github.com/maidopi-usagi/RiveGD)'s backend breadth is still ahead there.

## License

MIT — see [`LICENSE`](LICENSE). rive-runtime is © Rive, MIT-licensed.
