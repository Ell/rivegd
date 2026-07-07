# rivegd

**Rive for Godot** — a GDExtension integrating [Rive](https://rive.app)'s official C++ runtime and GPU renderer (the Rive Renderer, running natively on Godot's Vulkan device) into Godot 4.7+.

> Status: **Phase 2a.** Working today on Forward+/Mobile (Vulkan): `.riv` files load as resources, `RiveSprite2D`/`RiveControl` nodes play artboards with state machines, inputs are inspector properties, Rive events are signals, and pointer listeners receive mouse input. See [`GOALS.md`](GOALS.md) for the roadmap.

```gdscript
var file: RiveFileResource = load("res://ui/menu.riv")   # no import step
$RiveControl.file = file
$RiveControl.set_bool_input("muted", true)
$RiveControl.rive_event.connect(func(name, props): print(name, props))
```

## Documentation

- [`docs/usage.md`](docs/usage.md) — **how to use the extension** (install, nodes, inputs, events, building)
- [`GOALS.md`](GOALS.md) — goals, non-goals, success criteria, phased delivery
- [`docs/implementation-strategy.md`](docs/implementation-strategy.md) — architecture & evidence
- [`docs/ux-design.md`](docs/ux-design.md) — the intended developer experience
- [`docs/development-and-testing.md`](docs/development-and-testing.md) — repo layout, test tiers, CI (GitHub Actions + GitLab CI)

## Building (Linux, Phase 0)

```sh
git clone --recurse-submodules <repo>
cd rivegd

# stage 1: rive static libs (premake5/ninja, gcc)
tools/build_rive.sh

# stage 2: the GDExtension (SCons; use a venv if scons isn't installed)
python3 -m venv .venv && .venv/bin/pip install scons
.venv/bin/scons

# tests
tools/test.sh all
```

## License

MIT — see [`LICENSE`](LICENSE). rive-runtime is © Rive, MIT-licensed.
