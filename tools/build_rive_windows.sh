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
# Absolute compiler paths, preferring the system clang: the shim dir
# leads PATH (a bare "clang" would resolve to the shim itself and re-exec
# forever), and PATH may otherwise resolve to bundled toolchains (Swift's
# clang 17 clashes with modern mingw headers over __cpuidex).
CLANG_BIN="${RIVEGD_CLANG:-$([ -x /usr/bin/clang ] && echo /usr/bin/clang || command -v clang)}"
CLANGXX_BIN="${RIVEGD_CLANGXX:-$([ -x /usr/bin/clang++ ] && echo /usr/bin/clang++ || command -v clang++)}"
# Distros shipping both mingw thread models (Ubuntu) need clang pinned to
# the posix GCC install, or its libstdc++ headers inline win32-model
# gthread calls (__gthr_win32_*) the posix runtime can't satisfy at link.
GCC_POSIX_DIR="$(ls -d /usr/lib/gcc/x86_64-w64-mingw32/*-posix 2>/dev/null | sort -V | tail -1 || true)"
EXTRA_C=""
EXTRA_CXX=""
if [ -n "$GCC_POSIX_DIR" ]; then
    # Force the posix flavor outright: pin the GCC install for libgcc and
    # replace the default C++ header search with the posix set
    # (clang's install autodetection can otherwise pick the win32 model,
    # whose gthreads are unresolvable at final link).
    EXTRA_C="--gcc-install-dir=$GCC_POSIX_DIR"
    EXTRA_CXX="$EXTRA_C -nostdinc++ -isystem $GCC_POSIX_DIR/include/c++ -isystem $GCC_POSIX_DIR/include/c++/x86_64-w64-mingw32 -isystem $GCC_POSIX_DIR/include/c++/backward"
fi
printf '#!/bin/sh\nexec %s --target=x86_64-w64-mingw32 %s -Qunused-arguments -fuse-ld=lld "$@"\n' "$CLANG_BIN" "$EXTRA_C" > "$SHIM_DIR/clang"
printf '#!/bin/sh\nexec %s --target=x86_64-w64-mingw32 %s -Qunused-arguments -fuse-ld=lld "$@"\n' "$CLANGXX_BIN" "$EXTRA_CXX" > "$SHIM_DIR/clang++"
chmod +x "$SHIM_DIR/clang" "$SHIM_DIR/clang++"
# Report (and gate on) the libstdc++ flavor the shim actually resolves.
echo "mingw GCC posix dir: ${GCC_POSIX_DIR:-<single-flavor distro>}"
FLAVOR="$(echo '#include <mutex>' | "$SHIM_DIR/clang++" -x c++ - -E 2>/dev/null | grep -m1 -oE '/usr/lib/gcc/x86_64-w64-mingw32/[^/]+' || true)"
echo "libstdc++ headers from: ${FLAVOR:-<system default>}"
case "$FLAVOR" in
    *-win32)
        echo "ERROR: clang still resolves win32-thread-model mingw headers" >&2
        exit 1
        ;;
esac
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
