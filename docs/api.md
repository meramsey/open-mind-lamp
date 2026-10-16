# Local HTTP API (schema 1)

Base: `http://192.168.4.1` or the LAN IP / device's unique mDNS hostname.
No Internet endpoints, CORS allowance or telemetry. Static assets are public;
**every `/api/*` endpoint requires** `Authorization: Bearer <64-hex-admin-token>`.
The token is obtained through USB onboarding, not a hardcoded shared secret.

Request bodies must be JSON objects, at most 2048 bytes, nesting at most 8,
with unique top-level keys. State changes use POST. Unknown/out-of-range lamp
settings are rejected atomically. Booleans must be JSON booleans; integer settings
must be integral. Responses are JSON except NDJSON export. All responses use
`Cache-Control: no-store`.

| Method | Route | Behavior |
|---|---|---|
| GET | `/api/status` | Firmware, uptime, hue/target, heap health, RNG word/ones totals, health latch, missed slots, LED status, network and session |
| GET | `/api/rng/trace?after=N` | Read-only RAM trace after sequence N (default 0); no capture control or data writes |
| GET | `/api/settings` | Saved/active settings below |
| POST | `/api/settings` | Apply partial settings patch and persist; explicitly restart walk; denied during trial |
| POST | `/api/session/start` | Start fixed-N recording; prior saved trial must be exported/cleared |
| POST | `/api/session/stop` | `{}`; stop early, label `user_stopped`; idempotent if inactive |
| POST | `/api/session/clear` | `{}`; explicitly delete retained trial, only while inactive |
| GET | `/api/export` | Inactive saved trial as `application/x-ndjson` attachment |
| POST | `/api/wifi` | `{"ssid":"name","password":"password"}`; 1–32 SSID bytes, 8–63 password bytes, WPA2-compatible; denied during trial |
| POST | `/api/wifi/recover` | `{}`; forget home Wi-Fi; AP/data retained; denied during trial |
| POST | `/api/calibration/start` | `{}`; temporary 2%-PWM wiring test; denied during trial or an active calibration |
| POST | `/api/calibration/observe` | `{"run_id":1,"step":0,"observed":"red"}`; current presented frame only; primary inference followed by seven-color verification |
| POST | `/api/calibration/save` | `{"run_id":1,"step":10}`; persist proposed order only after seven verification matches and final presented frame |
| POST | `/api/calibration/cancel` | current `run_id`/`step`; discard override, retain saved order |

HTTP status: 200 success, 400 invalid JSON/body limits, 401 token missing/invalid,
404 unknown GET route, 409 invalid/unavailable operation or parameters, 500 internal
allocation/file errors. Failed POST operations return `{"error":"..."}`. The UI
surfaces these messages. Network credentials are never returned by the API.

## Lamp settings

| Field | Range | Default |
|---|---|---|
| `brightness` | 0–0.125 physical PWM scale | 0.08 |
| `saturation` | 0–1 | 0.9 |
| `sensitivity` | 0–5 degrees/net bit | 0.2 |
| `smoothing_ms` | 0–30000 | 1500 |
| `starting_hue` | 0 ≤ h < 360 | 180 |
| `sample_hz` | integer 1–100 words/s | 20 |
| `accumulation_ms` | integer 10–60000 | 1000 |
| `frame_hz` | integer 5–60 | 30 |
| `mode` | integer 0 ambient, 1 walk, 2 breathing, 3 pixel, 4 calibration, 5 activity, 6 experiment | 1 |
| `pixel` | integer chain index 0–63 | 0 |
| `enabled` | boolean | true |
| `rgb_order` | false=GRB, true=RGB | false |
| `gain_red`, `gain_green`, `gain_blue` | each 0.25–2; post-gamma/PWM channel multiplier | 1 |
| `channel_order_confirmed` | read-only; set only by successful workflow Save, cleared on manual order change | false |

Settings/experiment starts/Wi-Fi edits are denied during active calibration.
Calibration inference is based on user-reported physical observations, not optical
detection; only the two officially demonstrated orders (RGB/GRB) are candidates.
An inconclusive sequence, Cancel, failure, reboot or 60 seconds of inactivity
leaves saved settings unchanged. Save failure keeps the run ready for retry
until timeout. The workflow bypasses normal mode, LED enable and gains, with a
fixed safe 2%-PWM test; it still obeys health/readiness gating.

## Rendered color snapshot (`GET /api/status`)

`hue` and `target_hue` retain the smoothed walk and walk target meanings.
`color` is an atomic snapshot of the last **successfully transmitted** frame:

- `valid`: false before first success or after a failed transmission; actual output
  is then unknown. Previous successful fields may remain for diagnostics.
