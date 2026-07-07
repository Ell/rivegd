# rivegd

**Rive for Godot** — a GDExtension integrating [Rive](https://rive.app)'s official C++ runtime and GPU renderer (the Rive Renderer, running natively on Godot's Vulkan device) into Godot 4.7+.

```gdscript
var file: RiveFileResource = load("res://ui/menu.riv")   # no import step
$RiveControl.file = file                                 # pointer listeners just work
$RiveControl.set_bool_input("muted", true)               # state machine inputs
$RiveControl.set_property("stats/health", 0.75)          # data binding (all VM types)
$RiveControl.rive_event.connect(func(name, props): print(name, props))
```

## What works today

| Area | Status |
|---|---|
| Rendering | Rive Renderer on Godot's Vulkan device, zero-copy via RD textures; explicit frame sync (fenced, chunk-batched submissions) |
| Nodes | `RiveSprite2D`, `RiveControl` (mouse → Rive listeners), `RiveTexture` (any texture slot) |
| Assets | `load("res://foo.riv")` directly; hot reload; FileSystem thumbnails; live inspector preview |
| State machines | inputs as inspector properties (`inputs/*`), trigger fire-buttons, `state_changed` signal |
| Events | `rive_event(name, properties)` with custom properties |
| Data binding | **every** VM type: number/bool/string/color/enum (inspector dropdowns)/trigger/list/image/artboard/nested — writes, watch-based reads, `property_changed` |
| Text | Rive's HarfBuzz/SheenBidi engine (symbol-renamed; coexists with Godot's) |
| Scripting | Luau VM compiled in — editor-signed script-bearing files run; unsigned scripts rejected gracefully |
| Audio | Rive audio events through Godot's mixer via `RiveAudioStream` |
| C# | full API via ClassDB, verified headless in CI |
| Input | mouse/pointer listeners, keyboard+focus, gamepad batching |
| Perf | settled state machines sleep; `pause_when_hidden`; 50 artboards ≈ 9.1 ms avg on an RTX 4090 (`tests/project/bench.tscn`) |
| Editor | in-editor class reference (F1), configuration warnings, demo project |
| CI | GitHub Actions + GitLab CI: build, 41-assertion unit suite, headless smoke |

**Not yet:** macOS/iOS (Metal), D3D12, Android, web; desktop Compatibility cannot render (Godot creates GL 3.3, rive's desktop floor is 4.2 — [details](docs/usage.md)); Rive layout Control-resizing; per-node audio buses. See [`GOALS.md`](GOALS.md).

## Try it

```sh
git clone --recurse-submodules https://github.com/Ell/rivegd
cd rivegd
tools/build_rive.sh                          # stage 1 (clang, ninja, uuid-dev, glslang-tools, libvulkan-dev)
python3 -m venv .venv && .venv/bin/pip install scons
.venv/bin/scons                              # stage 2
godot --path demo                            # showcase scene
```

## Documentation

- [`docs/usage.md`](docs/usage.md) — **how to use the extension**
- [`demo/`](demo/) — runnable example scene (all API surface, built in code)
- [`GOALS.md`](GOALS.md) — goals, non-goals, success criteria, phased delivery
- [`docs/implementation-strategy.md`](docs/implementation-strategy.md) — architecture & research evidence
- [`docs/ux-design.md`](docs/ux-design.md) — the intended developer experience
- [`docs/development-and-testing.md`](docs/development-and-testing.md) — repo layout, test tiers, CI

## How it compares

Prior art splits into CPU-raster attempts (Skia + pixel-buffer copies — an approach Rive itself has moved away from) and GPU-native ones. rivegd was designed from primary sources with adversarially-verified research (see the strategy doc) and differentiates on frame-sync rigor, a pixel-asserting test suite, full data-binding type coverage, events/listeners, and editor UX. If you need macOS/D3D12 today, look at [RiveGD](https://github.com/maidopi-usagi/RiveGD), whose backend breadth is ahead of ours.

## License

MIT — see [`LICENSE`](LICENSE). rive-runtime is © Rive, MIT-licensed.
