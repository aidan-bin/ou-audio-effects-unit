#!/usr/bin/env bash
set -euo pipefail

clang_tidy_bin=""
for candidate in clang-tidy clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy-17 clang-tidy-16; do
    if command -v "$candidate" >/dev/null 2>&1; then
        clang_tidy_bin="$candidate"
        break
    fi
done

if [[ -z "$clang_tidy_bin" ]]; then
    echo "clang-tidy not found (tried clang-tidy, clang-tidy-20, clang-tidy-19, clang-tidy-18, clang-tidy-17, clang-tidy-16)"
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
