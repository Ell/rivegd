# rivegd

Rive integration for Godot 4.7+ — plays `.riv` files through Rive's
official runtime and GPU renderer.

Install: copy this `rive/` folder into your project's `addons/` directory
and open the project. The extension registers `RiveFileResource`,
`RiveSprite2D`, `RiveControl`, `RiveTexture`, and `RiveAudioStream`.

Requires the Forward+ or Mobile rendering method (Vulkan) on desktop, or a
Web export. Prebuilt binaries in `bin/` cover Linux x86_64 and web
(wasm32); other platforms build from source — see
https://github.com/Ell/rivegd for the usage guide and build instructions.
