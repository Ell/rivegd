# Rive ⇄ Godot — User Experience Design

Companion to `implementation-strategy.md`.

> **Status (July 2026):** this design is now substantially SHIPPED —
> drag-drop instantiation, inspector dropdowns/previews/thumbnails, inputs
> and data binding as real properties, signals + targeted callables
> (`on_event`/`on_property`), the `loaded` signal, pause/speed semantics,
> per-node audio buses, fit modes incl. Layout reflow, and Luau scripting
> (verified). Known deltas: awaiters are plain `await node.rive_event`
> (no bespoke helper); `animation_finished`/`input_changed` are not
> distinct signals; editor "Preview playback" toggle is the inspector
> preview rather than an in-viewport toggle. See README's feature table
> for the live status. This describes what using the integration feels like for a Godot developer, designer, or technical artist. Guiding principle: **Rive content should feel like a native Godot citizen** — signals, the inspector, Callables, pause/time-scale semantics, and focus should all behave the way a Godot user already expects, with zero boilerplate for the common cases.

---

## 1. First five minutes

1. **Drop a `.riv` into the project.** It imports automatically (raw-bytes resource), gets a rendered thumbnail in the FileSystem dock, and a preview panel in the inspector: artboard picker, state-machine/animation picker, play/pause scrubber — all live-rendered by the real renderer, so what you see is exactly what ships.
2. **Add a node.** `RiveSprite2D` (world-space) or `RiveControl` (UI). Assign the `.riv` in the inspector; `artboard` and `state_machine` are dropdowns populated from the file (no typing strings). The node renders live in the editor viewport, with an "Preview playback" editor toggle (like `GPUParticles2D`'s).
3. **Drag-and-drop shortcut:** dragging a `.riv` into the 2D viewport instantiates a configured `RiveSprite2D`; dragging into a Control hierarchy instantiates a `RiveControl`.

## 2. Inspector: state machine inputs & view models as real properties

The node's `_get_property_list()` reflects the selected state machine dynamically:

- **Bool input** → checkbox. **Number input** → float field/slider. **Trigger** → an inspector *button* (via `EditorInspectorPlugin`) that fires it live in the editor preview.
- **View model properties** (Rive's data binding) appear as a `Data Binding` inspector group: numbers, strings, booleans, colors (Godot color picker), enums (dropdown populated from the .riv's enum definitions), nested view models as foldable subgroups, path-addressed (`stats/health`).
- All of these are **animatable by Godot's AnimationPlayer and tweenable** because they are real properties — e.g. `create_tween().tween_property(rive_node, "input/progress", 1.0, 0.5)` just works. This one decision buys interop with the entire Godot animation/tween ecosystem for free.

Editing a value while the editor preview runs updates the artboard immediately — designers can tune a menu screen without running the game.

## 3. Script callbacks — yes, three tiers

**Tier 1 — signals (the Godot-native default).** Connectable from the editor's Node dock like any built-in node:

```gdscript
# Rive events reported by the state machine (with custom properties as a Dictionary)
rive.rive_event.connect(func(event_name: String, properties: Dictionary):
    if event_name == "coin_collected":
        score += properties.get("value", 1))

rive.state_changed.connect(_on_state_changed)   # (state_machine, from_state, to_state)
rive.animation_finished.connect(_on_outro_done) # one-shot animations
rive.input_changed.connect(_on_input_changed)   # state machine changed its own inputs
```

Under the hood: after every `advance()`, the runtime's `reportedEventCount()/reportedEventAt()` queue is drained and re-emitted as deferred signals on the main thread — safe to call any scene-tree API from handlers.

**Tier 2 — targeted Callables (no signal-name string matching).**

```gdscript
rive.on_event("coin_collected", _collect)           # filter by event name
var sub := rive.bind_property("stats/health", func(v): health_bar.value = v)
sub.unbind()                                        # returns a subscription handle
```

`bind_property` is two-way data binding's read side; writes are `rive.set_property("stats/health", 75)` (or the inspector property). This is the recommended surface — Rive is steering everything (text, colors, lists, nested artboards) toward view models.

**Tier 3 — await, for one-shot flows.**

```gdscript
rive.fire_trigger("show_dialog")
await rive.event_fired("dialog_dismissed")   # coroutine-friendly awaiter
queue_free()
```

## 4. Timers, time, and pause — Godot semantics, honored

Rive artboards advance on a delta we control, so all of Godot's time machinery applies naturally:

- **`process_callback`**: `IDLE` (default), `PHYSICS`, or `MANUAL`. Manual mode exposes `advance(delta)` for lockstep/replay/rollback use cases and for driving Rive from an `AnimationPlayer` track.
- **`speed_scale`** property (same idiom as `AnimationPlayer`/`GPUParticles2D`), multiplied with `Engine.time_scale` — slow-mo affects Rive content unless the node opts out via `ignore_time_scale`.
- **Pause**: standard `process_mode` inheritance. A paused game pauses Rive; a `PROCESS_MODE_ALWAYS` pause-menu `RiveControl` keeps animating. No special casing — users already know this system.
- **Godot timers/tweens compose freely** because inputs are properties and methods: `get_tree().create_timer(2.0).timeout.connect(func(): rive.fire_trigger("taunt"))`. Rive-internal timing (timed transitions, listener delays) stays inside the state machine where designers authored it.
- **Frugal mode**: `pause_when_hidden` (default on) stops advancing off-screen/hidden instances; a `sleeping` state kicks in when a state machine settles and no inputs change, waking on any input/pointer — important for UI-heavy scenes with dozens of artboards.

## 5. Input, UI, and focus

- **`RiveControl`** forwards `_gui_input` pointer events into artboard space (fit/alignment-aware transform), so Rive listeners (hover, click, drag) work with zero code. `mouse_filter`, `custom_minimum_size`, size flags, and container layout all behave normally; the artboard resizes via Rive's layout engine (Yoga) when `fit = LAYOUT`.
- **Keyboard & gamepad**: the runtime now has keyboard/gamepad listener groups — `RiveControl` forwards key events when focused, and (opt-in) translates Godot input actions to Rive's gamepad snapshot, so a designer-built menu can react to controller navigation.
- **Focus**: Rive's `FocusManager` (`focusNext/focusPrevious`) is bridged to Godot's focus system — `ui_focus_next` inside a focused `RiveControl` walks Rive's internal focusables before yielding to the next Godot control. A Rive-authored button behaves like a `Button` in the tab order.
- **"Rive as a button"** recipe ships as a tiny `RiveButton` demo: Rive event `pressed` → Godot signal, `focus_entered/exited` → Rive triggers.

## 6. Luau in `.riv` files — yes, supported and sandboxed

rive-runtime ships an embeddable **Luau scripting VM** (`WITH_RIVE_SCRIPTING`): scripted drawables, path effects, interpolators, and data converters authored in the Rive editor travel *inside* the `.riv` as compiled bytecode modules, and the runtime instantiates its own VM per file when the host doesn't supply one. We build with it enabled, so **script-bearing `.riv` files just work** — no Godot-side setup.

Boundaries we deliberately keep:

- **Sandboxed & signed.** Rive scripts run inside Rive's Luau VM with Rive's API surface only — no filesystem, no Godot access. The runtime verifies script **signatures** against Rive's public key (the `WITH_RIVE_TEST_SIGNATURE` dev keypair is never enabled in shipping builds), so a `.riv` can't smuggle arbitrary unsigned bytecode. This makes loading third-party `.riv` files a defensible default.
- **Not a Godot scripting channel.** You don't inject Luau from GDScript; the designed handoff between Rive-logic and game-logic is **data binding + events** (a scripted data converter in the .riv can transform values both ways). This keeps a crisp mental model: Luau = inside-the-artboard behavior (Rive editor's domain), GDScript/C# = game behavior.
- A project setting (`rive/scripting/enabled`) can disable the VM wholesale for teams that want a smaller binary or zero-script policy; script-bearing files then degrade per Rive's runtime rules.

