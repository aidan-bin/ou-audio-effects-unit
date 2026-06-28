#!/usr/bin/env bash

# Versioned suffixes to probe for clang tools, newest first.
CLANG_TOOL_VERSIONS=(20 19 18 17 16)

tool_candidates_string() {
    local tool="$1"
    local candidates="$tool"
    local version
    for version in "${CLANG_TOOL_VERSIONS[@]}"; do
        candidates+=", $tool-$version"
    done
    echo "$candidates"
}

find_versioned_tool() {
    local tool="$1"

    for candidate in "$tool" "${CLANG_TOOL_VERSIONS[@]/#/$tool-}"; do
        if command -v "$candidate" >/dev/null 2>&1; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}
</content>
