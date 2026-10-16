# Troubleshooting

## No serial port / wrong device

Use `lsusb` and `./tools/setup.sh --skip-system`. Expect native `303a:1001` and a
`/dev/ttyACM*` path, normally `/dev/ttyACM0`. Use a data-capable cable and try a
direct USB port. If needed hold BOOT, press/release RESET, then release BOOT to
enter ROM download mode. This does not erase flash. Close serial monitors before
probing/flashing; only one process can reliably control the port.

USB identity confirms the interface, not the board model. A different ESP32-S3
can enumerate identically; visually check PCB model/revision and the schematic.
Do not flash an unrelated board simply because esptool recognizes its chip.

## Permission denied

Check `getfacl /dev/ttyACM0`. An active desktop session may already grant user
access via `uaccess`. If not, run `sudo usermod -aG dialout "$USER"`, fully log out
and back in, and retry. Scripts do not automatically change groups. Avoid
`chmod 666`, running the toolchain as root or disabling system security policies.

## Missing toolchain / Python

Run setup again; it uses Espressif's managed environment. Do not use
`--break-system-packages`. If pyenv selects an incompatible Python, ensure Ubuntu's
Python 3.12 is available (`/usr/bin/python3`); IDF's supported installer chooses a
compatible interpreter. A preexisting wrong IDF checkout is rejected rather than
reset. Set `OML_IDF_PATH` to the exact pinned checkout. Preserve any local changes
before replacing a damaged toolchain yourself.

## AP has no Internet / browser cannot connect

That is normal: stay connected to the lamp AP, temporarily prevent Android from
switching to mobile data if needed, and type `http://192.168.4.1`. The device does
not implement a captive-portal interception server. Use HTTP explicitly.
The admin token is not the Wi-Fi password. Retrieve both from the USB boot log.
If a dashboard disconnect occurs, the standalone lamp and trial continue.

If mDNS fails, use the dashboard/log's numeric LAN IP. Only 2.4 GHz,
WPA2-Personal-compatible home networks are configured by this profile. A router
outage never removes the recovery AP. Hold BOOT for five seconds while running to
forget home credentials; avoid holding it across reset unless entering the loader.

## LEDs wrong color, too warm, or resets

Manufacturer examples disagree about RGB/GRB; follow the low-brightness calibration
sequence in [hardware.md](hardware.md). Geometry affects pixel placement, not
uniform lamp behavior. Ensure all 64 chain indices light in diagnostic mode.

Reduce brightness immediately if the board gets hot. Check actual supply/cable
voltage/current and ventilation. A software cap is not a measured thermal guarantee.
Use a proper regulated 5 V source; never feed 5 V into a GPIO. Repeated brownout
resets can interrupt trials and should be investigated before collecting data.

## RNG alarm / missed samples

An RNG gross-failure alarm latches until reboot and disables lamp sampling/LEDs.
Export the affected trial, record the event and examine source readiness/power/
firmware before restarting. Passing basic tests cannot establish unpredictability.

`missed_slots` is a cumulative diagnostic. Lateness/intervals in the export are
actual timing evidence. Flash writes and Wi-Fi can delay software sampling;
reduce rate or keep default 20 Hz if necessary. No missed sample is silently
replaced with a burst of catch-up measurements.

## Session storage unavailable or a trial will not start

An existing saved trial blocks a new one. Stop, download, audit, then explicitly
clear it. Following reboot, RAM counts are zero; the exported header/footer retain
the original metadata. Incomplete exports are marked interrupted by analysis.

If SPIFFS is unavailable, the firmware preserves unknown/corrupt content instead
of formatting it. With a full flash backup and verified Open Mind Lamp partition
layout, `./tools/initialize-storage.sh` can erase only the session partition after
explicit confirmation. This permanently deletes any saved trial in that region.
NVS errors are also preserved; inspect the boot error and back up before any
manual repair. No ordinary setup/build command erases NVS.

## Web test browser dependencies

Test browsers are installed under `.tools/browsers`. If Playwright reports missing
Ubuntu libraries, run `.tools/web-venv/bin/python -m playwright install-deps chromium
firefox` (apt privileges required), then rerun `./tools/test-web.sh`. Desktop engines
at mobile viewport sizes are tested; Android Wi-Fi/browser behavior still needs
physical acceptance testing.
