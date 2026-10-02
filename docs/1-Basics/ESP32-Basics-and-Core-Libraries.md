# ESP32 basics and core libraries

Continue with [Graphics libraries](ESP32-Graphics-Libraries.md) after this guide. Inspected September 30, 2026: Arduino-ESP32 **3.3.12** is installed locally. Examples target Arduino C++ and core 3.x.

## 1. Understand the platform

An ESP32 is a microcontroller. Your program runs on the board, reads inputs, controls outputs, and communicates with peripherals and networks. Flash stores firmware; RAM stores working data. Reset starts the program again.

ESP32 is a family, not one interchangeable board. Original ESP32, C3, S2, and S3 have different pins and features. Your HelloScreen project targets ESP32-C3: it is single-core and supports Bluetooth LE, not Classic Bluetooth. S2 has no Bluetooth. Check your board's pinout before copying GPIO numbers from a tutorial.

Arduino-ESP32 is Espressif's Arduino programming layer on top of ESP-IDF. It includes FreeRTOS, but you can start with ordinary setup() and loop() without creating tasks.

## 2. Set up, compile, and upload

1. Install **esp32 by Espressif Systems** through Arduino IDE Boards Manager.
2. Select your actual board and USB port. Use a USB data cable.
3. Make a sketch folder and matching file, such as SerialLesson/SerialLesson.ino.
4. Verify to compile, then Upload to transfer firmware.
5. Open Serial Monitor at the baud rate in Serial.begin().

HelloScreen specifies **ESP32C3 Dev Module**, with **USB CDC On Boot enabled**. These settings are for that board. Native USB can reconnect on another port after upload. Consult your board's BOOT/RESET sequence if uploading stalls.

This workspace's Libraries folder is used automatically when the Arduino directory is your configured sketchbook. Otherwise configure that location or install dependencies into your actual sketchbook. Compiler output shows which library copy is used.

## 3. First complete sketch

```cpp
#include <Arduino.h>
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Hello ESP32!");
}
void loop() {
  Serial.printf("Uptime: %lu seconds\n", millis() / 1000UL);
  delay(1000);
}
```

setup() runs once after reset. loop() repeats. Serial is an object; println() and printf() are its methods. Includes make declarations available. Arduino automatically includes Arduino.h in .ino files; ordinary .cpp files need it explicitly.

Use bool for true/false, int for integers, float for decimal measurements, and uint8_t/uint32_t for unsigned 8/32-bit values. const prevents modification; constexpr is useful for fixed configuration. Globals survive between loop calls; ordinary local variables last only during their function call. Most statements end in a semicolon.

## 4. Responsive timing

Long delay() calls stop your application logic from reading buttons or updating a screen. Other system tasks may still run. Use elapsed-time checks for concurrent activities:

```cpp
#include <Arduino.h>
unsigned long lastReport = 0;
void setup() { Serial.begin(115200); }
void loop() {
  unsigned long now = millis();
  if (now - lastReport >= 1000UL) {
    lastReport = now;
    Serial.println(now);
  }
  // Read buttons and update other state here.
  delay(1);
}
```

Unsigned subtraction remains valid when millis() wraps after roughly 49.7 days. Each periodic activity needs its own timestamp. This is the foundation for responsive sensors, networking, and animation.

## 5. Pins, buttons, ADC, and PWM

pinMode(pin, OUTPUT) configures an output. digitalWrite(pin, HIGH/LOW) sets it. INPUT_PULLUP enables an internal pull-up: a button between the pin and ground then reads LOW when pressed.

Choose a verified free GPIO from the schematic before using this fragment:

```cpp
// Fragment; BUTTON_PIN must be defined as your verified free GPIO.
// In setup():
pinMode(BUTTON_PIN, INPUT_PULLUP);
// In loop():
bool pressed = digitalRead(BUTTON_PIN) == LOW;
```

GPIO uses **3.3 V logic**; do not apply 5 V to inputs. Use a series resistor for a discrete LED. Motors and relays need suitable drivers and power supplies. Connect circuit grounds when appropriate. Avoid pins assigned to flash, PSRAM, USB, the display, and boot strapping. Some original ESP32 pins are input-only. Built-in LEDs may be inverted or addressable RGB devices.

