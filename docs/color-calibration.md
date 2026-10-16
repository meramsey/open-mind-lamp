# Color pipeline, calibration and deployment

## Diagnosis of the 0.1.0 discrepancy

The old dashboard drew `hsl(smoothed_walk_hue 80% 55%)`. It was an illustrative
swatch of the smoothed walk hue, **not the target hue and not the final LED command**.
It ignored actual saturation, physical brightness, gamma, LED disable, breathing,
fixed ambient/calibration hue, experiment-status hue and sparse-pixel masks.
There was no final rendered RGB state in the API. A 1500 ms polling delay also
made the swatch a past sample without displaying its age.

Audit findings across the full path:

| Stage | Finding / contract |
|---|---|
| RNG | Raw `esp_random()` word with RF readiness gate; no color-channel interpretation |
| Walk | `2*popcount(word)-32` accumulation, wrapped hue target and time-based shortest-path smoothing; no change to statistical algorithm |
| Mode | Ambient/manual calibration uses starting hue; breathing modulates PWM; experiment mode overrides hue; activity/pixel modes set masks |
| HSV | Standard six-sector HSV conversion; hue wraps, saturation normalized to 0–1 |
| Gamma | Exactly one `pow(component,2.2)` on normalized chromatic channels; unchanged default behavior |
| PWM/gains | Physical brightness multiplies gamma output, then per-channel gain; clamp 0–32 and round once to uint8 |
| Ordering | Saved `rgb_order` preserved; one explicit RGB↔GRB adaptation, not an unexplained second swap |
| RMT | led_strip 3.0.3 directly places component bytes according to configured GRB format; adds no gamma or brightness. DMA refresh waits for completion |
| Snapshot/API | Successful-refresh frame is the sole published command state; failures retain the previous frame but mark output unknown |
| Browser | Literal `rgb(r,g,b)` from `commanded_rgb`, no HSL conversion, gamma, brightness normalization, color interpolation or prediction |
| Synchronization | One in-flight 1500 ms status poll shared with user actions; pre-mutation/logout responses cannot overwrite newer state. A local timer displays sample age without extra requests |

No arithmetic overflow, duplicate gamma or incorrect extra driver swap was found
in the old default path. The definite bug was the independent browser color path.
The physical board's correct channel order and any spectral imbalance remain
uncertain until the user observes the LEDs. Waveshare's current `Color/WS_Matrix.cpp`
uses NEO_RGB; its Font/HTTP demos use NEO_GRB. Only these two documented candidate
orders are inferred; other observations are reported inconclusive, not guessed.
The physical PCB revision remains **unknown/unmarked**.

## Guided workflow (after authorized deployment)

1. Stop any experiment. Retain/export its data; calibration does not clear it.
   Leave the board ventilated. Open the dashboard's **Guided LED color calibration**.
2. Select **Start low-brightness test**. This deliberately overrides normal visual
   mode and LED enable while testing, but does not change saved settings. Health/
   entropy failures still stop the test. Test output is fixed at **2% PWM** with
   **unity gains**, including white, independent of ordinary brightness/gains.
3. Look at the **physical lamp**, not the dim browser swatch. Identification shows
   red, green, blue under the existing saved order. For each, choose **Physical
   color matches**, or select the color actually seen and **Report observed color**.
   Select Other / dark / uncertain if unsure. A successful RMT frame must have been
   transmitted before the confirmation controls are enabled.
4. If observations are red/green/blue, the existing order is proposed. If they are
   green/red/blue, the opposite order is proposed. Other patterns end the test as
   inconclusive with saved settings unchanged. This is inference from your answers,
   not automatic optical detection. A wrong observation can produce a wrong proposal.
5. The proposed order is tested with **red → green → blue → white → yellow → cyan
   → magenta**. Confirm the physical appearance of every color. A mismatch ends
   the run without saving; inspect the wiring/observations and restart.
6. Only after all seven match, select **Save confirmed ordering**. The order and
   user-confirmation flag are atomically persisted in NVS. Normal mode resumes.
   Until Save, your previous order remains authoritative.
7. Reload the page or reboot and check the saved order and confirmation flag.
   The workflow state itself is temporary; saved ordering/gains persist. A restart,
   Cancel, failure, or **60 seconds without a valid observation** discards the
   temporary override and resumes standalone mode without changing saved calibration.