## 7. Text

- Preferred: data-bound string properties (`rive.set_property("title/text", tr("MENU_START"))`) — plays well with Godot localization (`tr()`, translation remaps at the resource level for swapping whole .riv files per locale).
- Legacy text runs remain addressable: `artboard.set_text_run("run_name", value)`.
- Rive's own HarfBuzz/SheenBidi stack shapes the text (full bidi/complex-script support), fonts either embedded in the .riv or resolved through our `FileAssetLoader` bridge to Godot `FontFile` resources.

## 8. Audio

Rive audio events route through a per-node `audio_bus` property (default `"Master"`) via the external-audio-engine bridge — so Rive sound effects respect Godot's mixer, ducking, and pause behavior. A `rive_audio_event` signal fires alongside for teams that prefer triggering their own `AudioStreamPlayer`s.

## 9. Beyond 2D

- **`RiveTexture`** (a `Texture2DRD`-style resource, configurable artboard/state machine/size) drops Rive output into *any* texture slot: 3D `StandardMaterial3D` albedo (animated screens/decals in 3D), `TextureRect`, shader uniforms, particles. It exposes the same input/event API minus node conveniences.
- **Viewport-independent DPI**: nodes render at their on-screen pixel size times `oversample` (default 1.0); vector content means UI stays crisp on any display without asset variants — worth advertising, it's the headline advantage over sprite sheets and Lottie rasterization.

## 10. Editor quality-of-life (the "defacto" bar)

- Thumbnails + inspector preview with state-machine scrubbing (§1).
- **Hot reload**: re-export from the Rive editor over the same file → running editor preview and even a running game (with `--debug`) swap the artboard in place, preserving current input values where names still match.
- **Warnings**: configuration problems surface as node configuration warnings (yellow triangle) — missing artboard name after a file update, state machine referencing a deleted input, scripting disabled but file contains scripts.
- **Docs**: full in-editor class reference (XML docs bound through GDExtension), plus a demo project: menu UI (RiveControl + focus), HUD (data binding), character sticker (RiveSprite2D + events), 3D screen (RiveTexture).
- **C# parity**: everything above is ClassDB-registered, so C# gets identical API with idiomatic events (`rive.RiveEvent += ...`).

## API sketch (condensed)

```gdscript
# Nodes: RiveSprite2D (Node2D), RiveControl (Control)   Resources: RiveFileResource, RiveTexture
var rive := $RiveControl
rive.file = load("res://ui/menu.riv")
rive.artboard = "MainMenu"            # dropdown-backed String
rive.state_machine = "Flow"

rive.set_input("volume", 0.8)          # number/bool inputs
rive.fire_trigger("open")
rive.set_property("user/name", "ell")  # data binding (view model path)
var vm := rive.view_model               # RiveViewModelInstance (RefCounted)

rive.rive_event.connect(_on_event)      # signals
rive.on_event("purchase", _buy)         # targeted callable
await rive.event_fired("intro_done")    # awaiter

rive.speed_scale = 0.5                  # time controls
rive.process_callback = RiveNode.PROCESS_MANUAL
rive.advance(delta)
```
