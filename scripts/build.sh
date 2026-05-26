#!/usr/bin/env bash
set -euo pipefail

# Keep only the current platform's freshly built host library artifact.
rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
