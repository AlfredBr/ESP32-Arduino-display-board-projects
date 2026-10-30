# ESP32-S3-Touch-AMOLED-1.8: board features

What the **Waveshare ESP32-S3-Touch-AMOLED-1.8** can do, collected from
Waveshare's documentation, the vendor examples and libraries in
`WaveshareHelloWorld/vendor`, and the sketches in this workspace. This
workspace uses the **V2** board (CO5300 panel, CST820 touch). V1 (SH8601,
FT3168) is discontinued, and Waveshare switched shipments to V2 on
May 30, 2026. The label on the back of the board shows the version.

The **Status** column shows how far each feature has been taken here:

- **Used**: a workspace sketch exercises it on the real board.
- **Example**: a vendor example or library covers it; no workspace sketch yet.
- **Spec only**: on the datasheet or spec sheet, not tried here.

## At a glance

| Area | Hardware | Status | Where to look |
| --- | --- | --- | --- |
| Processor | ESP32-S3R8, dual-core Xtensa LX7, 240 MHz | Used | Every S3 sketch |
| Memory | 512 KB SRAM, 8 MB octal PSRAM, 16 MB flash | Used | WaveshareHelloWorld board page |
| Display | 1.8" AMOLED, 368 x 448, CO5300 over QSPI | Used | wireframe_cube, PieMenuS3, level |
| Touch | CST820 capacitive, I2C 0x15 | Used | PieMenuS3, level, WaveshareHelloWorld |
| Motion | QMI8658 6-axis accelerometer + gyroscope, I2C 0x6B | Used | level, WaveshareHelloWorld |
| Clock | PCF85063A real-time clock, I2C 0x51 | Used | WaveshareHelloWorld (NTP to RTC) |
| Power | AXP2101 PMU, Li-ion charger, fuel gauge, I2C 0x34 | Used | WaveshareHelloWorld battery page |
| Audio | ES8311 codec, I2C 0x18 + I2S, mic, speaker amp | Used | WaveshareHelloWorld speaker/mic pages |
| Storage | microSD slot | Used | WaveshareHelloWorld microSD page |
| Buttons | BOOT (GPIO0), PWR (through AXP2101) | Used | WaveshareHelloWorld buttons page |
| Wi-Fi | 2.4 GHz 802.11 b/g/n, onboard antenna | Used | WaveshareHelloWorld (NTP, HTTPS WTXT) |
| Bluetooth | Bluetooth 5 LE (no Classic) | Used | WaveshareHelloWorld BLE page |
| USB | USB-C, native USB (CDC serial) | Used | All builds (`CDCOnBoot=cdc`) |
| I/O expander | XCA9554 / TCA9554, I2C 0x20 | Used | Reset sequence in every S3 sketch |
| Expansion | 7 GPIO, I2C, UART and USB pads at 1.27 mm | Spec only | Waveshare pinout image |

## Processor and memory

- **ESP32-S3R8**: two Xtensa LX7 cores at up to 240 MHz. The cores have
  vector instructions, which Espressif's ESP-DL and ESP-SR libraries use for
  on-device neural networks and speech recognition. *Spec only.*
- **512 KB SRAM + 384 KB ROM** on chip. A typical LVGL sketch here
  (level) uses about 150 KB of static RAM and leaves about 170 KB of heap.
- **8 MB octal PSRAM**, stacked in the package. Enable it with
  `PSRAM=opi`. WaveshareHelloWorld puts a full-screen 368 x 448 frame buffer
  (330 KB) and audio recordings in PSRAM.
- **16 MB SPI NOR flash**. The workspace partition scheme is
  `app3M_fat9M_16MB`: 3 MB for the sketch, 9 MB FAT file system, plus NVS.
- **NVS (Preferences)** for small persistent settings. level stores its
  per-orientation zeros there; WaveshareHelloWorld stores its "RTC holds UTC"
  marker.
