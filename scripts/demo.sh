#!/usr/bin/env bash
set -euo pipefail

./scripts/build.sh
python3 -m pip install -r host/python-demo/requirements.txt
python3 host/python-demo/main.py
