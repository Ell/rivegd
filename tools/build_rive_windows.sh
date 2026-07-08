#!/usr/bin/env bash
# Stage-1 Windows cross-build (from Linux, posix-thread mingw model): rive static libs as x86_64
# COFF via clang --target=x86_64-w64-mingw32 (+lld). Vulkan-only — Godot
# on Windows runs Vulkan for Forward+/Mobile, so the D3D backends aren't
# needed (and their shader step needs fxc, a Windows-only tool).
#
# Two shims + one transient patch make rive's stock pipeline cross-build:
# - clang/clang++ wrappers add the mingw target triple (premake-ninja
#   bakes compiler names, so PATH shims beat editing generated files);
# - an fxc stub satisfies the unconditional d3d shader-gen rule with
#   empty headers nothing includes;
# - premake5_pls_renderer.lua gets a `no_d3d` option so src/d3d* stays
#   out of the build; the patch is REVERTED after the build (submodule
#   stays pristine).
set -euo pipefail
cd "$(dirname "$0")/../thirdparty/rive-runtime/renderer"
CONFIG="${1:-release}"

command -v x86_64-w64-mingw32-g++ >/dev/null || {
    echo "mingw-w64 toolchain required (headers/crt for the clang target)"; exit 1; }
command -v clang++ >/dev/null || { echo "clang required"; exit 1; }

SHIM_DIR="$(mktemp -d)"
trap 'rm -rf "$SHIM_DIR"; git checkout -q -- premake5_pls_renderer.lua' EXIT
# Absolute compiler paths: the shim dir leads PATH, so a bare "clang"
# here would resolve to the shim itself and re-exec forever.
CLANG_BIN="$(command -v clang)"
CLANGXX_BIN="$(command -v clang++)"
printf '#!/bin/sh\nexec %s --target=x86_64-w64-mingw32 -fuse-ld=lld "$@"\n' "$CLANG_BIN" > "$SHIM_DIR/clang"
printf '#!/bin/sh\nexec %s --target=x86_64-w64-mingw32 -fuse-ld=lld "$@"\n' "$CLANGXX_BIN" > "$SHIM_DIR/clang++"
cat > "$SHIM_DIR/fxc" <<'EOF'
#!/usr/bin/env bash
out=""; prev=""
for a in "$@"; do
  if [ "$prev" = "/Fh" ]; then out="$a"; break; fi
  prev="$a"
done
[ -n "$out" ] && printf '// d3d shaders not built (vulkan-only cross build)\n' > "$out"
exit 0
EOF
chmod +x "$SHIM_DIR"/clang "$SHIM_DIR"/clang++ "$SHIM_DIR"/fxc

python3 - <<'EOF'
c = open("premake5_pls_renderer.lua").read()
c = c.replace("""filter({ 'system:windows', 'options:with_rive_canvas', 'options:not for_unreal' })
    do
        files({ 'src/ore/d3d11/*.cpp' })
        files({ 'src/ore/d3d12/*.cpp' })
    end""",
"""filter({ 'system:windows', 'options:with_rive_canvas', 'options:not for_unreal', 'options:not no_d3d' })
    do
        files({ 'src/ore/d3d11/*.cpp' })
        files({ 'src/ore/d3d12/*.cpp' })
    end""")
c = c.replace("""filter({'system:windows', 'options:not for_unreal'})
    do
        files({ 'src/d3d/*.cpp' })
        files({ 'src/d3d11/*.cpp' })
        files({ 'src/d3d12/*.cpp' })
    end""",
"""newoption({ trigger = 'no_d3d', description = 'skip the D3D backends' })
    filter({'system:windows', 'options:not for_unreal', 'options:not no_d3d'})
    do
        files({ 'src/d3d/*.cpp' })
        files({ 'src/d3d11/*.cpp' })
        files({ 'src/d3d12/*.cpp' })
    end""")
open("premake5_pls_renderer.lua", "w").write(c)
EOF

PATH="$SHIM_DIR:$PATH" RIVE_OUT="out/windows_$CONFIG" \
    ../build/build_rive.sh ninja "$CONFIG" --os=windows --no_d3d \
    --no-lto --with-pic --with_vulkan \
    --with_rive_audio=external --with_rive_scripting -- \
    rive rive_pls_renderer rive_decoders rive_harfbuzz rive_sheenbidi \
    rive_yoga miniaudio luau_vm libpng libjpeg libwebp zlib
