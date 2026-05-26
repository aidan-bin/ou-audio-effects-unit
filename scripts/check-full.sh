#!/usr/bin/env bash
set -euo pipefail

./scripts/format-check.sh
./scripts/lint.sh
./scripts/check.sh
