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
- **C# works** — same API via ClassDB (`obj.Call("set_property", ...)`,
  signals into `Callable.From`); verified headless in CI
  (`tests/csharp/CsSmoke.cs` is the example).
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
$HUD.list_read_property("inventory", 0, "label")   # -> property_changed("inventory[0]/label", ...)
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

### Targeted callables

Sugar over the signals — filter by name/path without writing the `if` yourself:

```gdscript
$Menu.on_event("purchase", func(props): buy(props.sku))
$HUD.on_property("stats/health", func(v): bar.value = v)  # auto-watches the path
```

Both return the connected `Callable`; pass it to `disconnect("rive_event", cb)`
/ `disconnect("property_changed", cb)` to unsubscribe.

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

### Luau scripting in .riv files

Script-bearing files (scripted drawables, path effects, interpolators, data
converters authored in the Rive editor) just work — the runtime instances a
sandboxed Luau VM per file. Scripts are **signature-verified against Rive's
production key**: editor-exported files run their scripts; unsigned or
sample-signed bytecode is rejected gracefully (the file still loads and
plays its non-scripted content). There is no Godot→Luau channel by design —
data binding and events are the handoff (see GOALS N3).

### Audio

Rive audio events route through a shared engine that Godot's mixer pulls
from — add a [code]RiveAudioStream[/code] to any `AudioStreamPlayer` (pick
its bus for mixing/ducking) and play it once at startup:

```gdscript
var player := AudioStreamPlayer.new()
player.stream = RiveAudioStream.new()
player.bus = "SFX"
add_child(player)
player.play()
```

All Rive instances share the engine; without a playing RiveAudioStream,
Rive audio is silently dropped.

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

### Out-of-band (referenced) assets

