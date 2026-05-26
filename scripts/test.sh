#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f build/CMakeCache.txt ]]; then
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi

cmake --build build -j
ctest --test-dir build --output-on-failure
