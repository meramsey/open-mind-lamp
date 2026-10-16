# Third-party licenses and provenance

Original project code and documentation are MIT, copyright Open Mind Lamp
contributors. Third-party material retains its own license; the project's MIT
license does not replace those terms. Original hardware remains Waveshare's design.

## Pinned runtime dependencies

| Dependency | Version / revision | License verified from checkout |
|---|---|---|
| ESP-IDF | 6.1, `fff9895c82d744c7237be8847347bdd1b07c6643` | Apache-2.0 root; individual components retain notices |
| Espressif led_strip | 3.0.3 | Apache-2.0 |
| Espressif mDNS | 1.11.0 | Apache-2.0 |
| cJSON | 1.7.19~2 | MIT; Dave Gamble and cJSON contributors |

Component content hashes are in `firmware/dependencies.lock`. ESP-IDF pins its
submodules and tool binaries. Relevant included upstream components include
FreeRTOS (MIT), lwIP (BSD-style), SPIFFS (MIT), mbedTLS/TF-PSA-Crypto (upstream
dual-license/Apache terms), and newlib/GCC runtime material with their individual
licenses and runtime exceptions. Do not infer a component's license from the
top-level SDK alone.

The selected SDK includes precompiled Wi-Fi/PHY/coexistence libraries. Their
checked-out `components/esp_wifi/lib/LICENSE` and `components/esp_phy/lib/LICENSE`
texts are Apache-2.0 in the pinned release; binary implementation availability
does not make the entire radio stack source-rebuildable. No claim of a completely
blob-free firmware/toolchain is made. No third-party source files are modified.

`./tools/release.sh` generates an SPDX SBOM from the actual linked build with SPDX
file tags and packages upstream `LICENSE*`, `LICENCE*`, `COPYING*`, `NOTICE*` and
`COPYRIGHT*` texts under source-relative paths. The bundle also includes original
source, lockfile, exact sdkconfig, ELF/map/binaries, a file hash inventory and this
notice. The inventory is intentionally broader than only linked code so individual
notices are retained. The SBOM records linked-component scope; it is not a legal
relicensing of unidentified third-party files.

The SDK SPDX scan reports an unrecognized custom `LicenseRef-PD-hp` alternative
in upstream mldsa-native **test-only** RNG/example files. Those files are not in
this application's compile commands or linked firmware. The upstream license
explicitly also offers CC0-1.0 / 0BSD / MIT-0 / MIT alternatives and identifies
the code as testing-only; its full `mldsa-native/LICENSE` text is retained in
the notice inventory. No test-only upstream RNG is used for lamp measurements.
This tool warning is recorded rather than silently rewriting third-party tags.

## Development-only dependencies

- Espressif's managed Python tools and compiler/tool binaries use their upstream
  licenses and pinned tool metadata; they are downloaded, not embedded in the UI.
- Playwright Python 1.55.0 (Apache-2.0), pyee 13.0.1 (MIT), greenlet 3.2.5
  (MIT/Python-derived terms), typing_extensions 4.16.0 (PSF terms). The web-test
  requirements pin these versions; browsers retain Chromium/Mozilla/codec licenses.
  Test browsers and Python environments are not redistributed in the release bundle.
- Ubuntu system packages remain managed by apt under their own licenses.

## Manufacturer resources

Waveshare documentation, schematic, demos and 3D model are linked as engineering
references. They have **not** been copied into or relicensed with project source.
Implementation is original; board constants and protocol facts are documented
with provenance. Retrieved official artifacts had these SHA-256 values:

```
ESP32-S3-Matrix-Sch.pdf
ae1e41577f0efc06ad1253d079f1088421cd70775bc050f3848fe3ce1c98f120
ESP32-S3-Matrix-Demo.zip
b2aa108af96a59f83b40f9d0cd2be21d67f5422e6b51e50093fd4a1c2dc25d9d
```

Hashes identify what was inspected, not a guarantee that future downloads are
unchanged or an authorization to redistribute manufacturer material.
