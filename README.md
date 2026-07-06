# rivegd

**Rive for Godot** — a GDExtension integrating [Rive](https://rive.app)'s official C++ runtime and GPU renderer into Godot 4.7+.

> Status: **Phase 0** (build skeleton + headless runtime proof). See [`GOALS.md`](GOALS.md) for the full goal set and phase plan.

## Documentation

- [`GOALS.md`](GOALS.md) — goals, non-goals, success criteria, phased delivery
- [`docs/implementation-strategy.md`](docs/implementation-strategy.md) — architecture & evidence
- [`docs/ux-design.md`](docs/ux-design.md) — the intended developer experience
- [`docs/development-and-testing.md`](docs/development-and-testing.md) — repo layout, test tiers, CI

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
