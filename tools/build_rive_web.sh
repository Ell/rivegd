#!/usr/bin/env bash
# Stage-1 wasm build (web export, GOALS G2.3 / Phase 4): rive static libs
# compiled with Emscripten, matched to Godot 4.7's web templates
# (Emscripten 4.0.20 — rive's script installs it under
# build/dependencies/emsdk_<ver>; stage 2 sources the same env).
#
# WebGL2 backend (RIVE_WEBGL is set by rive's premake for emscripten);
# no Vulkan. Same feature set as desktop otherwise.
set -euo pipefail
cd "$(dirname "$0")/../thirdparty/rive-runtime/renderer"
CONFIG="${1:-release}"
# SUPPORT_LONGJMP=wasm: self-contained longjmp (libpng/luau) — the default
# emscripten mode needs JS runtime support Godot's main module lacks.
RIVE_EMSDK_VERSION="${RIVE_EMSDK_VERSION:-4.0.20}" \
EMCC_CFLAGS="-sSUPPORT_LONGJMP=wasm" \
# --no-wasm-simd: emscripten dylink lazily binds some symbols through JS
# stubs, and v128 (SIMD) parameters cannot cross a JS trampoline — rive's
# IntersectionBoard::addRectangle(int4) faults at first call in a side
# module. Scalar lowering keeps every signature JS-safe.
exec ../build/build_rive.sh ninja "$CONFIG" wasm --no-lto --with-pic --no-wasm-simd \
    --with_rive_audio=external --with_rive_scripting -- \
    rive rive_pls_renderer rive_decoders rive_harfbuzz rive_sheenbidi \
    rive_yoga miniaudio luau_vm libpng libjpeg libwebp zlib
