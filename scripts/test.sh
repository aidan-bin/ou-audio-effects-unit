#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/cmake.sh"

ensure_release_build_tree

cmake --build build -j
ctest --test-dir build --output-on-failure
