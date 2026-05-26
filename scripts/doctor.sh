#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/tooling.sh"

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

check_tool_or_versioned() {
    local tool="$1"
    local hint="$2"

    if find_versioned_tool "$tool" >/dev/null; then
        echo "[ok] $tool (or versioned variant)"
    else
        echo "[missing] $tool - $hint"
        missing=1
    fi
}

echo "Checking core build and test dependencies..."
check_tool cmake "Install CMake (e.g. brew install cmake)"
check_tool ctest "Usually installed with CMake"
check_tool cc "Install a C compiler toolchain"

echo
echo "Checking formatting and lint dependencies..."

check_tool_or_versioned clang-format "Install LLVM clang-format (e.g. brew install llvm)"
check_tool_or_versioned clang-tidy "Install LLVM clang-tidy (e.g. brew install llvm)"

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