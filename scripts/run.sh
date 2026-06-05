#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

source "$SCRIPT_DIR/lib/cmake.sh"
source "$SCRIPT_DIR/lib/tooling.sh"
source "$SCRIPT_DIR/lib/dsp_files.sh"
source "$SCRIPT_DIR/lib/workflows.sh"

usage() {
	cat <<'EOF'
Usage: ./scripts/run.sh [command]

Commands:
  build
	build-demo
	build-firmware
	build-tests
  check
  check-full
  clean
  demo
  doctor
  format
  format-check
  lint
  test
EOF
}

if [[ $# -gt 1 ]]; then
	usage
	exit 1
fi

command_name="${1:-check}"

case "$command_name" in
	build) run_build ;;
	build-demo) run_build_demo ;;
	build-firmware) run_build_firmware ;;
	build-tests) run_build_tests ;;
	check) run_check ;;
	check-full) run_check_full ;;
	clean) run_clean ;;
	demo) run_demo ;;
	doctor) run_doctor ;;
	format) run_format ;;
	format-check) run_format_check ;;
	lint) run_lint ;;
	test) run_test ;;
	-h|--help|help) usage ;;
	*) usage; exit 1 ;;
esac