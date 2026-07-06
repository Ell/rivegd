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

# Stage-1 output. build_rive.sh puts release builds in out/release.
rive_out = ARGUMENTS.get("rive_out", "thirdparty/rive-runtime/renderer/out/release")
if not os.path.isdir(rive_out):
    print("ERROR: rive static libs not found at '%s'." % rive_out)
    print("Run tools/build_rive.sh first (see docs/development-and-testing.md).")
    Exit(1)

env.Append(CPPPATH=["thirdparty/rive-runtime/renderer/include"])
# rive's vulkan headers are guarded by RIVE_VULKAN (set by its premake
# --with_vulkan build; we must match).
env.Append(CPPDEFINES=["RIVE_VULKAN"])
# Link order matters: pls renderer first (uses rive + decoders), then rive,
# then the (symbol-renamed) vendored deps. Explicit File nodes — SCons strips
# the "lib" prefix from LIBS names, which turned liblibpng.a into the SYSTEM
# -lpng (unrenamed symbols → dlopen failure).
env.Append(
    LIBS=[
        File(os.path.join(rive_out, "lib%s.a" % name))
        for name in [
            "rive_pls_renderer", "rive_decoders", "rive", "rive_harfbuzz",
            "rive_sheenbidi", "rive_yoga", "libpng", "libjpeg", "libwebp",
            "zlib",
        ]
    ]
)

# core/ and render/ (and rive's NoOpFactory) derive from rive types, which
# are built rtti-off; compile them the same way. godot/ keeps godot-cpp's
# defaults — it never includes rive renderer headers directly.
core_env = env.Clone()
core_env.Append(CXXFLAGS=["-fno-rtti"])
sources = (
    [core_env.SharedObject(f) for f in Glob("src/core/*.cpp")]
    + [core_env.SharedObject(f) for f in Glob("src/render/*/*.cpp")]
    + [core_env.SharedObject("thirdparty/rive-runtime/utils/no_op_factory.cpp")]
    + Glob("src/godot/*.cpp")
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