Rive files can *reference* images/fonts/audio instead of embedding them
("out-of-band" exports). rivegd resolves these automatically: place the
asset files next to the `.riv` using Rive's export naming
(`<name>-<id>.png` etc. — exactly what the editor's export produces) and
they decode and render with no code. Inspect what a file references:

```gdscript
for a in rive_file.get_asset_descriptions():
    print(a["unique_filename"], " type=", a["type"], " resolved=", a["resolved"])
```

`resolved=false` after load means the sibling file was missing.

### Accessibility (screen readers)

Rive lets designers annotate artboards with semantics (roles, labels).
`RiveControl.accessibility_enabled = true` mirrors that semantic tree into
Godot's accessibility system (AccessKit) as live sub-elements — buttons,
text, sliders inside your Rive UI become visible to screen readers, with
bounds tracking the node's fit transform. No other game-engine Rive
runtime exposes this today. (Elements publish when assistive technology is
active; the semantic stream itself is inspectable via
`get_semantics_node_count()`.)

### Fallback fonts

When a Rive text run needs a glyph its authored font lacks (CJK,
localization, symbols), registered fallbacks are consulted in order —
global, one line (rive-unity doesn't expose this yet):

```gdscript
RiveFileResource.add_fallback_font(load("res://fonts/NotoSansJP.ttf"))
RiveFileResource.add_fallback_font(japanese_bytes)  # PackedByteArray works too
```

`RiveFileResource.clear_fallback_fonts()` resets. Register before loading
text-bearing scenes; shaping picks fallbacks up on the next re-shape.

### Per-node audio routing

By default every artboard's audio mixes into one shared stream (put a
`RiveAudioStream` on any `AudioStreamPlayer`). To route one node's Rive
audio to its own Godot bus (ducking, reverb zones, per-UI volume):

```gdscript
$Menu.audio_bus = "UI"   # done — internal player + dedicated engine
```

Power users can instead set `RiveAudioStream.instance_id` on their own
player to pull a specific node's dedicated mix.

### Dynamic lists (inventories, card grids)

Author a component artboard + a list artboard whose view model has a
`list` property feeding a component list (see
`tests/project/fixtures/README.md` for the recipe). Then from GDScript:

```gdscript
$Grid.list_append("items", "CardVM")          # a card appears in the layout
$Grid.list_set_property("items", 1, "value", 0.25)  # only card 1 changes
$Grid.list_clear("items")                     # grid empties
```

Verified end-to-end by `cards_smoke`; 200-500 static cards bench at ~6 ms
(settled instances sleep — G4.6). Author a Scroll constraint on the list's
layout and pointer drags scroll it (scroll_smoke) — move timestamps drive
rive's scroll physics.

### Overlays over a game (HUDs, health bars, dialogue)

Rive shines as UI *over* gameplay. The patterns:

- **World-anchored (health bar over an enemy):** make a `RiveSprite2D` a
  **child** of the entity node — it follows automatically, captures no
  input, and each instance is fully independent (many enemies = many bars,
  each with its own data binding). Data-bind the bar to the entity's stats:

  ```gdscript
  var bar := RiveSprite2D.new()
  bar.file = load("res://ui/health_bar.riv")
  enemy.add_child(bar)                  # follows the enemy for free
  # each frame / on damage:
  bar.set_property("fill", enemy.health)   # independent per instance
  ```

- **Screen HUD / dialogue box:** a `RiveControl` with
  `mouse_filter = MOUSE_FILTER_IGNORE` — it animates and data-binds but lets
  gameplay clicks fall **through** to the game (verified). Use the default
  `STOP` instead for a modal box that has its own buttons.
- **In 3D:** put a `RiveTexture` on a billboarded quad's material for a
  world-space bar, or a `RiveControl` in a `CanvasLayer` positioned each
  frame from `camera.unproject_position(entity.global_position)`.

### Fit & alignment

`fit` on every node (`RiveSprite2D`, `RiveControl`, `RiveTexture`) controls
how the artboard maps to its texture — mirroring the Rive editor/other
runtimes: `Contain` (default), `Cover`, `Fill`, `Fit Width`, `Fit Height`,
`None`, `Scale Down`, and `Layout`. **Layout** resizes the artboard itself
so its Rive layout (Yoga) reflows to the node's size — the mode to use for
responsive full-screen UI ("fits any screen size"). `alignment` picks the
anchor (9-grid) for modes that don't fill exactly. Pointer input and
listener-aware hit testing invert the same transform, so clicks stay
accurate in every mode.

Resizing a `RiveControl` is cheap and stateful: with `fit = Layout` the
artboard reflows **live** while the size changes, and once the size
settles (0.3 s) only the GPU texture is swapped — the state machine, view
model, and playback position all survive (verified: a toggled switch stays
toggled through a resize).

With `fit = Layout`, `layout_scale` sets the content scale (Unity's
"Layout Scale Factor"): the artboard is laid out at `size / layout_scale`
and drawn scaled up. DPI recipes: constant pixel size = leave at 1.0;
constant physical size = `DisplayServer.screen_get_dpi() / 160.0` (or your
reference DPI), updated on window/screen change.

### Loaded signal

Instance creation is asynchronous (queued to the render thread).
`RiveSprite2D`/`RiveControl` emit `loaded` when the instance is live —
the equivalent of rive-unity's `WidgetStatus.Loaded`:

```gdscript
rive_node.loaded.connect(func(): rive_node.set_property("health", 50.0))
```

(Property writes before `loaded` are safe — they're cached and replayed —
but reads return values only after load.)

### Multitouch

Touch input tracks per finger: `InputEventScreenTouch`/`ScreenDrag` forward
their touch index as the Rive pointer id, so simultaneous presses (dual
thumbsticks, pinch UIs, multi-button boards) work as authored.
`send_pointer_event(phase, position, pointer_id)` exposes the same for
programmatic/3D input.

### Interactive Rive on 3D surfaces

`RiveTexture` works in any material, and it accepts pointer input in **UV
space** — so a Rive UI projected onto a mesh (sphere, screen, curved panel)
stays fully interactive. Raycast the mesh, derive the hit UV, forward it:

```gdscript
func _on_mesh_clicked(hit_position: Vector3) -> void:
    var uv := uv_at(hit_position)      # your mesh's UV at the hit point
    rive_texture.send_pointer_uv(1, uv)  # down
    rive_texture.send_pointer_uv(2, uv)  # up  -> Rive listeners fire
```

Phases: 0 move, 1 down, 2 up, 3 exit. Godot raycasts don't return UVs, so
`uv_at` is per-mesh: analytic for primitives (see the sphere inversion in
`tests/project/texture3d_smoke.gd`, which pins this whole chain), or
face-UV interpolation via `MeshDataTool` for arbitrary meshes. Hover works
too — forward phase 0 from mouse motion for rollover states.

For interactive overlays over an interactive game, set
`hit_test_behavior = 1` (Translucent): the control captures pointer events
**only where the artboard has an interactive listener** — clicks over
empty/decorative regions fall through to whatever is behind. This is
listener-aware (Rive's own hit test), more correct than alpha-based
click-through: invisible hit areas still capture; opaque decoration does
not block. Default (0, Opaque) captures the whole rect.

See `demo/overlays.tscn` for health bars following moving enemies + a
dialogue box.

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
