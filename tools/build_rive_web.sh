#!/usr/bin/env bash
# Stage-1 wasm build (web export): rive static libs compiled with
# Emscripten, matched to Godot 4.7's web templates.
#
# - Emscripten 4.0.20: what the official 4.7 templates are built with
#   (their runtime prints the version; Godot's CI workflow file lags).
# - SUPPORT_LONGJMP=wasm: libpng's setjmp error handling — Godot's engine
#   module only exports the wasm-EH longjmp variants, not
#   emscripten_longjmp, so the default mode fails at dlink load.
# - --no-wasm-simd: emscripten's dynamic linker lazily binds some symbols
#   through JS trampolines, and v128 (SIMD) parameters cannot cross them —
#   rive's IntersectionBoard::addRectangle(int4) faults at first flush.
#   Scalar lowering keeps every signature JS-safe.
#
# WebGL2 backend (RIVE_WEBGL is set by rive's premake for emscripten);
# no Vulkan. Same feature set as desktop otherwise.
set -euo pipefail
cd "$(dirname "$0")/../thirdparty/rive-runtime/renderer"
CONFIG="${1:-release}"
export RIVE_EMSDK_VERSION="${RIVE_EMSDK_VERSION:-4.0.20}"
export EMCC_CFLAGS="-sSUPPORT_LONGJMP=wasm"
exec ../build/build_rive.sh ninja "$CONFIG" wasm --no-lto --with-pic \
    --no-wasm-simd --with_rive_audio=external --with_rive_scripting -- \
    rive rive_pls_renderer rive_decoders rive_harfbuzz rive_sheenbidi \
    rive_yoga miniaudio luau_vm libpng libjpeg libwebp zlib
