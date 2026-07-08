#!/usr/bin/env python
"""
Stage-2 build: compiles src/ into the GDExtension shared library and links
the stage-1 rive static libraries (built by tools/build_rive.sh).

  scons                       # template_debug for the host platform
  scons target=template_release
  scons dev_build=yes         # debug symbols + assertions, editor dev loop
"""
import os

env = SConscript("thirdparty/godot-cpp/SConstruct")

env.Append(CPPPATH=["src", "thirdparty/rive-runtime/include"])
if env["platform"] == "windows":
    # MinGW has no Vulkan headers; use the set rive's stage 1 vendored.
    env.Append(CPPPATH=[
        "thirdparty/rive-runtime/renderer/dependencies/"
        "KhronosGroup_Vulkan-Headers_vulkan-sdk-1.4.321/include"
    ])

is_web = env["platform"] == "web"
is_windows = env["platform"] == "windows"

# Stage-1 output. build_rive.sh puts release builds in out/release and
# wasm builds in out/wasm_release (tools/build_rive_web.sh).
rive_out = ARGUMENTS.get(
    "rive_out",
    "thirdparty/rive-runtime/renderer/out/wasm_release"
    if is_web
    else "thirdparty/rive-runtime/renderer/out/windows_release"
    if is_windows
    else "thirdparty/rive-runtime/renderer/out/release",
)
if not os.path.isdir(rive_out):
    print("ERROR: rive static libs not found at '%s'." % rive_out)
    print("Run tools/build_rive.sh first (see docs/development-and-testing.md).")
    Exit(1)

env.Append(
    CPPPATH=[
        "thirdparty/rive-runtime/renderer/include",
        # glad GL loader headers (GL bridge / Compatibility backend)
        "thirdparty/rive-runtime/renderer/glad/include",
        "thirdparty/rive-runtime/renderer/glad",
    ]
)
# Backend defines must match the stage-1 build: desktop pairs Vulkan with
# the glad desktop-GL loader (RIVE_VULKAN + RIVE_DESKTOP_GL); web is
# WebGL2 (RIVE_WEBGL — rive's premake sets it for system:emscripten).
env.Append(
    CPPDEFINES=(
        ["RIVE_WEBGL"]
        if is_web
        else ["RIVE_VULKAN"]  # windows: no GL (glad loader is posix-only)
        if is_windows
        else ["RIVE_VULKAN", "RIVE_DESKTOP_GL"]
    )
    + [
        # External audio engine (must match stage 1's
        # --with_rive_audio=external).
        "WITH_RIVE_AUDIO",
        "EXTERNAL_RIVE_AUDIO_ENGINE",
        "MA_NO_DEVICE_IO",
        "MA_NO_RESOURCE_MANAGER",
        # Luau scripting (must match stage 1's --with_rive_scripting).
        "WITH_RIVE_SCRIPTING",
    ]
)
# Link order matters: pls renderer first (uses rive + decoders), then rive,
# then the (symbol-renamed) vendored deps. Explicit File nodes — SCons strips
# the "lib" prefix from LIBS names, which turned liblibpng.a into the SYSTEM
# -lpng (unrenamed symbols → dlopen failure).
# librive and librive_pls_renderer reference each other (scripting's GPU
# canvas hooks) — group-link so ld rescans the archives.
rive_libs = " ".join(
    os.path.join(rive_out, ("%s.lib" if is_windows else "lib%s.a") % name)
    for name in [
        "rive_pls_renderer", "rive_decoders", "rive", "rive_harfbuzz",
        "rive_sheenbidi", "rive_yoga", "miniaudio", "luau_vm",
        "libpng", "libjpeg", "libwebp", "zlib",
    ]
)
env.Append(_LIBFLAGS=" -Wl,--start-group %s -Wl,--end-group" % rive_libs)

if is_web:
    # Self-contained longjmp (libpng error handling): wasm-EH-based, no JS
    # runtime support needed — Godot's engine module doesn't export
    # emscripten_longjmp, so the default mode fails at dlink load. Must
    # match stage 1 (tools/build_rive_web.sh sets EMCC_CFLAGS) and be set
    # BEFORE core_env clones this environment.
    env.Append(CCFLAGS=["-sSUPPORT_LONGJMP=wasm"])
    env.Append(LINKFLAGS=["-sSUPPORT_LONGJMP=wasm"])

# core/ and render/ (and rive's NoOpFactory) derive from rive types, which
# are built rtti-off; compile them the same way. godot/ keeps godot-cpp's
# defaults — it never includes rive renderer headers directly.
core_env = env.Clone()
core_env.Append(CXXFLAGS=["-fno-rtti"])
# In-editor class reference (F1 help), compiled into the library.
if env["target"] in ["editor", "template_debug"]:
    doc_data = env.GodotCPPDocData(
        "src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml")
    )

# Web has no Vulkan; windows is Vulkan-only (no GL bridge).
render_sources = [
    f
    for f in Glob("src/render/*/*.cpp")
    if not (is_web and "vulkan" in str(f))
    and not (is_windows and os.sep + "gl" + os.sep in str(f))
]

sources = (
    ([doc_data] if env["target"] in ["editor", "template_debug"] else [])
    + [core_env.SharedObject(f) for f in Glob("src/core/*.cpp")]
    + [core_env.SharedObject(f) for f in render_sources]
    + [core_env.SharedObject("thirdparty/rive-runtime/utils/no_op_factory.cpp")]
    + Glob("src/godot/*.cpp")
    + Glob("src/godot/editor/*.cpp")
)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "addons/rive/bin/macos/librivegd{}.framework/librivegd{}".format(
            env["suffix"], env["suffix"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "addons/rive/bin/{}/librivegd{}{}".format(
            env["platform"], env["suffix"], env["SHLIBSUFFIX"]
        ),
        source=sources,
    )

Default(library)