- Other ESP32-S3 features (*Spec only*): hardware AES/SHA/RSA/HMAC,
  random-number generator, secure boot and flash encryption, an internal
  temperature sensor, LEDC PWM, RMT, TWAI (CAN), a ULP coprocessor, and light
  and deep sleep. Peripherals that need pins are limited to the free pads (see
  [Expansion](#expansion-pads)).

## Display

- **1.8-inch AMOLED, 368 x 448 portrait, 16.7 M colors.** Sketches here drive
  it in RGB565 (16-bit).
- **Driver:** CO5300 on V2, SH8601 on V1, both over 4-bit **QSPI**: CS 12,
  SCLK 11, D0-D3 4/5/6/7. Use `Arduino_CO5300` from GFX_Library_for_Arduino.
  The panel reset goes through the I/O expander.
- **Brightness** is a panel command (`setBrightness(0-255)`), not a PWM
  backlight. Black pixels are truly off, so dark interfaces save battery.
- **Redraw windows must start on an even pixel and have even sizes.** The LVGL
  sketches round every invalidated area in an `LV_EVENT_INVALIDATE_AREA`
  handler.
- **Measured speed (PieMenuS3):** a full-frame QSPI push takes about 43 ms. A
  full LVGL frame takes 180-220 ms to render and push. Redrawing only what
  changed is much faster: a pie slice in 65-106 ms, and level's bubble moves
  smoothly.
- **Graphics stacks used here:**
  - Arduino_GFX direct drawing: wireframe_cube.
  - Arduino_GFX with a PSRAM canvas: WaveshareHelloWorld.
  - LVGL 9.6 from `Libraries/lvgl`, with a 40-row partial buffer: PieMenuS3,
    level.
  - The vendor folder also ships LVGL 8.4 with its own `lv_conf.h`, used by
    the vendor examples.

## Touch

- **CST820 capacitive touch** (V2) at I2C **0x15**, interrupt on **GPIO21**.
  Its registers match the CST816 family: register 0x02 holds the finger count
  and 12-bit X/Y. V1 used an FT3168.
- Single touch with coordinates. The workspace builds gestures in software:
  `SwipeGesture.h` (swipes, taps, long press, with native tests) and LVGL's
  click and long-press events.
- Write 0xFF to register 0xFE to turn off auto-sleep, or the first tap after
  idle can be lost.
- The vendor's Arduino_DriveBus library also offers an interrupt-driven
  CST816x driver (`02_Drawing_board`). *Example.*

## Motion sensor (QMI8658)

- **6-axis IMU**: 3-axis accelerometer (±2/4/8/16 g) and 3-axis gyroscope
  (±16 to ±1024 °/s), I2C **0x6B**, through SensorLib's
  `SensorQMI8658.hpp`. It is one chip, not two.
- **Axes on this board** (worked out in level): +x points to the top of the
  screen, and +z points *into* the glass, so lying face up it reads z = -1 g.
  The vendor's auto-rotate demo uses the same x/y sign convention.
- **Accuracy seen here:** about 46 mg zero offset and a reading of 1.15 g at
  rest. Angles cancel the gain error. Zeroing removes the offset.
- **Used here:**
  - Tilt angles and spirit level: level.
  - Live accelerometer and gyroscope pages: WaveshareHelloWorld.
  - Screen auto-rotation: vendor `13_LVGL_Widgets`.
- **Chip features covered by SensorLib examples** (*Example*):
  - Tap and double-tap detection.
  - Pedometer (step counter).
  - Any-motion, no-motion and significant-motion detection.
  - Wake-on-motion.
  - FIFO buffering.
  - Hardware-synchronized (locked) sampling.
  - Gyroscope self-calibration.
  - Madgwick AHRS orientation (roll, pitch, yaw).
  - A die temperature sensor.
- The QMI8658 has two interrupt outputs, but Waveshare doesn't document
  whether they reach an ESP32 pin. The examples here poll.

## Real-time clock (PCF85063A)

- I2C **0x51**, through SensorLib's `SensorPCF85063.hpp`. *Used:*
  WaveshareHelloWorld writes UTC to the RTC after NTP sync and restores the
  system clock from it at boot when offline.
- The AXP2101 powers it from the main battery. Reserved pads accept a separate
  backup cell, so the time survives a battery swap.
- **Alarms** (by second, minute, hour, day, weekday) and a programmable
  **clock output**: SensorLib `PCF85063_AlarmByUnits` and
  `PCF85063_ClockOutput`. *Example.*
- `isClockIntegrityGuaranteed()` reports whether the oscillator stopped, for
  example after the battery went flat.

## Power (AXP2101)

- I2C **0x34**, through XPowersLib (`XPowersLib.h`, `XPOWERS_CHIP_AXP2101`).
  The V2 library set borrows XPowersLib from the V1 set.
- **3.7 V Li-ion battery** on an MX1.25 2-pin connector, charged from USB-C.
- **Used here:** battery present, battery voltage, battery percent (fuel
  gauge), USB (VBUS) voltage, charging status, and the PWR key short-press and
  long-press interrupts.
- **More in XPowersLib** (*Example*, see vendor `12_LVGL_AXP2101_ADC_Data`):
  system voltage, die temperature, charge current and target voltage, low
  battery warnings, and switching the individual DC-DC and LDO rails on or
  off.
- **Battery life, by Waveshare's figures:** about 1 hour at full brightness,
  3-4 hours with the screen off. Their highest AXP2101 temperature reading was
  46 °C.
- A long PWR press is a hardware power-off. Applications can't override it.

## Audio (ES8311)

- **ES8311 mono codec**: control over I2C **0x18**, samples over I2S. The
  ESP32-S3's I2S is driven through `ESP_I2S.h`, and the codec through the
  vendor driver packaged as `WaveshareES8311`.
- **Pins:** MCLK 16, BCLK 9, WS 45, ESP32 out to codec on 8, codec to ESP32 on
  10. GPIO46 enables the speaker amplifier; keep it LOW when silent.
- **Onboard microphone** (analog, digitized by the ES8311) and **onboard
  speaker**.
- **Used here:** tone playback, live mic level, and 16 kHz record-and-playback
  into PSRAM (WaveshareHelloWorld). The vendor `15_ES8311` plays a clip, then
  echoes the mic to the speaker.
- With the ESP32-S3's vector instructions, Espressif's ESP-SR (wake word and
  command recognition) is possible. *Spec only.*

## Storage

- **microSD slot.** The workspace uses `SD_MMC` in 1-bit mode: CLK 2,
  CMD 1, D0 3. Waveshare's wiki describes the same wires as SPI (SCK 2,
  MOSI 1, MISO 3) with chip select on expander pin 7. *Used:* mount, list,
  and a write/read/delete round trip (WaveshareHelloWorld).
- **9 MB FAT partition** in onboard flash, available to FFat. *Spec only.*
- GPIO3 is SD data. ESP32-C3 sketches used GPIO3 to enable the display; don't
  carry that over.

## Buttons

- **BOOT (GPIO0)**: hold it at power-on for download mode. Otherwise it's a
  normal active-low input (`INPUT_PULLUP`).
- **PWR**: wired to the AXP2101 power key. Read it through PMU interrupts
  (`isPekeyShortPressIrq()`, `isPekeyLongPressIrq()`). Waveshare's wiki also
  lists it on expander pin 4. A short press can be used by the application;
  a long press powers off.

## Wireless

- **Wi-Fi 2.4 GHz** (b/g/n), station and access point. *Used:*
  WaveshareHelloWorld connects in the background, syncs NTP and makes
  verified HTTPS requests (WTXT). The workspace's `4_x` Wi-Fi examples (AP,
  STA, SmartConfig, TCP, UDP) target ESP32-C3 but use the same core APIs.
  `06_GFX_ESPWiFiAnalyzer` scans networks. *Example.*
