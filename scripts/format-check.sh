#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/tooling.sh"
source "$SCRIPT_DIR/lib/dsp_files.sh"

clang_format_bin="$(find_versioned_tool clang-format || true)"

if [[ -z "$clang_format_bin" ]]; then
    echo "clang-format not found (tried $(tool_candidates_string clang-format))"
    exit 1
fi

"$clang_format_bin" --dry-run --Werror "${DSP_FORMAT_FILES[@]}"
