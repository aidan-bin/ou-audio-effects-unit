#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found"
    exit 1
fi

clang-format --dry-run --Werror \
    dsp/effects/effects.c \
    dsp/effects/effects.h \
    dsp/math/fast_math.c \
    dsp/math/fast_math.h