- **Bluetooth 5 LE**: advertising, GATT server, scanning. *Used:*
  WaveshareHelloWorld advertises a service and scans. The S3 has no Bluetooth
  Classic, so no SPP or A2DP audio. Coded PHY (long range) and 2M PHY are part
  of the chip's BLE 5 support. *Spec only.*
- **ESP-NOW** peer-to-peer packets work with the same radio. *Spec only.*

## USB

- **USB-C** on the S3's native USB, used for flashing and for serial
  (`CDCOnBoot=cdc`, appearing as COM5 here). No USB-UART chip, so the port
  re-enumerates on reset, and a sketch that crashes early can hide the port.
  Hold BOOT while plugging in to recover.
- The native USB can also be a HID keyboard or mouse, MIDI device or mass
  storage device through TinyUSB in the ESP32 core. *Spec only.*

## I/O expander

XCA9554 / TCA9554 at I2C **0x20**, driven with `Adafruit_XCA9554`.

| Pin | Use | Source |
| --- | --- | --- |
| 0-2 | Panel, touch and peripheral reset lines; pulse low for 20 ms at boot | Every S3 sketch here |
| 4 | PWR button | Waveshare wiki |
| 6 | Output, reset at boot by the V1 `16_LVGL_Sqprj` demo; purpose undocumented | Vendor example |
| 7 | microSD chip select (SPI mode) | Waveshare wiki |

