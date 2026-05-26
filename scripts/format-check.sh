#!/usr/bin/env bash
set -euo pipefail

clang_format_bin=""
for candidate in clang-format clang-format-20 clang-format-19 clang-format-18 clang-format-17 clang-format-16; do
    if command -v "$candidate" >/dev/null 2>&1; then
        clang_format_bin="$candidate"
        break
    fi
done

if [[ -z "$clang_format_bin" ]]; then
    echo "clang-format not found (tried clang-format, clang-format-20, clang-format-19, clang-format-18, clang-format-17, clang-format-16)"
    exit 1
fi

"$clang_format_bin" --dry-run --Werror \
    dsp/effects/effects.c \
    dsp/effects/effects.h \
    dsp/math/fast_math.c \
    dsp/math/fast_math.h
