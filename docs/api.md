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
| GET | `/api/settings` | Saved/active settings below |
| POST | `/api/settings` | Apply partial settings patch and persist; explicitly restart walk; denied during trial |
| POST | `/api/session/start` | Start fixed-N recording; prior saved trial must be exported/cleared |
| POST | `/api/session/stop` | `{}`; stop early, label `user_stopped`; idempotent if inactive |
| POST | `/api/session/clear` | `{}`; explicitly delete retained trial, only while inactive |
| GET | `/api/export` | Inactive saved trial as `application/x-ndjson` attachment |
| POST | `/api/wifi` | `{"ssid":"name","password":"password"}`; 1–32 SSID bytes, 8–63 password bytes, WPA2-compatible; denied during trial |
| POST | `/api/wifi/recover` | `{}`; forget home Wi-Fi; AP/data retained; denied during trial |

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