## Expansion pads

Waveshare brings out **7 GPIOs, 1 I2C, 1 UART and 1 USB** on 1.27 mm pads.
The pad-to-GPIO mapping is published only as a picture on the product page,
so check it before wiring.

The I2C pads share the onboard bus (SDA 15, SCL 14); avoid addresses 0x15,
0x18, 0x20, 0x34, 0x51 and 0x6B. The UART is probably UART0 (GPIO43/44).

From the pins the board already uses, the free GPIOs must come from
13, 17, 18, 38-42, 47 and 48. GPIO26-37 belong to the flash and octal PSRAM.

## I2C bus

One bus, SDA **15**, SCL **14**, shared by every onboard chip. The sketches
set `Wire.setTimeOut(25)` so a stuck device can't freeze the UI.

| Address | Chip | Function |
| --- | --- | --- |
| 0x15 | CST820 | Touch |
| 0x18 | ES8311 | Audio codec control |
| 0x20 | XCA9554 | I/O expander |
| 0x34 | AXP2101 | Power management |
| 0x51 | PCF85063A | Real-time clock |
| 0x6B | QMI8658 | Accelerometer + gyroscope |

## GPIO map

| GPIO | Use |
| --- | --- |
| 0 | BOOT button |
| 1, 2, 3 | microSD CMD, CLK, D0 |
| 4, 5, 6, 7 | Display QSPI D0-D3 |
| 8, 10 | I2S data to codec, from codec |
| 9, 16, 45 | I2S BCLK, MCLK, WS |
| 11, 12 | Display QSPI SCLK, CS |
| 14, 15 | I2C SCL, SDA |
| 19, 20 | USB D-, D+ |
| 21 | Touch interrupt |
| 46 | Speaker amplifier enable (keep LOW when silent) |

The pin names come from `pin_config.h` in
`WaveshareHelloWorld/vendor/<revision>/libraries/Mylibrary`.

## Software

- **Frameworks:** Arduino (arduino-esp32 3.3.12 here) and ESP-IDF; Waveshare
  ships 16 Arduino and 6 ESP-IDF demos. MicroPython is mentioned in
  Waveshare's docs but not supported directly.
- **Board settings:** `esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`.
  Each S3 project's `Build.ps1` passes them along with the vendor libraries.
- **Libraries:** GFX_Library_for_Arduino, Adafruit_XCA9554, Adafruit_BusIO,
  Mylibrary (`pin_config.h`), SensorLib, XPowersLib, WaveshareES8311 and
  Arduino_DriveBus from the vendor folder, plus LVGL 9.6 from `Libraries/lvgl`.
  Pass each vendor library to arduino-cli separately, so the vendor folder's
  LVGL 8.4 never shadows LVGL 9.6.

## Gotchas found in this workspace

- **Draw buffer alignment:** align LVGL draw buffers to `LV_DRAW_BUF_ALIGN`.
  A misaligned buffer is silently rejected and the panel stays blank.
- **Even redraw windows:** round redraw areas to even coordinates and sizes
  for the CO5300.
- **Reset order:** pulse expander pins 0-2 before starting the panel. Wait
  about 200 ms before probing touch.
- **Matching library set:** use the vendor library set and `BOARD_AMOLED_V2`
  that match the board revision.
- **IMU startup:** ignore the first IMU samples or anything far from 1 g; the
  first read after power-up can be garbage.
- **Upload port:** close Serial Monitor before uploading; the native USB port
  is exclusive.

## Sources

- Waveshare product docs: <https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8>
- Waveshare wiki: <https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8>
- Vendor repository (commit 78e13f8): <https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8>
- [WaveshareHelloWorld/FEATURES.md](WaveshareHelloWorld/FEATURES.md),
  [PieMenuS3/README.md](PieMenuS3/README.md), [level/README.md](level/README.md)
- SensorLib and XPowersLib headers and examples under `WaveshareHelloWorld/vendor/v2/libraries`
