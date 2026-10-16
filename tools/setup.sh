#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
if [[ "${1:-}" != --skip-system ]]; then
    sudo apt-get update
    sudo apt-get install -y git wget curl flex bison gperf python3 python3-pip python3-venv \
        cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 usbutils \
        build-essential shellcheck
fi
mkdir -p "$ROOT/.tools"
if [[ ! -d "$IDF_DIR/.git" ]]; then
    git clone --branch v6.1 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git "$IDF_DIR"
fi
[[ "$(git -C "$IDF_DIR" rev-parse HEAD)" == "$IDF_COMMIT" ]] || { printf 'Unexpected IDF revision; preserving checkout.\n' >&2; exit 1; }
"$IDF_DIR/install.sh" esp32s3
activate_idf
idf.py --version
python "$ROOT/tools/device.py" --allow-missing
printf '\nSetup ready. Next: ./tools/test.sh and ./tools/build.sh\n'
printf 'Browser checks: ./tools/test-web.sh (isolated Python environment; downloads test browsers).\n'