Mechanical switches bounce. Debounce and detect a change of state before counting presses.

analogRead(pin) reads an ADC-capable input. Raw counts are not precise voltage; attenuation and calibration matter. analogReadMilliVolts(pin) gives a calibrated estimate where supported. On the original ESP32, ADC2 has Wi-Fi restrictions. Use documentation for your exact chip.

PWM rapidly switches a digital output to control average power; it is not a true analog voltage. Arduino-ESP32 3.x LEDC calls use pins:

```cpp
// Fragment: LED_PIN must be a verified output with a suitable LED circuit.
if (ledcAttach(LED_PIN, 5000, 8)) {
  ledcWrite(LED_PIN, 128); // 8-bit range 0..255, about 50% duty.
}
```

Older ledcSetup() and ledcAttachPin() calls were removed in core 3.0. See the [LEDC reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html) and [migration guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html).

## 6. Core library reference

These come with Arduino-ESP32 or the underlying platform, rather than the workspace's third-party Libraries folder.

| Header or API | Purpose | Learn first |
| --- | --- | --- |
| Arduino.h | Pins and timing | pinMode, digitalRead/Write, millis, delay |
| Serial / HardwareSerial.h | Debugging and UART peripherals | begin, available, read, print; select peripheral RX/TX pins |
| Wire.h | I2C devices | begin(sda, scl), device addresses, transactions |
| SPI.h | Displays, SD, fast peripherals | Clock, MOSI/MISO, chip select, transactions |
| WiFi.h | Wi-Fi station/access point | begin, status, localIP |
| NetworkClient.h | TCP connections | Connection lifecycle and timeouts |
| NetworkClientSecure.h | TLS connections | Certificates and clock validity |
| HTTPClient.h | Outgoing HTTP requests | begin, GET, status codes, end |
| WebServer.h | Simple local HTTP server | Routes, begin, frequent handleClient calls |
| Preferences.h | Small persistent settings in NVS | begin, typed get/put, end |
| FS.h / LittleFS.h / SD.h | Files in flash/on SD | Mount, open, read/write, close |
| ArduinoOTA.h / Update.h | Firmware updates | Service/update lifecycle and partition compatibility |
| ESPmDNS.h | Local .local hostname discovery | begin; depends on network mDNS support |
| BLEDevice.h | Built-in BLE stack API | Services, characteristics, advertising |
| BluetoothSerial.h | Classic Bluetooth serial | Original ESP32; unavailable on C3/S3/S2 |
| time.h | Network-synchronized system time | configTime, getLocalTime, timezones |
| Ticker.h | Periodic callbacks | Keep callbacks short and nonblocking |
| esp_sleep.h / FreeRTOS headers | Advanced power/scheduling | Wake sources, tasks, queues after fundamentals |

Use the [official core documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/) for details.

## 7. I2C and SPI

I2C uses SDA and SCL plus ground. Several peripherals can share the wires if addresses differ. Pull-ups must be appropriate for 3.3 V; many modules already include them. Wire.begin(sda, scl) selects pins explicitly. An I2C scanner helps find addresses, but detecting an address does not prove a driver is correct.

SPI uses SCLK, MOSI, MISO, and a separate CS per device. A write-only display can omit MISO and add DC and reset signals. Shared display/SD buses require proper transactions and coordinated ownership. Pins already assigned to your display are not free for new experiments.

## 8. Wi-Fi example

This complete sketch attempts a connection with a timeout. Replace credentials locally and avoid committing passwords.

```cpp
#include <Arduino.h>
#include <WiFi.h>
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000UL) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("Connection timed out");
  }
}
void loop() { delay(10); }
```

Common ESP32 boards including C3 use 2.4 GHz Wi-Fi. Match your chip's supported band. A real application should retry on a schedule while staying responsive. See the [Wi-Fi reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html).

Next try HTTPClient for outgoing requests or WebServer for a status page. Check errors, set timeouts, and call http.end() when finished. HTTPS requires an appropriate CA certificate and valid time; use a secure client. Long network calls can freeze an application UI, so keep them out of graphics callbacks.

