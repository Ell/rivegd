#!/usr/bin/env bash
# Test entry point (docs/development-and-testing.md).
#   tools/test.sh unit      # Tier 0: C++ unit tests (no Godot, no GPU)
#   tools/test.sh lint      # layering check
#   tools/test.sh all
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RIVE="$ROOT/thirdparty/rive-runtime"
RIVE_OUT="${RIVE_OUT:-$RIVE/renderer/out/release}"
MODE="${1:-all}"

run_lint() {
    "$ROOT/tools/check_layering.sh"
}

run_unit() {
    mkdir -p "$ROOT/out/tests"
    # TUs that derive from rive types compile with -fno-rtti to match the
    # rive libs (built rtti-off; RTTI-on TUs would reference missing
    # typeinfo). The Catch2 test TU keeps default flags (needs exceptions).
    g++ -std=c++17 -g -O1 -fno-rtti -c \
        -I"$ROOT/src" -I"$RIVE/include" \
        "$ROOT/src/core/riv_file.cpp" -o "$ROOT/out/tests/riv_file.o"
    g++ -std=c++17 -g -O1 -fno-rtti -c \
        -I"$RIVE/include" \
        "$RIVE/utils/no_op_factory.cpp" -o "$ROOT/out/tests/no_op_factory.o"
    g++ -std=c++17 -g -O1 \
        -I"$ROOT/src" \
        -I"$RIVE/include" \
        -I"$RIVE/tests/include" \
        "$ROOT/tests/unit/riv_file_test.cpp" \
        "$ROOT/out/tests/riv_file.o" \
        "$ROOT/out/tests/no_op_factory.o" \
        -L"$RIVE_OUT" -lrive -lrive_harfbuzz -lrive_sheenbidi -lrive_yoga -lminiaudio \
        -o "$ROOT/out/tests/unit_tests"
    (cd "$ROOT" && ./out/tests/unit_tests)
}

case "$MODE" in
    lint) run_lint ;;
    unit) run_unit ;;
    all)
        run_lint
        run_unit
        ;;
    *)
        echo "usage: tools/test.sh [unit|lint|all]" >&2
        exit 2
        ;;
esac
