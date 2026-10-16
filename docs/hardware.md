# Hardware discovery and configuration

## Primary sources consulted

1. [Waveshare ESP32-S3-Matrix wiki](https://www.waveshare.com/wiki/ESP32-S3-Matrix)
   (page identifies WS2812B in FAQ, 8×8, GPIO14 examples, 4 MB flash and thermal warning).
2. [Official schematic PDF](https://files.waveshare.com/wiki/ESP32-S3-Matrix/ESP32-S3-Matrix-Sch.pdf).
3. [Official demo archive](https://files.waveshare.com/wiki/ESP32-S3-Matrix/ESP32-S3-Matrix-Demo.zip):
   `Color/WS_Matrix.h`, `Color/WS_Matrix.cpp`, `Font/WS_Flow.cpp`, `Game/WS_QMI8658.cpp`.
4. [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf).
5. [ESP-IDF 6.1 RNG requirements](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/random.html).
6. [ESP-IDF 6.1 release](https://github.com/espressif/esp-idf/releases/tag/v6.1).

Evidence retrieved during implementation on 2026-10-15 local time. Local board
probe and physical confirmation are separate: a chip/USB ID is not a PCB model ID.

## Confirmed design from official documents

| Item | Configuration / evidence |
|---|---|
| MCU | Schematic U66: ESP32-S3FH4R2; dual LX7, 4 MB embedded quad flash, 2 MB PSRAM |
| LEDs | 64 × WS2812B-0807 on schematic; one-wire 800 kHz family protocol |
| Data | GPIO14 → onboard transistor interface → LED_DIN; no unrelated board mapping assumed |
| Color order | Font/HTTP demo uses GRB; Color demo uses RGB. **Conflicting official evidence**; default GRB, selectable RGB, physical RGB calibration required |
| Geometry | Font demo: top-right origin, progressive columns; conceptual index `(7-x)*8+y`, coordinates top-left. Pixel diagnostic uses chain indices 0–63, so it does not depend on geometry |
| USB | USB-C direct GPIO19 D−, GPIO20 D+; native USB Serial/JTAG; no CH340 required |
| Buttons | BOOT GPIO0 (active low), RESET on CHIP_PU; never drive these as LED data |
| IMU | QMI8658C, I²C SDA GPIO11 / SCL GPIO12, interrupts GPIO10 / GPIO13; separate from GPIO14 |
| Power | USB VBUS nominal 5 V; onboard regulated 3.3 V logic and 5 V LEDs; GPIO signals remain 3.3 V |
| Other | IMU is left uninitialized to avoid an unnecessary dependency; no external ADC is used |

Manufacturer's generic setup screenshots contain material from other products.
The matrix-specific schematic/demo takes precedence over copied screenshots.
The available schematic does not establish a revision for the physical board.
Photograph the PCB's model and revision markings; if unmarked, record that plus
the chip marking and the official schematic used. Do not invent a revision.

## Actual host discovery

- Native USB VID/PID **303a:1001**, `/dev/ttyACM0`.
- Read-only esptool probe: ESP32-S3 QFN56 **silicon revision v0.2**, 40 MHz crystal,
  embedded 4 MB XMC flash, 2 MB AP_3v3 PSRAM, USB-Serial/JTAG mode.
- Host user has an explicit read/write ACL on the tty; group `dialout` membership
  is not currently needed for the active desktop session.
- **Silicon revision v0.2 is not the PCB revision.** The user identifies the
  model as ESP32-S3-Matrix; the physical PCB revision is unmarked/unknown, not
  confirmed as any specific revision. The user reports operating LEDs/dashboard
  after the initial flash. The user subsequently deployed 0.2.0 and confirmed
  successful physical RGB/GRB calibration. Preserve that saved order/gains;
  the RNG diagnostics do not require recalibration or guessing the order again.

## ESP-IDF profile

Native target `esp32s3` (not a fabricated board preset), 4 MB DIO flash at 40 MHz,
USB Serial/JTAG console, no mandatory PSRAM, custom partition layout. The firmware
checks chip model and flash size before GPIO initialization; it cannot detect
which PCB is wired to that chip. The flasher asks for physical model/revision.

The LED driver is Espressif `led_strip` 3.0.3, RMT with DMA at 10 MHz timing resolution.
DMA avoids Wi-Fi interrupt starvation of software-fed LED pulses.

## Power and calibration

Default physical PWM scale is 0.08. Settings accept at most 0.125 and every channel
is hard-clamped to 32/255 in software. A conservative old-style 60 mA/full-white
estimate gives `64 * 60 mA * 32/255 ≈ 482 mA` for the LEDs alone at the ceiling,
plus MCU/radio and quiescent LED current. This is an **estimate, not a measured
limit** for this package/revision. A weak 500 mA USB port can still be inadequate.
Use a good regulated 5 V supply with headroom (e.g. 1 A or more), a short cable,
and measure actual current/temperature before prolonged enclosed use.

No guaranteed board current or thermal limit is published in the cited sources.
The firmware cap is a conservative engineering policy, not manufacturer certification.
Do not raise it without measurements. No software thermal cutoff is claimed.

Use the [guided calibration procedure](color-calibration.md): three primary
observations under the saved order, then red/green/blue/white/yellow/cyan/magenta
verification under the proposed order. Every test uses unity gains and 2% PWM
(5/255 at an active channel); seven matches plus explicit Save are required.
No optical sensor is present and no automatic optical detection is claimed.
Both RGB and GRB are supported because the official examples conflict, not
because USB identity determines color order. Existing saved calibration is retained
unless the user completes and saves a new calibration or explicitly changes it.

Optional normal-operation RGB gains are each 0.25–2, default 1, applied after
gamma and physical brightness before rounding/clamping to 32/255. Pixel mode
indices 0–63 verify continuity/orientation. Raw acquisition/statistics are separate.

See [validation](validation.md) for the exact remaining acceptance procedure.
