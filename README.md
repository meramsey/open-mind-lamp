# Open Mind Lamp

**An offline, open-source hardware-randomness lamp and auditable experimental notebook.**

Open Mind Lamp turns a Waveshare ESP32-S3-Matrix into a smooth, diffuse color source.
Its hue follows an unbiased random walk driven by the ESP32-S3 hardware RNG.
A lightweight browser interface runs directly on the lamp: no account, app, cloud,
subscription, iOS device, or Internet connection is needed for operation.

The optional experiment recorder supports exploratory intention-based hypotheses
and matched control conditions. Psychokinetic influence is **not scientifically
established**. This project makes the measurements and assumptions inspectable;
it does not interpret unusual results as evidence of a particular cause.

## Status: 0.2.1 hardware-validation candidate

Implemented and cross-compiled with pinned **ESP-IDF 6.1**. Host C++ invariants,
independent Python statistics/auditing, shell checks and real Chromium/Firefox
browser workflows are supplied. The 0.1.0 firmware has booted on the connected
board. The user subsequently deployed 0.2.0 and confirmed physical channel-order
calibration; that saved calibration remains authoritative. PCB revision remains
unknown. The 0.2.1 live-entropy diagnostics await authorized deployment and bounded
hardware validation. Thermal/power measurements and broader acceptance remain required before a
hardware-qualified release. See [validation](docs/validation.md).

## Features

- 64 LEDs as one ambient light; random walk, breathing, pixel, calibration,
  RNG activity and experiment status modes.
- Separate sampling, accumulation and animation clocks; bounded brightness.
- Hardware entropy only with active Wi-Fi and power saving disabled; latching
  basic health alarms. Test-only deterministic generator is never user-selectable.
- Unique WPA2 AP credentials and independent admin token, provisioned over USB.
  Optional home Wi-Fi, unique mDNS hostname and always-available recovery AP.
- Persistent settings; fixed-length baseline/control/intention trials; raw 32-bit
  word exports with timing, source, protocol, configuration and completion reason.
- Offline audit, binomial tails, Wilson intervals, CSV/raw conversion and
  exploratory control comparisons using Python's standard library.
- Responsive dark/light UI, entirely embedded in firmware.
- Final-command RGB snapshots shared by RMT and API, with target/smoothed hue
  labels, frame age and explicit optical limitations.
- Guided RGB/GRB identification and seven-color user confirmation at 2% PWM;
  persistent ordering and optional per-channel gains. [Color guide](docs/color-calibration.md).
- Read-only live RNG/window/render diagnostics and a bounded RAM trace with an
  independent causality verifier. [Entropy-to-LED validation](docs/rng-validation.md).

## Quick start (Xubuntu / Ubuntu 24.04)

From the repository directory:

```bash
./tools/setup.sh
./tools/test.sh
./tools/build.sh
./tools/flash.sh
./tools/monitor.sh
```

The flashing tool builds, tests, probes the chip, asks you to check the physical
model/revision, backs up the full flash, and requires explicit overwrite confirmation.
Upgrades preserve the unchanged NVS/session partitions; export a stopped trial
before deployment. Run the flash command only when you intend to authorize writing.

After flashing, reset with the USB monitor open. Copy the unique WPA2 password
and admin token from the `onboarding` messages. Connect to `OpenMindLamp-XXXXXX`,
open **http://192.168.4.1**, and enter the token. Android's “No Internet” message
is expected; choose to stay connected. The lamp continues when the browser closes.

[Full getting started guide](docs/getting-started.md) · [Hardware](docs/hardware.md) ·
[Architecture](docs/architecture.md) · [Experiments](docs/experiments.md) ·
[Statistics](docs/statistics.md) · [API](docs/api.md) · [Troubleshooting](docs/troubleshooting.md)

## Analyze a downloaded trial

```bash
python3 analysis/src/analyze.py trial.ndjson
python3 analysis/src/analyze.py trial.ndjson --csv samples.csv --raw samples.bin
python3 analysis/src/analyze.py control.ndjson intention.ndjson --compare
```

Each trial is capped at 4096 words (131072 bits), with one saved trial on-device.
Export and verify it before clearing. Interrupted trials are retained, never
silently resumed or overwritten. Normal standalone lamp activity does not write
samples to flash.

## Development and release

```bash
./tools/test-web.sh         # isolated venv + Chromium/Firefox downloads
./tools/release.sh          # local release bundle, checksums and license inventory
```

Build/test tools need Internet on first setup. Once installed and dependencies
cached, firmware builds and analysis can run offline. The lamp itself never
requires Internet. No host service or shell startup change is installed.

Original software: [MIT](LICENSE). ESP-IDF and its components retain their own
licenses, including Espressif's RF binary-library terms; see
[third-party notices](THIRD_PARTY_NOTICES.md). Waveshare hardware documentation
and demo source are referenced, not relicensed or copied into this repository.
