#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
activate_idf
python "$ROOT/tools/device.py" --probe
PORT="$(detect_port)"
umask 077
mkdir -p "$ROOT/backups"
DEST="$ROOT/backups/flash-$(date -u +%Y%m%dT%H%M%SZ).bin"
[[ ! -e "$DEST" ]] || { printf 'Backup already exists; try again in a second.\n' >&2; exit 1; }
python -m esptool --chip esp32s3 --port "$PORT" read-flash 0 0x400000 "$DEST"
sha256sum "$DEST" > "$DEST.sha256"
printf 'Full 4 MB flash backup: %s\nKeep private: it may contain credentials. eFuses are not backed up.\n' "$DEST"
