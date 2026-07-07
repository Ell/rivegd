# Draft: upstream godotengine/godot issue

> Review and file at https://github.com/godotengine/godot/issues/new/choose
> (Bug report). Repro code lives in `tests/upstream/godot-headless-import-crash/`.

---

**Title:** Headless `--import` aborts at exit when any GDExtension registers a class (4.7-stable)

## Tested versions

- Reproducible: v4.7.stable.official [5b4e0cb0f], Linux x86_64 (official download)

## System information

Arch Linux, x86_64 — issue occurs in `--headless` mode (also reproduced on
Ubuntu 24.04 GitHub Actions runners), so GPU/driver are not involved.

## Issue description

Running `godot --headless --path <project> --import` on a project containing
a GDExtension that registers **at least one class** makes the process abort
(SIGABRT) **at shutdown** — after `EditorSettings: Save OK!`. The import
itself completes and the `.godot` cache is written correctly; only the exit
crashes, which breaks CI pipelines that check the exit code.

Isolation (fresh `.godot/` before each run — with a warm cache the run can
pass, so the bug looks intermittent in CI but is deterministic on first
import):

| Project contents | fresh `--import` result |
|---|---|
| no extension | exit 0 (0/4 crash) |
| extension that loads but registers **no** classes | exit 0 (0/3 crash) |
| extension registering one empty `Resource` subclass | SIGABRT (3/3) |

`reloadable = true/false` makes no difference. The registered class is never
instantiated; registering it is sufficient.

Backtrace is opaque with the stripped official binary (abort inside the main
binary, after editor teardown begins); happy to re-run against a debug build
if useful.

## Steps to reproduce

1. Build the ~30-line GDExtension below against godot-cpp (godot-4.7-stable
   branch, `template_debug`):

```cpp
#include <gdextension_interface.h>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
using namespace godot;

class NullResource : public Resource {
    GDCLASS(NullResource, Resource)
protected:
    static void _bind_methods() {}
};

static void init_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(NullResource);
}
static void uninit_module(ModuleInitializationLevel) {}

extern "C" GDExtensionBool GDE_EXPORT null_ext_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library,
                                            r_initialization);
    init_obj.register_initializer(init_module);
    init_obj.register_terminator(uninit_module);
    init_obj.set_minimum_library_initialization_level(
        MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
```

2. Place it in a minimal project with a `.gdextension`
   (`entry_symbol = "null_ext_init"`, `compatibility_minimum = "4.7"`).
3. `rm -rf <project>/.godot`
4. `godot --headless --path <project> --import`
5. Process prints normal import output, then aborts at exit (exit code 134).

## Minimal reproduction project

(attach zip of the project + prebuilt .so, or link to
https://github.com/Ell/rivegd/tree/main/tests/upstream/godot-headless-import-crash)
