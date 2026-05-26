#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found"
    exit 1
fi

find dsp -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
