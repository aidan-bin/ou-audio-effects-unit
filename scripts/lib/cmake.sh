#!/usr/bin/env bash

ensure_release_build_tree() {
    local needs_configure=0
    local cache_file="build/CMakeCache.txt"
    local cmake_file="CMakeLists.txt"

    if [[ ! -f "$cache_file" ]]; then
        needs_configure=1
    elif ! grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "$cache_file"; then
        needs_configure=1
    elif [[ "$cmake_file" -nt "$cache_file" ]]; then
        needs_configure=1
    fi

    if [[ "$needs_configure" -eq 1 ]]; then
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    fi
}