- `frame_id`, `rendered_at_us`, `age_ms`: successful-refresh sequence/completion
  time and age, independent of browser poll rate.
- `logical_hue`, `logical_rgb` (three 0–255 integers), `saturation`, `brightness`,
  `mode`: mode-selected color before gamma and the effective frame PWM scale.
- `commanded_rgb`: semantic RGB, after gamma once, brightness, gains and cap.
- `driver_rgb`: library arguments for fixed GRB driver format.
- `wire_bytes`: ordered bytes on the wire; `channel_order` is `RGB` or `GRB`.
- `lit_pixels`, `lit_mask`: count and 16-hex-digit pixel mask; each set bit uses
  commanded RGB, remaining pixels are black. The mask is zero for an all-black frame.
- `calibration_run`, `calibration_step`: temporary test-frame identity or zero
  when not in a guided workflow.

`calibration` includes `active`, `phase`, `ready`, `presented`, `run_id`, `step`,
`index`, `expected`, `candidate_order`, `saved_order`, `saved_order_confirmed`,
`remaining_ms` and `brightness`. Phases are idle/identify/verify/ready/saved/
cancelled/timed_out/inconclusive/interrupted. Observation names are red, green,
blue, white, yellow, cyan, magenta or other. The initial three observations use
the saved ordering; verification starts again at red under the inferred candidate.
No data change occurs until Save. See [workflow and display limits](color-calibration.md).

These RGB values are commanded output, **not optically measured light**. The
browser copies `commanded_rgb` into CSS directly and keeps polling at 1500 ms,
with one in-flight status request. Snapshot age grows locally between responses;
animated colors can move between samples. No RNG/color prediction is performed.

## Live RNG diagnostics (0.2.1)

`/api/status.rng` adds source/readiness, successful acquisition count, readiness
rejection count, latest unsigned word/hex, ones/zeros/net, whether applied, acquisition
timestamp/age, current window ID/sample count/net/pending degrees, last commit
before/after/net/movement, target/smoothed hue, health latch and the three rates.
Before the first successful acquisition, `latest_word` is null. A successful
acquisition includes the health-triggering word; `latest_applied:false` distinguishes
it from a healthy word passed to the walk. IDs/secrets/radio-internal RNG calls
are not lamp measurement samples.

`color.walk_hue` is the exact walk hue passed to the frame builder, before mode
overrides. `color.render_step` gives before/target/dt_ms/smoothing_ms/after for the
actual successful frame's `Walk::render()` call. It is retained with the frame,
not recomputed from the current clock. `network` adds `ap_started`, `wifi_mode`
and `power_save_none`; readiness checks driver readback at each acquisition boundary.

Trace responses contain schema/type/source/boot ID/settings revision, lamp settings,
RNG and color snapshots, oldest/latest event sequences, capacity and `events`.
Events are reset/sample/commit: samples include exact SDK-return words and applied
flags; commits include window count/net and target before/after. Records older
than the 128-event RAM ring or latest reset are unavailable; consumers must detect
sequence gaps. Serialization streams from a copied snapshot outside the sampling
mutex. No flash/experiment storage or calibration is touched. There is no write
endpoint, deterministic-injection option or dashboard background trace polling.

See [bounded live validation](rng-validation.md) for the read-only collector,
independent replay, correct display mode and limits of physical entropy claims.

Example:

```bash
curl -H "Authorization: Bearer $OML_TOKEN" http://192.168.4.1/api/status
curl -H "Authorization: Bearer $OML_TOKEN" -H 'Content-Type: application/json' \
  -d '{"brightness":0.04,"mode":1}' http://192.168.4.1/api/settings
curl -H "Authorization: Bearer $OML_TOKEN" -H 'Content-Type: application/json' \
  -d '{"condition":"control","direction":"high","protocol":"protocol-1","samples":1200,"client_utc":"2026-10-15T12:00:00Z"}' \
  http://192.168.4.1/api/session/start
```

Use environment variables/private files for secrets rather than placing literal
tokens in shared scripts/logs. The API uses HTTP, so use only trusted local links.

## Export format

NDJSON consists of exactly one `header`, zero or more `sample` objects and an
optional `footer` if the session closed successfully. A sample looks like:

```json
{"type":"sample","i":0,"t_us":1240,"word":2863311530,"late_us":0,"healthy":true}
```

`word` is the raw unsigned 32-bit RNG output, without conditioning. `t_us` is
relative to the header's device monotonic `start_us`; it is not wall-clock time.
`late_us` is lateness relative to the sample scheduler's planned slot. The first
sample follows the HTTP start request after scheduling resumes. The analyzer
verifies footer counts against raw data. Download after stop; streaming a changing
file is intentionally unsupported. Reboot-retained status provides export
availability; metadata and counts should be read from the file, not zeroed RAM.
