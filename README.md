# rivegd

[Rive](https://rive.app) integration for Godot 4.7+, built on Rive's official
C++ runtime and GPU renderer. Runs on the Vulkan renderers on desktop
(Linux and Windows) and on WebGL2 in web exports.

```gdscript
var file: RiveFileResource = load("res://ui/menu.riv")   # no import step
$RiveControl.file = file                                 # pointer listeners just work
$RiveControl.set_bool_input("muted", true)               # state machine inputs
$RiveControl.set_property("stats/health", 0.75)          # data binding
$RiveControl.on_event("purchase", func(p): buy(p.sku))
$Grid.list_append("items", "CardVM")                     # dynamic lists
```

## Features

- Rendering through the Rive Renderer on Godot's own graphics device —
  no CPU rasterization, no texture copies. Vector feathering and the rest
  of Rive's renderer-only features work.
- `RiveSprite2D`, `RiveControl` (forwards mouse/touch/keyboard/gamepad into
  the artboard), and `RiveTexture` for use in any material or texture slot,
  including interactive Rive on 3D meshes.
- Load `.riv` files directly with `load()`; hot reload on re-export;
  referenced (out-of-band) assets resolve from sibling files; editor
  thumbnails, inspector preview, and drag-and-drop instantiation.
- State machine inputs and view-model properties are real Godot properties:
  inspector-editable, animatable, tweenable. Signals for events, state
  changes, and property changes, plus `on_event`/`on_property` helpers.
- Data binding for every view-model type Rive can author, including lists
  (with per-item access) that drive Rive layout at runtime, nested view
  models, and artboard/image properties.
- Fit modes including `Layout`, which resizes the artboard so Rive layouts
  reflow with the Control; resizing preserves state machine state.
- Multitouch, drag scrolling of Rive scroll containers, and an optional
  listener-aware hit-test mode where clicks over empty regions fall
  through to the controls behind.
- Rive text (including RTL), with an API for registering fallback fonts.
- Luau scripting support (scripts authored in the Rive editor run as-is).
- Rive audio through Godot's mixer, with optional per-node bus routing.
- Optional accessibility support: Rive's semantic tree is mirrored into
  Godot's accessibility system for screen readers.
- Full API available from C#.
- Web export support (`platform=web` GDExtension side module; works with
  Godot's dlink templates).

Not supported yet: macOS/iOS (Metal), Android. The desktop
Compatibility renderer can't render Rive content (Godot creates a GL 3.3
context; the renderer needs GL 4.2 on desktop) — files still load and state
machines still run, useful for servers and tests.

## Installing

Grab `rivegd-<version>.zip` from
[Releases](https://github.com/Ell/rivegd/releases) and extract it into
your project's `addons/` folder. Prebuilt binaries cover Linux x86_64,
Windows x86_64, and web exports; other platforms build from source
(below).

## Building

```sh
git clone --recurse-submodules https://github.com/Ell/rivegd
cd rivegd
tools/build_rive.sh                # stage 1: rive static libs
                                   # (clang, ninja, uuid-dev, glslang-tools, libvulkan-dev)
python3 -m venv .venv && .venv/bin/pip install scons
.venv/bin/scons                    # stage 2: the extension → addons/rive/bin/
godot --path demo                  # example scenes
```

For web exports, see [`docs/usage.md`](docs/usage.md#web-exports).

## Documentation

- [`docs/usage.md`](docs/usage.md) — full usage guide: nodes, data binding,
  overlays, dynamic lists, 3D, web, audio, accessibility.
- [`demo/`](demo/) — runnable examples.
- In-editor class reference (F1 on any Rive class).
- [`CLAUDE.md`](CLAUDE.md) — build details and pitfalls for contributors.

## License

MIT — see [`LICENSE`](LICENSE). rive-runtime is © Rive, MIT-licensed.
