# Getting started

## Parts and host

- Waveshare **ESP32-S3-Matrix** (not another S3 matrix board), data-capable USB-C
  cable, regulated 5 V USB supply/port. Start with the board uncovered and brightness low.
- Xubuntu/Ubuntu 24.04, Git, approximately 5–8 GB free for toolchain and browsers.
- Android or a desktop browser; no proprietary application needed.
- Optional frosted diffuser with an air gap; do not seal the board in insulating foam.

Read the [hardware evidence and unresolved revision check](hardware.md) first.
Inspect the actual PCB markings before applying this pin configuration.

## Installation

Run `./tools/setup.sh`. It installs Ubuntu packages through apt, checks out the
exact ESP-IDF 6.1 commit, and runs Espressif's supported `install.sh esp32s3`
workflow. Tools/Python packages are installed in Espressif's managed user
environment, never with system-wide pip. The IDF source lives in `.tools/esp-idf`;
upstream tool binaries are normally in `~/.espressif`. `OML_IDF_PATH` can name an
existing checkout at the exact pinned commit.

`./tools/setup.sh --skip-system` skips apt if packages are already installed.
Rerunning setup reuses the checkout and installed tool versions. Existing
checkouts at another commit are preserved and rejected. Setup detects USB and
checks permission; no attached board is required for host development.

Set `OML_PORT=/dev/serial/by-id/usb-...` when multiple Espressif USB devices are
present. Only native USB `303a:1001` is accepted by the supplied flash workflow;
USB-UART bridges require an intentionally adapted detection workflow.

## Build and preservation

```bash
./tools/test.sh
./tools/test-web.sh
./tools/build.sh
./tools/backup-firmware.sh
./tools/flash.sh
```

Backup reads the complete 4 MB flash into ignored, owner-only `backups/`, with a
SHA-256 companion. It temporarily resets into the ROM loader but does not erase
or write flash. Keep backups private: existing Wi-Fi credentials may be included.
The flasher always creates a fresh backup before confirmation. eFuses are outside
flash; they are not captured or modified. Encrypted/secure-boot devices need a
device-specific recovery plan; a flash dump alone is not a portable firmware image.

The first flash changes the partition table. Session storage at 0x300000 may
contain old firmware data. If the log reports an unavailable filesystem, first
retain the factory backup, then use `./tools/initialize-storage.sh` to explicitly
erase only the session area. This is destructive and separately confirmed; it
must only be used after verifying Open Mind Lamp's partition layout. It is not
needed for ordinary upgrades. Normal flashing preserves NVS and saved sessions.

## Onboard and run

Run `./tools/monitor.sh` and press RESET; exit monitor with Ctrl+]. Boot logs show:

- device-specific AP SSID and random 24-hex-character WPA2 password;
- independent random 64-hex-character admin token;
- AP IP 192.168.4.1 and unique `openmindlamp-XXXXXX.local` hostname.

These secrets persist in NVS and are printed on each boot so physical USB access
can recover them. Copy them privately. Browser token storage is tab-session-only;
theme preference alone is saved long-term. Do not save full boot logs in issues.

Join the AP on Android, accept staying on a network without Internet, and type
`http://192.168.4.1` explicitly (not HTTPS or a search query). Enter the token.
Save lamp settings. Defaults are 20 words/s, 1 s accumulation, 0.2° per net bit,
1500 ms smoothing, 30 fps, 8% physical PWM and 90% saturation.

For a home network, save a 2.4 GHz WPA2-Personal-compatible SSID/password in the
UI. Pure WPA3-only and enterprise networks are not supported by this profile.
Look up the LAN IP in the dashboard or serial log. mDNS is best-effort and may
be blocked by Android/router configuration; use the numeric IP if necessary.
The private AP stays on in station mode, including when the router is offline.

Hold BOOT **while firmware is running** for five seconds to forget home Wi-Fi
without deleting data/settings/secrets. Holding BOOT during reset enters ROM
download mode instead. Experiments are stopped and labeled during recovery.

## Restoring a backup

With the exact same device and unmodified security eFuses, source the pinned IDF
environment and use `python -m esptool --chip esp32s3 --port PORT write-flash 0
backups/FILE.bin` after checking its `.sha256` and explicitly deciding to restore.
This overwrites the complete flash including current settings and trials. Never
restore another device's flash backup. No restore command is run automatically.
