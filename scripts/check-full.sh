#!/usr/bin/env bash
set -euo pipefail

./scripts/doctor.sh
./scripts/format-check.sh
./scripts/lint.sh
SKIP_DOCTOR=1 ./scripts/check.sh
