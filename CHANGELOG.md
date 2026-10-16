# Changelog

## 0.2.1 — live entropy-to-LED verification

- Read-only live sample bit counts, pending/committed movement, target/rendered
  hues, render-step inputs and commanded RGB through the existing status poll.
- Fixed 128-event RAM sample/commit ring and authenticated streaming trace GET;
  bounded host collection/replay without changing settings or experiment data.
- Gate accepted hardware reads before/after acquisition on AP readiness, actual
  AP/APSTA mode and WIFI_PS_NONE; reject automatic PM/fixed-pattern RNG builds.
- Timestamp acquisition after the RNG read and scheduler decisions after taking
  the API mutex, avoiding stale pre-lock acquisition timestamps.
- Native known-sequence full-pipeline tests and independent Python replay detect
  target/render/RGB mismatches, gaps, resets, readiness rejection and mode overrides.
- Preserve the user's confirmed 0.2.0 physical calibration. No new flash performed.

## 0.2.0 — color synchronization and user-observed calibration

- Replace the fixed-HSL browser swatch with the final transmitted RGB command,
  shared frame/mask state, frame timestamp/age and separate target/logical hues.
- Keep gamma exactly once; add optional bounded RGB gains and explicit driver/
  wire mapping with the existing saved RGB/GRB setting preserved on upgrade.
- Add primary-observation order inference and a seven-color verification pass,
  2% PWM, explicit save/cancel, stale-step protection and 60-second inactivity timeout.
- Persist ordering/gains atomically without touching Wi-Fi or saved trials;
  legacy settings migrate with unity gains and no invented confirmation.
- Add production frame/API/browser consistency, RMT mapping, animation, calibration
  and NVS persistence regression tests and updated physical-validation instructions.
- The update is built/tested on the host; physical calibration is not yet confirmed.

## 0.1.0 — hardware-validation candidate

Initial original implementation: native ESP-IDF firmware, hardware RNG readiness
and health monitoring, independent random-walk/animation engine, conservative
matrix rendering, local AP/STA onboarding and recovery, authenticated browser UI,
persistent settings and bounded raw trial journal, offline statistical audit,
Linux tools, host tests, CI and candidate release packaging.

Known qualification limits: physical PCB revision/channel order/current/thermal
and real-device runtime/network/storage acceptance remain pending. External TRNG
transports, USB host mode, blinded allocation, OTA and pixel-perfect animation
replay are future extensions, not claimed features.
