#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
export PLAYWRIGHT_BROWSERS_PATH="$ROOT/.tools/browsers"
python3 -m venv "$ROOT/.tools/web-venv"
"$ROOT/.tools/web-venv/bin/python" -m pip install -r "$ROOT/web/tests/requirements.txt"
"$ROOT/.tools/web-venv/bin/python" -m playwright install chromium firefox
"$ROOT/.tools/web-venv/bin/python" -m unittest discover -s "$ROOT/web/tests" -v
