# Validation and release record

## Completed on the development host

- Official Waveshare schematic/demo inspected; matrix-specific constants
  established before driver selection. Artifact hashes in third-party notices.
- USB `303a:1001`, `/dev/ttyACM0`, read/write permission verified.
- Read-only esptool probe: ESP32-S3 silicon v0.2, 4 MB flash, 2 MB embedded PSRAM,
  40 MHz crystal, native USB Serial/JTAG. No flash write/erase performed.
- Complete 4194304-byte existing-firmware backup saved privately in `backups/`,
  with SHA-256; excluded from Git and release bundles.
- Pinned ESP-IDF 6.1 installed through supported installer and firmware built.
  Image approximately 881 KB; 71% of its app partition free; no PSRAM required.
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

Host tests do not establish that the physical hardware has passed acceptance.
GitHub workflows are supplied but have not been executed on GitHub by this agent.

## Required physical acceptance before a hardware-qualified release

1. Photograph model/revision/chip markings. Confirm physical board is the documented
   Waveshare ESP32-S3-Matrix; record revision or explicitly record unmarked.
2. Run `./tools/flash.sh`, review detected identity, retain backup and explicitly
   confirm overwriting the target. No board has been flashed by the initial agent.
3. Monitor boot. Check AP credentials unique and repeatable across reboot, entropy
   readiness, RMT initialization, storage state and stable heap. Initialize storage
   explicitly only if previous partition contents prevent first mount.
4. At brightness 0.03, verify red/green/blue using calibration; save RGB/GRB result.
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
