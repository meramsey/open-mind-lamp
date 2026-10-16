#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
"$ROOT/tools/build.sh"
"$ROOT/tools/test.sh"
activate_idf
python "$ROOT/tools/device.py" --probe
PORT="$(detect_port)"
IDENTITY="$(python "$ROOT/tools/device.py" --identity-only)"
read -r -p 'Verify silkscreen. Type exact board model (ESP32-S3-Matrix): ' MODEL
[[ "$MODEL" == ESP32-S3-Matrix ]] || { printf 'Board not confirmed.\n' >&2; exit 1; }
read -r -p 'Enter physical PCB revision or "unmarked" after visual inspection: ' REVISION
[[ -n "$REVISION" ]] || exit 1
"$ROOT/tools/backup-firmware.sh"
read -r -p "Overwrite firmware on $PORT, PCB $REVISION? Type FLASH $PORT: " ANSWER
[[ "$ANSWER" == "FLASH $PORT" ]] || { printf 'Cancelled.\n'; exit 1; }
# Re-probe immediately before writing; do not trust a stale tty enumeration.
python "$ROOT/tools/device.py" --probe
[[ "$(detect_port)" == "$PORT" ]] || exit 1
[[ "$(python "$ROOT/tools/device.py" --identity-only)" == "$IDENTITY" ]] || exit 1
idf.py -C "$ROOT/firmware" -p "$PORT" flash
