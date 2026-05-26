#!/usr/bin/env bash
set -euo pipefail

missing=0

check_tool() {
    local tool="$1"
    local hint="$2"

    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[missing] $tool - $hint"
        missing=1
    else
        echo "[ok] $tool"
    fi
}

echo "Checking core build and test dependencies..."
check_tool cmake "Install CMake (e.g. brew install cmake)"
check_tool ctest "Usually installed with CMake"
check_tool cc "Install a C compiler toolchain"

echo
echo "Checking formatting and lint dependencies..."

if command -v clang-format >/dev/null 2>&1 || \
   command -v clang-format-18 >/dev/null 2>&1 || \
   command -v clang-format-17 >/dev/null 2>&1 || \
   command -v clang-format-16 >/dev/null 2>&1; then
    echo "[ok] clang-format (or versioned variant)"
else
    echo "[missing] clang-format - Install LLVM clang-format (e.g. brew install llvm)"
    missing=1
fi

if command -v clang-tidy >/dev/null 2>&1 || \
   command -v clang-tidy-18 >/dev/null 2>&1 || \
   command -v clang-tidy-17 >/dev/null 2>&1 || \
   command -v clang-tidy-16 >/dev/null 2>&1; then
    echo "[ok] clang-tidy (or versioned variant)"
else
    echo "[missing] clang-tidy - Install LLVM clang-tidy (e.g. brew install llvm)"
    missing=1
fi

echo
echo "Checking host demo dependencies..."
check_tool python3 "Install Python 3"

if python3 -m pip --version >/dev/null 2>&1; then
    echo "[ok] python3 -m pip"
else
    echo "[missing] python3 pip module - Install pip for Python 3"
    missing=1
fi

if [[ "$missing" -ne 0 ]]; then
    echo
    echo "One or more dependencies are missing."
    exit 1
fi

echo
echo "All required tools were found."