#!/usr/bin/env bash
# Sourced by project tools; no shell startup changes.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
IDF_DIR="${OML_IDF_PATH:-$ROOT/.tools/esp-idf}"
activate_idf() {
    if [[ ! -f "$IDF_DIR/export.sh" ]]; then
        printf 'Toolchain missing. Run %s/tools/setup.sh\n' "$ROOT" >&2; exit 1
    fi
    if [[ "$(git -C "$IDF_DIR" rev-parse HEAD)" != "$IDF_COMMIT" ]]; then
        printf 'ESP-IDF must be pinned to v6.1 (%s). Set OML_IDF_PATH to that checkout.\n' "$IDF_COMMIT" >&2; exit 1
    fi
    # ESP-IDF export scripts are upstream shell code and may access unset variables.
    set +u
    # shellcheck source=/dev/null
    source "$IDF_DIR/export.sh" >/dev/null
    set -u
}
detect_port() { python "$ROOT/tools/device.py" --port-only; }
