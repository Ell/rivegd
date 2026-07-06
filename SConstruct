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

env.Append(LIBPATH=[rive_out])
env.Append(LIBS=["rive", "rive_harfbuzz", "rive_sheenbidi", "rive_yoga"])

# core/ (and rive's NoOpFactory) derive from rive types, which are built
# rtti-off; compile them the same way. godot/ keeps godot-cpp's defaults.
core_env = env.Clone()
core_env.Append(CXXFLAGS=["-fno-rtti"])
sources = (
    [core_env.SharedObject(f) for f in Glob("src/core/*.cpp")]
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
