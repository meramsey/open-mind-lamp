#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
activate_idf
idf.py -C "$ROOT/firmware" build
idf.py -C "$ROOT/firmware" size