## 9. Persistent settings

This complete sketch saves a boot counter in NVS:

```cpp
#include <Arduino.h>
#include <Preferences.h>
void setup() {
  Serial.begin(115200);
  Preferences settings;
  if (!settings.begin("lesson", false)) {
    Serial.println("Settings open failed"); return;
  }
  uint32_t boots = settings.getUInt("boots", 0) + 1;
  if (settings.putUInt("boots", boots) == 0) Serial.println("Save failed");
  settings.end();
  Serial.printf("Boot count: %lu\n", static_cast<unsigned long>(boots));
}
void loop() { delay(10); }
```

Preferences is good for brightness, modes, and calibration. Use a filesystem for larger files. Write only when values change; flash has finite write endurance. See the [Preferences reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html). Do not automatically format storage after a mount failure unless losing existing data is acceptable.

## 10. Memory and callbacks

Check allocations and avoid enormous local arrays that overflow a task stack. ESP.getFreeHeap() helps diagnose memory pressure. PSRAM is optional hardware, not guaranteed. Reuse buffers rather than repeatedly allocating them.

Keep interrupt handlers tiny: set a flag or notify a task. Do not print, draw, wait, or make network requests there. volatile does not make shared access thread-safe; tasks may need locks or queues. Learn normal operation before adding deep sleep, which wakes through reset-style startup and has chip-specific wake sources.

## 11. Ten third-party libraries worth knowing

This is a practical shortlist for your projects, not a measured popularity ranking. Only LovyanGFX and LVGL were found as library directories here. Other entries are recommendations, not newly installed dependencies. Official project links were checked September 30, 2026. Check compatibility with your chip and core before installation.

| # | Library | Why learn it? | Important detail |
| --- | --- | --- | --- |
| 1 | [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | TFT text, shapes, sprites, images. | Installed; needs correct board configuration. |
| 2 | [LVGL](https://github.com/lvgl/lvgl) | Widgets, menus, touch interfaces. | Installed v9.6.0; needs hardware integration and regular servicing. |
| 3 | [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | JSON APIs, configuration, MQTT payloads. | Match major-version examples; check parsing failures. |
| 4 | [PubSubClient](https://github.com/knolleary/pubsubclient) | MQTT home automation and telemetry. | Service loop() regularly; configure packet size; publishes QoS 0. |
| 5 | [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | BLE services and scanning. | BLE only; verify chip support and callback signatures. |
| 6 | [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) | Async web interfaces and WebSockets. | Use ESP32Async and compatible AsyncTCP; learn core WebServer first. |
| 7 | [FastLED](https://github.com/FastLED/FastLED) | Addressable LED animations. | Check chip/backend support; budget power and signal levels. |
| 8 | [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) | Consistent interfaces for compatible sensor drivers. | Also requires the actual sensor's driver. |
| 9 | [RTClib](https://github.com/adafruit/RTClib) | External RTC calendar time, such as DS3231. | Requires supported hardware; separate from ESP32 network time. |
| 10 | [Bounce2](https://github.com/thomasfredericks/Bounce2) | Debounced buttons and clean transitions. | Call update() frequently. |

## 12. Practice and troubleshooting

Progress through Serial output, a debounced button, PWM LED, I2C sensor, saved setting, Wi-Fi request, and finally a graphics display. Replace long delays with millis scheduling as you combine features.

Existing hello_world_serial, 4_2_Wifi_STA, and 4_1_Wifi_AP are useful reading; older code may need core 3.x migration.

| Symptom | First checks |
| --- | --- |
| Upload failure | Data cable, board, port, BOOT/RESET; other apps holding the port. |
| Empty/garbled Serial | Baud, USB CDC setting, port; reset after opening Monitor. |
| Undefined API | Header, spelling, core/library version; read first compiler error. |
| Random resets | Power, peripheral current, stack/heap, watchdog logs. |
| Missing sensor | Pins, ground, voltage, address, pull-ups. |
| Frozen UI | Blocking requests, connection loops, long delays. |

Examples have been reviewed, not compiled or tested on hardware. Pin fragments require your board-specific definitions.
