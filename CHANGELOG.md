# Changelog

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
