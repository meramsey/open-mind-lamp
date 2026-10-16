#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "$0")/common.sh"
"$ROOT/tools/test.sh"
"$ROOT/tools/test-web.sh"
"$ROOT/tools/build.sh"
activate_idf
mkdir -p "$ROOT/release"
python -m pip freeze > "$ROOT/release/python-tools.txt"
python -m esp_idf_sbom create "$ROOT/firmware/build/project_description.json" \
    --no-sync-excluded-cves --file-tags --rem-unused --format spdx-json@2.2 \
    --output "$ROOT/release/sbom.spdx.json"
python "$ROOT/tools/package-release.py" "$IDF_DIR"
VERSION="$(<"$ROOT/VERSION")"
python "$ROOT/tools/verify-release.py" "$ROOT/release/open-mind-lamp-$VERSION-candidate.tar.gz"
