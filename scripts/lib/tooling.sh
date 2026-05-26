#!/usr/bin/env bash

tool_candidates_string() {
    local tool="$1"
    echo "$tool, $tool-20, $tool-19, $tool-18, $tool-17, $tool-16"
}

find_versioned_tool() {
    local tool="$1"

    for candidate in "$tool" "$tool"-20 "$tool"-19 "$tool"-18 "$tool"-17 "$tool"-16; do
        if command -v "$candidate" >/dev/null 2>&1; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}