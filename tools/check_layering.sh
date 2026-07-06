#!/usr/bin/env bash
# Layering lint: src/core and src/render must never include godot-cpp.
# (docs/development-and-testing.md §1)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
violations=0

for dir in "$ROOT/src/core" "$ROOT/src/render"; do
    [ -d "$dir" ] || continue
    if grep -rn --include='*.hpp' --include='*.cpp' --include='*.h' \
        -e '#include.*godot_cpp' -e '#include.*gdextension_interface' "$dir"; then
        violations=1
    fi
done

if [ "$violations" -ne 0 ]; then
    echo "LAYERING VIOLATION: core/ and render/ must be Godot-free." >&2
    exit 1
fi
echo "layering: OK"
