#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
activate_idf
PORT="$(detect_port)"
idf.py -C "$ROOT/firmware" -p "$PORT" monitor
