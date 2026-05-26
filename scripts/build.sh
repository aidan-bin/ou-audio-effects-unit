#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/cmake.sh"

# Keep only the current platform's freshly built host library artifact.
rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib

ensure_release_build_tree

cmake --build build -j
