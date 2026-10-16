#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
mkdir -p "$ROOT/.tools/host-tests"
cmake -S "$ROOT/firmware/test" -B "$ROOT/.tools/host-tests" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$ROOT/.tools/host-tests"
ctest --test-dir "$ROOT/.tools/host-tests" --output-on-failure
python3 -m unittest discover -s "$ROOT/analysis/tests" -v
if command -v shellcheck >/dev/null; then shellcheck -x -P "$ROOT/tools" "$ROOT"/tools/*.sh; fi