Settings/experiments and Wi-Fi edits are locked during the workflow. Duplicate,
stale, wrong-run or not-yet-presented observations are rejected. Cancel/save also
use run/step tokens. A browser disconnect cannot leave the test on indefinitely.
This workflow confirms channel ordering only, not radiometric/colorimetric accuracy.

## Optional per-channel balance

After ordering is confirmed, use ordinary manual calibration mode (mode 4), low
brightness (e.g. 0.02–0.04), saturation 0 for white, and **Sampling and hardware**
gain fields. Each of `gain_red`, `gain_green`, `gain_blue` defaults to 1 and accepts
0.25–2. Reduce a strong channel first, save and reassess white/secondaries. These
are subjective manual balance factors, not sensor-derived calibration or a display
color profile. They affect standalone rendering and trial visual settings, not
raw RNG words. Experiment headers include the saved gains/order. The guided
wiring test intentionally bypasses them so a previous balance adjustment cannot
hide a channel-order fault. The 32/255 channel ceiling always applies; clipping
may change hue at high brightness/gain combinations.

## Reading the dashboard/API

- **Walk target hue**: the accumulator's desired hue before smoothing.
- **Smoothed walk hue**: the internal animation hue; mode overrides may differ.
- **Rendered logical hue / logical RGB**: mode-selected, pre-gamma chromatic color
  from the successfully transmitted frame. White's hue is arbitrary; saturation
  zero is what makes it white.
- **Commanded RGB**: final semantic RGB bytes for lit pixels after all software
  transformations. Unlit pixels are black. A mask and lit count describe sparse
  modes; the swatch is the lit-pixel command, not a spatial average of the matrix.
- **Driver RGB / wire bytes**: diagnostic library arguments and serialized byte
  sequence, respectively; these are not alternative semantic colors.
- **Frame/age/validity**: API records completion time. Swatch stays on the latest
  received sample, not an extrapolation of randomness; it can lag by polling plus
  network time. The browser's approximate age conservatively adds the full status
  request duration to the server-reported age, plus local elapsed time; no shared
  clock or one-way latency estimate is assumed. Age >5 seconds is labeled STALE. On transmit failure output is
  unknown and the preview is cleared, with the last successful command retained
  only for diagnostics.

The software has no photodiode/color sensor and cannot measure emitted light.
An RGB PWM command is not a spectroradiometric measurement. Browser screens use
sRGB transfer functions, their own brightness/white point, color management and
spectra; LEDs have different spectral peaks, efficiencies and diffusion. The
raw-command swatch is intentionally very dim at low PWM and is **not brightness
normalized**. Matching its numeric bytes does not imply equal perceived hue,
brightness, white balance or absolute light output. Do not judge physical
calibration from a screenshot alone.

## Safe deployment commands

These are prepared commands, **not authorization for the agent to flash**. The
0.2.0 update has not been deployed during color development.

```bash
cd /home/mike/Documents/dev/open-mind-lamp
./tools/test.sh
./tools/test-web.sh
./tools/build.sh
# Only when you explicitly intend to deploy (stop/export any active trial first):
OML_PORT=/dev/ttyACM0 ./tools/flash.sh
./tools/monitor.sh
```

The flasher creates a fresh timestamped backup and asks for model, PCB revision
(`unmarked` after inspection), and `FLASH /dev/ttyACM0`. It writes bootloader,
unchanged partition table and application only; it does not erase NVS or the
session partition. Never run `erase-flash` or initialize-storage for this update.
Existing factory backups are neither read nor overwritten by build/tests; the
backup script refuses an existing output path. The settings key remains
`settings_v1`; old saved RGB/GRB order migrates unchanged, new gains default to 1,
and a confirmation flag defaults false rather than inventing a physical result.

Verify firmware **0.2.0** in the monitor/dashboard. Join the existing AP (this
board: OpenMindLamp-408C15), open http://192.168.4.1 and log in with the existing
token. Refresh the page so firmware/UI versions match. For read-only diagnostics:

```bash
read -rsp 'Admin token: ' OML_TOKEN
export OML_TOKEN
curl --fail --silent --show-error \
  -H "Authorization: Bearer $OML_TOKEN" \
  http://192.168.4.1/api/status | python3 -m json.tool
unset OML_TOKEN
```

Confirm `color.valid`, `commanded_rgb`, `channel_order`, `lit_pixels`, recent
`age_ms`, and separate `hue`/`target_hue`. Run the physical workflow and report the
actual primary/white/secondary observations, saved order, and any remaining hue
imbalance. No physical calibration pass is claimed until these observations are
confirmed by the user. Temperature/current measurements remain separate checks.
