#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/tooling.sh"

clang_tidy_bin="$(find_versioned_tool clang-tidy || true)"

if [[ -z "$clang_tidy_bin" ]]; then
    echo "clang-tidy not found (tried $(tool_candidates_string clang-tidy))"
    exit 1
fi

if [[ ! -f build/compile_commands.json ]]; then
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
fi

extra_args=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    sdk_path="$(xcrun --sdk macosx --show-sdk-path)"
    extra_args+=("--extra-arg-before=-isysroot${sdk_path}")
fi

run_tidy() {
    "$clang_tidy_bin" -quiet -p build -header-filter='^dsp/' "${extra_args[@]}" "$1" 2>&1 | \
        sed -E '/^[0-9]+ warnings generated\.$/d'
}

run_tidy dsp/effects/effects.c
run_tidy dsp/math/fast_math.c
