#!/usr/bin/env bash
set -euo pipefail

rm -rf build
rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib
find . -type d -name __pycache__ -prune -exec rm -rf {} +
find . -type f -name '*.pyc' -delete
