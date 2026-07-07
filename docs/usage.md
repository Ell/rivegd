# Using rivegd

Rive playback for Godot **4.7+** as a GDExtension. This guide covers today's
API (Phase 2b). For where the project is heading, see [`GOALS.md`](../GOALS.md)
and [`ux-design.md`](ux-design.md).

## Current requirements & limitations

- **Godot 4.7+, Forward+ or Mobile rendering method (Vulkan).**
  macOS/iOS (Metal), D3D12, Android GLES3, and web are on the roadmap
  (GOALS G1.3/G2). A GL/GLES backend is already built in, but **desktop
  Compatibility cannot render**: Godot creates a GL 3.3 core context and
  rive's desktop-GL minimum is 4.2 (GLES minimum is 3.0, so Android/web
  qualify). Unsupported renderers degrade to logic-only: `.riv` files load
  and state machines advance, nothing draws.
- Prebuilt binaries: Linux x86_64 only so far (CI artifacts). Other desktop
  platforms build from source (below).
- Data binding: typed writes, watch-based reads, and per-path change
  signals (GOALS G4.4).

## Install

1. Copy `addons/rive/` into your project (or symlink it while developing).
2. Place the built library at
   `addons/rive/bin/<platform>/librivegd.<platform>.<target>.<arch>.so|dll`
   (CI artifacts follow this layout; `scons` puts them there automatically).
3. Open the project — the extension registers `RiveFileResource`,
   `RiveSprite2D`, and `RiveControl`.

## Quick start

Drop a `.riv` file anywhere in your project. It loads like any resource —
no import step:

```gdscript
var file: RiveFileResource = load("res://ui/menu.riv")
print(file.get_artboard_names())            # ["MainMenu", ...]
print(file.get_state_machine_names(""))     # state machines of the default artboard
print(file.get_input_descriptions("", "")) # [{name, type: "bool"|"number"|"trigger", default}]
```

### In the scene

Add a **`RiveSprite2D`** (world-space 2D) or **`RiveControl`** (UI) node and
assign the `file` property in the inspector. `artboard` and `state_machine`
become dropdowns populated from the file; leaving them empty uses the
defaults. The node plays immediately, in the editor and at runtime.

- `RiveSprite2D` renders at its `size` (pixels) with the artboard
  contain-fitted and centered.
- `RiveControl` renders at its Control rect size (resize-aware) and forwards
  mouse input to the artboard, so **Rive listeners (hover/click/drag) work
  with zero code**. Standard `mouse_filter` semantics apply.

### State machine inputs

Inputs appear directly in the inspector under an **Inputs** group (booleans
as checkboxes, numbers as float fields) — and they are *real properties*, so
AnimationPlayer tracks and Tweens can drive them:

```gdscript
$Menu.set_bool_input("muted", true)
$Menu.set_number_input("health", 0.75)
$Menu.fire_trigger("open")

# Real properties too:
create_tween().tween_property($Menu, "inputs/health", 1.0, 0.5)
```

### Data binding (view models)

If the artboard has a view model, it is bound automatically (its default
instance when one is declared, a fresh instance otherwise). Top-level scalar
properties also appear in the inspector under a **Data Binding** group.
Properties are path-addressed and typed by the Variant you pass
(bool / number / String / Color):

```gdscript
$HUD.set_property("stats/health", 0.75)
$HUD.set_property("user/name", "ell")
$HUD.set_property("theme/accent", Color.CRIMSON)
$HUD.fire_property_trigger("stats/level_up")
```

Reads are watch-based (values live on the render thread, so reads are one
frame behind writes):

```gdscript
$HUD.watch_property("stats/health")    # reports the current value immediately
$HUD.property_changed.connect(func(path, value):
    if path == "stats/health":
        health_bar.value = value)
print($HUD.get_property("stats/health"))   # last value seen
```

Values set from script or the inspector are cached and re-applied when the
instance rebuilds (file swap, resize, hot reload); watches re-arm too.

