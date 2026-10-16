# S3 swipeable feature tests

A 13-page test app for the Waveshare ESP32-S3-Touch-AMOLED-1.8 V2.
Swipe left for the next page and right for the previous page; navigation wraps.
You can also tap the bottom arrows. Tap the outlined buttons to run a test.

| Page | What to try |
| --- | --- |
| Board check | Check driver status, PSRAM, free RAM, and uptime |
| Clock + RTC | Analog/digital Eastern clock; request NTP sync; compare hardware RTC time |
| Wi-Fi | Check IP, signal strength and connection; tap reconnect |
| WTXT client | Read the service message; tap refresh; send text using sibling wtxt/wtxt.ps1 |
| Touch | Tap, drag, swipe; inspect coordinates and counters |
| Accelerometer | Tilt; lying still should show a vector magnitude near 1 g |
| Gyroscope | Rotate; motion should change readings; still readings should be near zero |
| Speaker | Tap for a 0.7-second 440 Hz tone at 35% volume |
| Microphone | Speak and watch the level; record two seconds, then play it back |
| Buttons | Press BOOT and briefly press PWR; check separate counters |
| Battery + power | Read battery/USB voltage and charging; cycle display brightness |
| microSD | Mount and inspect a card, then test temporary-file write/read/delete |
| Bluetooth LE | Advertise as S3 Feature Test and scan for nearby BLE devices |

## Build and upload

```powershell
.\Build-HelloScreen.ps1
.\Build-HelloScreen.ps1 -Upload -Port COM5
```

V2 and COM5 are defaults. The original panel remains selectable with
`-Revision original`; this device uses V2. Open WaveshareHelloWorld.ino in the
IDE to see the HelloScreen.ino entry tab, FeatureTests.cpp, AudioDemo.cpp and
WtxtClient.cpp. Use the PowerShell script to resolve the project-local libraries.

## Wi-Fi and clock

The local WiFiCredentials.h was copied from WatchFaceRTC without displaying its
contents. It is excluded by this project's .gitignore. The network reconnect and
background NTP approach comes from WatchFaceRTC/WatchFace.ino. TestConfig.h uses
the same Eastern daylight-saving rules and NTP servers.

No waiting loop blocks startup while Wi-Fi connects. The clock shows a waiting
status until time is available. After NTP sync, the hardware RTC is set to UTC.
An NVS marker identifies this convention; on subsequent boots a valid marked RTC
can restore system time offline. RTC values written by older apps are not assumed
to be UTC. Battery/backup power must keep the RTC powered for retention.

## WTXT

The page uses https://wtxt.azurewebsites.net/value, the existing RootCA.h trust
anchor, bounded 4096-byte responses and ESP32's cJSON parser. TLS verification
stays enabled and requests wait for NTP synchronization. Fetching runs in a
separate task so HTTPS timeouts do not stop swipes, drawing or audio updates.
Polling occurs every 20 seconds while the page is open; a request already started
may finish after you leave. Errors preserve the last message and receivedAt time.

The C3 client's small/medium/large and red/green/blue/cyan/magenta/yellow/white
message directives remain supported. Recognized tags are removed, text wraps,
long messages scroll, and color directives draw a rounded border. This app uses
the GFX bitmap font at sizes 2/3/4. Unknown tags remain visible. Timestamp timezone
is shown as supplied by the service (UTC when its offset is zero).

## Audio and navigation

Microphone capture uses ES8311 and I2S stereo 16-bit samples at 16 kHz. Recordings
stay in PSRAM; no audio is uploaded. The amplifier stays off during capture.
Playback is limited to the recorded samples. Leaving an audio page stops output
and recording. DMA reads/writes use zero timeout to keep navigation responsive.

Swipes require at least 65 pixels of horizontal travel and must be predominantly
horizontal. Each contact changes page once; a swipe cannot also trigger a test
button. Small movement followed by release counts as a tap. The V2 peripheral
reset and CST820-compatible touch driver follow the manufacturer's drawing demo.

SD operations run only when tapped and may briefly pause navigation while a card
mounts. The round-trip test creates a uniquely named /s3-test-*.txt file and removes
only that file after reading it. Existing files are not modified. Mounting alone
reads the root directory. PWR long holds retain the board's power-off behavior.

## Serial diagnostics

Serial Monitor: 921600 baud. A bounded status line every five seconds reports
page, connection, NTP, touch, memory, IMU samples, audio and WTXT request status.
Commands: n or > = next page; p or < = previous; t = current page's lower test
button; r = record on the microphone page. These provide navigation if touch is
unavailable. Credentials and WTXT message contents are not printed.

See [FEATURES.md](FEATURES.md) for drivers/pins and [BUILD.md](BUILD.md) for build
options. Vendor provenance and licenses remain in vendor/SOURCE.md and vendor/.

## Verification

The feature app compiled and was uploaded to COM5. Startup diagnostics confirmed
all five peripheral drivers initialized. Wi-Fi connected, NTP synchronized, WTXT
returned Updated, IMU data changed with motion, and microphone sample data was
received. Swipe recognition has native automated tests in tests/SwipeGestureTests.cpp.
Listening to playback and checking button, BLE, SD, battery and RTC retention
behavior are physical tests available on the corresponding pages.
