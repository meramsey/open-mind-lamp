# Live hardware RNG → LED verification

## Verified production path

1. `networking/network.cpp`: `esp_wifi_start()` in APSTA mode, `WIFI_PS_NONE`,
   AP-start/stop events. The private AP stays enabled regardless of client count,
   Internet, STA association, STA retry or `network_recover()`.
2. `entropy/source.hpp`: `HardwareSource::read()` checks RF readiness before and
   after **each** `esp_random()` call. Failed checks return no accepted word.
   The real ESP-IDF implementation (`components/esp_hw_support/hw_random.c`)
   reads the hardware RNG, busy-waits for entropy replenishment and XOR-mixes
   further hardware reads (and the SDK's supported asynchronous timer source).
   Exported words are exact SDK returns, not raw ADC voltages/register snapshots.
3. `main.cpp`: successful read → `Health::observe()` → raw experiment recorder
   if a real trial is active → **healthy words only** → `Walk::sample()`.
4. `Walk::sample()`: `net = 2*popcount(word)-32`, added to the current accumulator.
   `Walk::commit()` alone changes the target to `wrap(before+sensitivity*net)`.
   Empty/zero-net windows do not move the target. A settings/trial reset is explicit.
5. `Walk::render()`: shortest-path exponential smoothing toward that target using
   elapsed frame time. It returns the actual before/target/dt/tau/after inputs.
6. `color_frame()` selects the active mode, HSV, one gamma transform, brightness,
   saved channel gains and conservative cap; `Matrix::show()` maps the saved
   RGB/GRB ordering and transmits through RMT DMA.
7. A successful refresh publishes `RenderedColor`; `/api/status` supplies that
   snapshot and RNG diagnostics to the existing 1500 ms dashboard poll. No browser
   timer supplies random events or advances the firmware's walk target.

**Three clocks:** sample rate schedules reads; accumulation interval schedules
commits; frame rate schedules smoothing/transmission. The random values determine
the signed hue displacement. Rendered hue may continue settling after a zero-net
window because an earlier target was different. Positive hue movement is called
clockwise here (red → yellow → green); this is a hue-wheel convention.

## Physical entropy contract and remaining assumptions

[Espressif's ESP32-S3 RNG contract](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/random.html)
requires an active RF entropy source or explicitly enabled SAR entropy. This
firmware uses always-active AP/APSTA RF, disables modem power saving and rejects
`CONFIG_PM_ENABLE` builds. There are no application sleep calls or ADC consumers.
Early boot secrets temporarily enable SAR entropy, then disable it **before** RF.
Fixed-pattern SDK bring-up RNG builds are also rejected at compile time.

Accepted reads check the AP event flag and read back Wi-Fi mode/PS before/after
the SDK call. A cached profile serves the 1 ms scheduler/LED gate; driver readback
runs at acquisition boundaries and status requests, not on every scheduler tick.
Rejected attempts have a cumulative counter, so a transient gate rejection between
HTTP polls is not silently treated as uninterrupted readiness. AP/STA/no-client
operation and station recovery use the same profile; recovery does not stop AP,
change Wi-Fi mode/PS, or enable SAR concurrently with RF.

These checks establish software configuration/causality **under the SDK/hardware
contract**, not an independent physical entropy measurement. Assumptions still
include correct RF/ADC hardware, SDK guarantees during radio channel changes and
retries, and event/readback timing. Software cannot prove that every internal noise
sample is fresh, observe RF internals continuously, measure optical output, or
prove unpredictability from a finite sample. Basic RNG health tests detect only
gross failures. Actual AP/STA/recovery captures with the new instrumentation have
not been run without authorized deployment. No time-driven hue-cycle defect was
found; prior readiness monitoring and absence of raw-window evidence were gaps.

## Short live procedure (no diagnostic settings writes)

1. After **explicitly authorizing** deployment, run the ordinary flash workflow.
   It creates a new backup and preserves the unchanged NVS/session partitions:

   ```bash
   cd /home/mike/Documents/dev/open-mind-lamp
   ./tools/test.sh
   ./tools/test-web.sh
   ./tools/build.sh
   OML_PORT=/dev/ttyACM0 ./tools/flash.sh
   ./tools/monitor.sh
   ```

   Verify firmware **0.2.1**. No flash/erase, credential reset, calibration reset,
   trial clear or factory-backup overwrite is performed by the development task.
   Your user-confirmed 0.2.0 RGB/GRB/gain settings remain authoritative.
2. Join the lamp AP (this board: `OpenMindLamp-408C15`), open http://192.168.4.1,
   and inspect **Live RNG → LED diagnostics**. Stop any active trial/calibration;
   **do not clear saved trials**. Use **Random Walk (mode 1)**, LEDs on, positive
   saturation/sensitivity, conservative brightness (e.g. 0.04), and preferably
   default 20 words/s, 1000 ms accumulation, 30 fps. Retain the saved order/gains.
   Wait two seconds after any explicit setting change; avoid changes during capture.
3. Collect 15 seconds, with at most one read-only GET per 0.5 s plus request time.
   Enter the existing admin token privately at the prompt (or use `OML_TOKEN`):

   ```bash
   python3 analysis/src/verify_live_rng.py --seconds 15 --output /tmp/oml-rng-ap.json
   python3 analysis/src/verify_live_rng.py --offline /tmp/oml-rng-ap.json
   ```

4. PASS requires gap-free successful samples and at least two complete windows
   with some nonzero movement. The verifier recomputes word popcounts/window sums,
   target updates, sampled-frame smoothing from actual inputs, the latest target
   available at each frame, HSV/gamma/gains/PWM, RGB/GRB bytes and uniform pixel mask.
   It rejects readiness/health failures, new rejected reads, reboots, setting resets,
   changed source, unsupported modes and incomplete evidence. An initial partial
   ring window is explicitly skipped. Repeated word values are not automatically
   faults. Quantization can leave RGB bytes unchanged for small hue changes.
5. If STA is already configured, repeat with it connected, using a fresh filename
   (`/tmp/oml-rng-sta.json`). Stay joined to the lamp AP while temporarily making
   the home router unavailable and collecting another 15-second trace to exercise
   STA retries. Restore the router afterward. Do not use Forget Wi-Fi/long BOOT
   merely to test: those intentionally clear saved STA credentials. Source audit
   covers that recovery path; an explicit physical recovery test is separate.

Capture bounds are 5–120 seconds (plus one outstanding HTTP timeout), a maximum
64 KiB response and a fixed 128-event device ring (~8 KiB). New settings drop old
trace history, not stored trials. If the ring overwrites unread events, retry at
`--interval 0.1` or with lower sample rate chosen explicitly; gaps are never waved
through as a proof. A slow 60-second accumulation setting needs a shorter interval
chosen explicitly to fit two windows within the bounded capture. A capture file is
created exclusively (owner-only); choose a new filename rather than overwriting.

The trace endpoint `/api/rng/trace?after=N` is authenticated/read-only. It copies
the ring under the mutex, then streams JSON outside it. There is no per-frame event
log, extra measurement RNG read, serial spam, flash trace or diagnostic injection
endpoint. Capture traffic may add timing jitter; actual counts/timestamps are used.

## Modes and deterministic tests

Static ambient/manual calibration and experiment-status hues legitimately bypass
the walk. Guided calibration overrides the entire display temporarily. LED disable
and health/entropy safety gates make it black. Pixel/activity modes alter masks;
breathing adds time-driven **brightness**, not a hue cycle. The live verifier
requires mode 1 rather than mislabeling these overrides as RNG failures.

`firmware/test/pipeline_test.cpp` runs the same production functions with a native
silicon API double: known +4, −4, cancelling 0 and neutral 0 net-bit windows give
targets 359° → 1° → 359° → 359° → 359° at 0.5°/net bit. It checks smoothing, RMT
bytes, API fields, readiness transitions and that elapsed time alone cannot move
the target. Native fixture JSON is labeled `deterministic-diagnostic-TEST-ONLY` and
`experimental_record:false`; only explicit **offline** fixture tests may accept it.
The real experiment parser rejects that format. No deterministic source/test file
is compiled into production or selectable through its API, and no diagnostic test
invokes the real trial recorder.