Lists hold view-model instances; watch the list path to observe its size:

```gdscript
$HUD.watch_property("inventory")                  # size changes via property_changed
$HUD.list_append("inventory", "ItemVM")           # instance of a named view model
$HUD.list_set_property("inventory", 0, "label", "Sword")
$HUD.list_swap("inventory", 0, 1)
$HUD.list_remove_at("inventory", 0)
$HUD.list_clear("inventory")
```

Structural list state lives on the render thread and does not survive
instance rebuilds.

Images, artboards, and nested instances:

```gdscript
$HUD.set_property("avatar", my_image_or_texture)      # image property
$HUD.set_artboard_property("slot", "CoolWidget")      # bindable artboard by name
$HUD.replace_view_model("child", "ChildVM", "variant-b")  # swap nested instance
```

### Events & state changes

Rive events surface as a signal with custom properties as a Dictionary;
state-machine transitions report the entered animation state's name:

```gdscript
$Menu.rive_event.connect(func(event_name: String, properties: Dictionary):
    if event_name == "purchase":
        buy(properties.get("sku", "")))

$Menu.state_changed.connect(func(state_name: String):
    print("entered state: ", state_name))
```

### Keyboard & focus

`RiveControl` forwards key events (GLFW-mapped) and printable text to Rive's
focus system when focused — Rive keyboard listeners and text fields work.
Focus inside the artboard is steppable from script:

```gdscript
$Menu.focus_mode = Control.FOCUS_ALL
$Menu.focus_next_element()      # walk Rive's internal tab order
$Menu.focus_previous_element()
$Menu.send_text_input("hello")  # programmatic text entry
```

Bridging Rive's focus edge into Godot's own focus chain (yielding to the
next Control) is on the roadmap (GOALS G4.7).

### Hot reload

Nodes rebuild automatically whenever their `RiveFileResource` changes —
re-exporting a `.riv` over the same file (or calling `set_data`) updates
every live instance; inspector-set inputs and view-model properties are
re-applied by name.

### Time

- `playing` starts/stops advancing; `speed_scale` multiplies the delta
  (`Engine.time_scale` also applies, since deltas are process deltas).
- Pause is Godot-standard: a paused tree pauses Rive; use
  `process_mode = PROCESS_MODE_ALWAYS` for pause menus that keep animating.
- **Efficiency defaults**: hidden nodes stop advancing (`pause_when_hidden`,
  default on), and a settled state machine sleeps — GPU work stops until any
  input, pointer event, or property write wakes it. Both are automatic.

## Building from source

```sh
git clone --recurse-submodules https://github.com/Ell/rivegd
cd rivegd

# Stage 1: rive-runtime static libs (needs clang, ninja, uuid-dev,
#          glslang-tools, libvulkan-dev)
tools/build_rive.sh

# Stage 2: the GDExtension (needs scons; use a venv if not installed)
python3 -m venv .venv && .venv/bin/pip install scons
.venv/bin/scons                      # debug; add target=template_release for release

# Tests
tools/test.sh all
```

Linux needs: `clang`, `ninja`, `scons`, `uuid-dev`, `glslang-tools` (Vulkan
shader generation), `libvulkan-dev`, and a C++17 gcc for the
unit tests. The stage-1 build self-installs its pinned premake and fetches
rive's vendored dependencies on first run.

## Troubleshooting

- **"no RenderingDevice (Compatibility renderer or headless)"** — switch the
  project to Forward+ or Mobile (Project Settings → Rendering → Renderer).
- **Godot 4.7 headless `--import` crashes at exit** with any
  class-registering GDExtension (upstream bug; repro in
  `tests/upstream/godot-headless-import-crash/`). The import completes —
  rerun or ignore the exit code in CI scripts.
- **Nothing renders but no errors**: check the node is inside the tree and
  `file.is_valid()`; `get_import_error()` explains malformed files.
