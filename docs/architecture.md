# Architecture

## Data path

`active RF → esp_random() → unconditioned 32-bit word → health checks + raw recorder
→ net-bit accumulation → hue target → time-based smoothing → mode selection
→ HSV chromatic RGB → gamma once → PWM/gains/clamp → ColorFrame → RMT DMA`

After a successful RMT refresh the same `ColorFrame` is stored in `RenderedColor`
and serialized by `api/color_json.*`; the browser previews its final commanded
RGB bytes. No separate browser HSL approximation is used. Failed transmissions
mark the state unknown without replacing the last successful snapshot. Sparse
mode masks, frame IDs, timestamps and age accompany the lit-pixel RGB command.
Target, smoothed walk, and mode-selected logical hues are distinct concepts.

Raw acquisition, statistical interpretation and rendering are separate. All 32
bits contribute to the lamp; no modulo mapping or rejection removes samples.
IDs and onboarding secrets also use RNG but are not measurement samples.

`main.cpp` owns the standalone loop. Sampling uses monotonic microsecond
deadlines, accumulation has its own deadline, and rendering has a separate frame
deadline. The main task yields every tick. There is no browser-driven animation
clock. When late, acquisition records one current word, records lateness, counts
missed slots, and advances its deadline without fabricating catch-up samples.
Flash/NVS activity may introduce timing jitter; it is observable in exported data.

The HTTP task and lamp task share one mutex. Settings/record operations are
serialized. Export is only allowed after recording stops; streaming the saved
file does not hold the lamp mutex. No heap allocations are performed per animation
frame; `fprintf` uses a bounded record format and libc buffering. JSON allocation
is confined to boot/API/session header operations. SPIFFS writes happen in the
sampling loop, so this is not a hard-real-time instrument.

## Modules

- `lighting/core.*`: portable settings validation, HSV conversion, random walk,
  and continuous gross RNG-failure monitors. Native C++ tests use the same code.
- `entropy/source.hpp`: source interface and gated hardware RNG. Future serial
  or USB adapters must implement provenance, readiness, transport integrity and
  their own validation. USB-host TRNG support is **not implemented**: the board's
  native USB is currently used as a device and host/power requirements differ.
- `entropy/deterministic.hpp`: SplitMix64-derived test stream, test-only. Not
  included by production code; never suitable for an experimental hardware source.
- `lighting/color.*`: allocation-free shared mode/color/mask frame builder and
  successful-transmission snapshot. `lighting/matrix.*` only maps/transmits it.
- `lighting/calibration.*`: user-observed RGB/GRB inference, seven-color
  verification and explicit save/cancel/timeout state machine. Overrides are
  temporary, at 2% PWM/unity gains; measurements cannot be recorded concurrently.
- `networking/`: unique AP onboarding, optional STA, no-sleep RF entropy condition,
  retries, mDNS, recovery. AP remains enabled even without clients or Internet.
- `storage/`: ESP-IDF NVS settings and secrets. Settings are version-keyed JSON,
  atomically committed in NVS, validated on load. NVS errors do not trigger erasure.
- `experiments/`: bounded NDJSON journal with explicit start/stop/failure states.
- `api/`: authenticated local HTTP API plus embedded static assets. Body length,
  nesting and duplicate top-level keys are bounded before processing.
- `analysis/`: independent counts, audit, statistics and conversions, no dependencies.

Calibration order and optional gains use the existing NVS settings document with
backwards-compatible defaults. A prior saved order is never replaced just because
its confirmation flag is missing. Only user-confirmed Save or an explicit manual
ordering edit can change it. Native tests exercise production persistence through
an NVS double and production rendering through a configured-GRB LED driver double;
browser fixtures use the production frame serializer. Hardware optical acceptance
is still a distinct requirement.

## Entropy and health

Espressif guarantees the documented physical entropy conditions when RF is active
or the SAR entropy source is explicitly enabled. The lamp keeps AP active,
disables Wi-Fi power save and never enters light/deep sleep. Sampling is gated
on the AP-start/stop events. Early first-boot secret generation temporarily uses
`bootloader_random_enable()`, then **disables it before Wi-Fi initialization**.
Do not add ADC use or sleep without reviewing the entropy contract.

The continuous monitors, in LSB-first bit order, latch on a run of 64 equal bits
or a disjoint 1024-bit window with fewer than 384 or more than 640 ones. They
start at boot and span sample words. They are deliberately broad gross-failure
indicators, not SP 800-90B certification or a min-entropy estimate. On an alarm,
the triggering word remains in the trial, the trial is labeled `health_failure`,
sampling stops and LEDs turn off. Reboot clears the latch; investigate first.

These monitors cannot detect all faults, prove fresh entropy, or show genuine
unpredictability. Trial-start health history is not reset, so independently
rechecking a session alone may not reproduce an alarm spanning pretrial bits.
The recorded per-word health status is authoritative for that acquisition history.

## Persistence and recovery

Only one trial is retained, up to 4096 words. Header is flushed/fsynced at start;
samples flush/fsync each 32 words; footer syncs on stop. Up to 31 most recent
words may be buffered; filesystem damage from power failure can lose more.
An existing trial prevents another start until explicitly cleared. After reboot,
status says `retained_or_interrupted` and raw export remains available; offline
analysis determines the prior trial's actual metadata/completion. In-RAM session
counts shown after reboot are zero, not reconstructed counts.

Only an entirely erased session partition can be autoformatted. A corrupt or
unknown filesystem is preserved and experiments are disabled. Lamp/network can
continue. Manual initialization has a separate explicit erase command.

Normal lamp mode writes no samples or hue checkpoints. Boot resumes saved mode
from the configured starting hue rather than wearing flash with frame updates.
Interrupted experiments are never resumed as though uninterrupted. No OTA,
secure boot or flash encryption provisioning is performed by this release.

## Security boundary

Unique random AP password and admin token are generated from physical entropy
and stored in NVS. The UI is local HTTP with an Authorization bearer token; all
API reads/writes require it. No CORS permission is emitted; cross-origin pages
cannot supply the required header without a rejected preflight. CSP and no-store
headers limit common browser risks. This is not TLS: other parties on a trusted
LAN may observe traffic. NVS is **not encrypted at rest** in the reproducible
default profile. Physical USB/flash access reveals secrets; that is the recovery
trust channel. Use a trusted local network and do not port-forward this service.
