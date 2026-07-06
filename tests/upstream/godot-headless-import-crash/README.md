# Upstream repro: Godot 4.7 headless `--import` aborts with any class-registering GDExtension

**Status:** affects rivegd CI (worked around); candidate for an upstream godotengine/godot (or godot-cpp) report.

## Symptom

`godot --headless --path <project> --import` exits with SIGABRT **at shutdown**
(after `EditorSettings: Save OK!`) whenever the project contains a GDExtension
that registers at least one class. The import itself completes; only the exit
crashes. Subsequent runs with a warm `.godot/` may pass, so it looks flaky in
CI — it isn't; wipe `.godot/` to reproduce deterministically.

## Isolation (Godot 4.7-stable official Linux x86_64, godot-cpp @ ba0edfe)

| Extension | fresh `--import` result |
|---|---|
| none | 0/4 crash |
| loads, registers **no** classes | 0/3 crash |
| registers one trivial `Resource` subclass (`class_ext.cpp` here) | 3/3 crash |
| full rivegd extension | 4/4 crash |

`reloadable` true/false makes no difference. rive-runtime linkage makes no
difference — `class_ext.cpp` links only godot-cpp.

## Repro

```sh
g++ -std=c++17 -fPIC -shared \
    -I<godot-cpp>/include -I<godot-cpp>/gen/include -I<godot-cpp>/gdextension \
    class_ext.cpp <godot-cpp>/bin/libgodot-cpp.linux.template_debug.x86_64.a \
    -o libnull_ext.so
# place in a minimal project with a .gdextension (entry_symbol null_ext_init),
# then:
rm -rf project/.godot
godot --headless --path project --import   # SIGABRT at exit
```

## Workaround

CI treats the `--import` step as best-effort (`|| true`); the subsequent
`--script` smoke run is unaffected and is the actual gate.
