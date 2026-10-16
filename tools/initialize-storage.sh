#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
activate_idf
python "$ROOT/tools/device.py" --probe
PORT="$(detect_port)"
read -r -p 'Erase ONLY Open Mind Lamp session partition at 0x300000? Type ERASE SESSION STORAGE: ' ANSWER
[[ "$ANSWER" == 'ERASE SESSION STORAGE' ]] || exit 1
"$ROOT/tools/backup-firmware.sh"
python -m esptool --chip esp32s3 --port "$PORT" erase-region 0x300000 0x100000
printf 'Session area erased. Open Mind Lamp will initialize it on reboot.\n'
