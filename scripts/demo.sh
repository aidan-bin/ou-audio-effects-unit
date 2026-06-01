#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

source "$SCRIPT_DIR/lib/cmake.sh"
source "$SCRIPT_DIR/lib/tooling.sh"
source "$SCRIPT_DIR/lib/dsp_files.sh"
source "$SCRIPT_DIR/lib/workflows.sh"

if [[ $# -gt 0 ]]; then
	echo "Usage: ./scripts/demo.sh"
	exit 1
fi

run_demo
