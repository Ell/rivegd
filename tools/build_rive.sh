#!/usr/bin/env bash
# Stage-1 build: rive-runtime static libraries via its premake5 build system.
#
#   tools/build_rive.sh              # host release build (gcc)
#   tools/build_rive.sh debug        # host debug build
#
# Phase 0 builds the base runtime only. Feature flags (--with_rive_text,
# --with_rive_layout, --with_rive_audio=external, --with_rive_scripting)
# land in Phase 2/4 — see GOALS.md G5 and docs/implementation-strategy.md §6.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${1:-release}"

# build_rive.sh must run from a directory containing a premake5.lua.
# renderer/ is the canonical entry point: it pulls in the core runtime,
# decoders, and the Rive Renderer. Output: renderer/out/<config>/.
# The script's default premake args already enable text + layout.
# Rive's build only supports clang (or msvc); any clang >= LLVM 11 works.
# On Linux the Itanium C++ ABI makes clang-built rive libs link cleanly
# against the gcc-built godot-cpp objects in stage 2.
# --no-lto: rive release builds default to -flto=full, which emits LLVM
# bitcode archives that gcc/GNU ld (stage 2) cannot link.
# Targets: the core runtime plus the (symbol-renamed) text/layout deps it
# references when built with the default --with_rive_text --with_rive_layout.
cd "$ROOT/thirdparty/rive-runtime/renderer"
# --with-pic: the archives get linked into a shared library (GDExtension).
# --with_vulkan: enables the Vulkan RenderContext backend (Phase 1).
# --with_rive_audio=external: miniaudio mixes but owns no OS device;
# Godot's audio server pulls PCM through RiveAudioStream.
# --with_rive_scripting: Luau VM for script-bearing .riv files (G5.4).
# Scripts are signature-verified against rive's PRODUCTION key; rive's
# sample-signed test fixtures will load but their scripts won't run.
exec ../build/build_rive.sh ninja "$CONFIG" --no-lto --with-pic --with_vulkan \
    --with_rive_audio=external --with_rive_scripting -- \
    rive rive_pls_renderer rive_decoders rive_harfbuzz rive_sheenbidi \
    rive_yoga miniaudio luau_vm libpng libjpeg libwebp zlib
