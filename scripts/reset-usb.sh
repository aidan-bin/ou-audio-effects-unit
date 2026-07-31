#!/bin/bash
# Reset the STM32 device via OpenOCD to force USB re-enumeration.
# Works around macOS CDC ACM driver zombie after isochronous streaming.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

openocd -f "$REPO_ROOT/openocd.cfg" -c "init; reset run; shutdown" 2>&1

echo "Device reset. Wait ~5s for USB re-enumeration."
