#!/usr/bin/env bash
set -euo pipefail

# Keep only the current platform's freshly built host library artifact.
rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib

needs_configure=0

if [[ ! -f build/CMakeCache.txt ]]; then
	needs_configure=1
elif ! grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' build/CMakeCache.txt; then
	needs_configure=1
fi

if [[ "$needs_configure" -eq 1 ]]; then
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi

cmake --build build -j
