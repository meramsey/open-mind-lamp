# Validation and release record

## Completed on the development host / initial deployment

- Official Waveshare schematic/demo inspected; matrix-specific constants
  established before driver selection. Artifact hashes in third-party notices.
- USB `303a:1001`, `/dev/ttyACM0`, read/write permission verified.
- Read-only esptool probe: ESP32-S3 silicon v0.2, 4 MB flash, 2 MB embedded PSRAM,
  40 MHz crystal, native USB Serial/JTAG.
- Complete 4194304-byte existing-firmware backup saved privately in `backups/`,
  with SHA-256; excluded from Git and release bundles.
- Pinned ESP-IDF 6.1 installed through supported installer and firmware built.
  No PSRAM required. 0.1.0 was approximately 881 KB; 0.2.0 remains within the same
  application partition with approximately 71% free.
- Native C++ invariants passed with UndefinedBehaviorSanitizer: hue wrapping,
  frame-independent smoothing/targets, raw bit accounting, brightness cap,
  deterministic repeatability and health-alarm behavior over 100000 words.
- Eight Python analysis tests passed: exact combinatorial reference comparisons,
  tails, count/timing/type integrity, early/interrupted/test-source eligibility,
  truncated-file recovery, duplicate keys and control comparison.
- Chromium and Firefox integration workflows passed at 390 px and 1280 px:
  login, dashboard, theme, settings, experiment start/locking/stop, download,
  clear and logout; no external requests or JS errors. Device API was mocked.
- Shell scripts checked by ShellCheck. Local build and candidate packaging are
  automated; SPDX inventory, upstream notices and artifact checksums are included.
- Subsequent authorized 0.1.0 flash succeeded, write hashes were verified and boot
  logs showed the application/AP/DHCP startup. The user reports a working dashboard
  and a screen/LED color mismatch. Model is user-confirmed; PCB revision is
  unmarked/unknown, not a confirmed numbered revision.
- 0.2.0 host regressions additionally cover the production final RGB frame and
  serializer, seven primary/secondary/white vectors, gamma/PWM/gains, RGB/GRB
  driver bytes, sparse masks, transitions, failed transmission state, user-observed
  workflow tokens/timeouts, legacy calibration migration and NVS save failures.
  Host NVS/RMT doubles are used, not physical optical measurements.
- Chromium/Firefox regressions feed actual C++-serialized frame fixtures into the
  dashboard, check raw CSS RGB including breathing/off/failure states, confirm
  the observation/save interaction and test delayed polling/mutation races.
- The user subsequently flashed 0.2.0 and confirmed physical RGB/GRB calibration
  successfully. That saved configuration is authoritative and is not recalibrated
  or replaced by the entropy-verification work. PCB revision remains unknown.
- 0.2.1 adds a native known-sequence test of the production hardware wrapper/walk/
  smoothing/RMT/API chain, independent raw-window/frame replay and dashboard RNG
  diagnostics coverage. It has not been flashed by this task; an actual bounded
  hardware trace still requires authorized deployment and the procedure in
  [rng-validation.md](rng-validation.md).

Host tests do not establish that the physical hardware has passed acceptance.
For color checkpoint `96d9df0`, both GitHub workflows completed successfully:
[firmware cross-build](https://github.com/meramsey/open-mind-lamp/actions/runs/38902666236)
and [host/browser tests](https://github.com/meramsey/open-mind-lamp/actions/runs/38902666684).
These validate code/build behavior, not the physical color observations.

## Required physical acceptance before a hardware-qualified release

1. Photograph model/revision/chip markings. Confirm physical board is the documented
   Waveshare ESP32-S3-Matrix; record revision or explicitly record unmarked.
2. Run `./tools/flash.sh`, review detected identity, retain backup and explicitly
   confirm overwriting the target. Initial 0.1.0 deployment is complete; deployment
   of subsequent updates needs fresh explicit authorization.
3. Monitor boot. Check AP credentials unique and repeatable across reboot, entropy
   readiness, RMT initialization, storage state and stable heap. Initialize storage
   explicitly only if previous partition contents prevent first mount.
4. Physical calibration was reported successful by the user on 0.2.0; retain it.
   For a different physical board or an explicitly requested recalibration, run
   the guided 2%-PWM primary identification and seven-color verification pass
   described in [color-calibration.md](color-calibration.md); explicitly save only
   after observing red/green/blue/white/yellow/cyan/magenta physically matching.
   Verify all 64 chain indices/orientation; record any difference from official demo.
5. Measure input current/voltage and board temperature at default and intended
   maximum operating brightness, including white, with radio active and final
   diffuser/enclosure. Choose final operating limit based on those observations.
6. Join AP using physical Android Chrome and Firefox without Internet; verify
   onboarding, controls, export and standalone behavior after closing the browser.
   Check desktop browsers on intended OSs; host tests cover Linux engines only.
7. Configure home Wi-Fi, observe LAN IP/mDNS, remove router power, and verify AP
   recovery. Test five-second BOOT runtime recovery without reflashing.
8. Record a fixed-N trial, compare export's raw counts with the analyzer, reboot
   and export retained data. Confirm settings restore and no trial autostarts.
9. Interrupt power during a sacrificial trial. Confirm missing/partial footer is
   reported, prior data is preserved and new trials require explicit clear. Test
   full 4096-word retention, repeated session cleanup and storage failure behavior.
10. Observe 24-hour standalone stability/heap and sampling jitter under API load.
    Capture hardware RNG samples for independent diagnostics. Document RF/power/
    environmental conditions; passing tests is not proof of unpredictability.

These checks require physical observation and permission to overwrite the attached
board. Their results cannot be inferred from a successful compile or USB chip ID.

## Reproducing host verification

```bash
./tools/setup.sh --skip-system  # after system packages are present
./tools/test.sh
./tools/test-web.sh
./tools/build.sh
./tools/release.sh
```

Release bundles are explicitly labeled hardware-validation candidates until the
physical record is completed. No remote repository, tag or published release is
created automatically.
