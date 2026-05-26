#!/usr/bin/env bash
set -euo pipefail

if [[ "${SKIP_DOCTOR:-0}" != "1" ]]; then
	./scripts/doctor.sh
fi

./scripts/test.sh
