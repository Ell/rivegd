# rivegd demo

A single scene exercising the public API, built entirely in `main.gd` so it
doubles as example code:

- **RiveControl** playing `light_switch.riv` — click the switch (Rive pointer
  listeners) or use the CheckButton (`set_bool_input`).
- **RiveSprite2D** playing `bullet_man.riv`'s state machine.
- **RiveTexture** feeding a stock `TextureRect` — Rive as a plain `Texture2D`.
- **Data binding**: a slider writing the `width` view-model property
  (`set_property`), with `watch_property` + `property_changed` feeding the log.
- **Signal log**: `rive_event`, `state_changed`, `property_changed`.

There are two scenes:
- `main.tscn` — the API showcase (nodes, inputs, events, data binding).
- `overlays.tscn` — the "Rive over a game" patterns: health bars that
  follow moving enemies (independent `RiveSprite2D` children, data-bound)
  and a `mouse_filter=IGNORE` dialogue box. (Uses test artboards as
  stand-ins — swap in your own bar/dialogue `.riv`.)

Run it (after building the extension — see the repo README):

```sh
godot --path demo
```

Assets are symlinked from `tests/project/fixtures/` (rive-runtime's MIT test
assets). Needs a Forward+/Mobile (Vulkan) renderer.
