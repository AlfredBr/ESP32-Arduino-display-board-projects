# Swipeable test app

The current app implements 13 feature pages. See [README.md](README.md) for navigation, tests, Wi-Fi/RTC behavior, and the WTXT client.

# Board features and libraries

| Feature | Driver/API | Local example or usage |
| --- | --- | --- |
| AMOLED | Arduino_GFX, SH8601 original / CO5300 V2 | HelloScreen; 01_HelloWorld |
| Capacitive touch | Arduino_DriveBus, FT3x68 original / CST816x-family API for CST820 V2 | 02_Drawing_board and revision-specific touch examples |
| Accelerometer + gyroscope | SensorLib: SensorQMI8658.hpp | HelloScreen; QMI8658 UI example |
| RTC | SensorLib: SensorPCF85063.hpp | HelloScreen; PCF85063 simpleTime example |
| Microphone + speaker | WaveshareES8311: es8311.h; ESP_I2S.h from ESP32 core | ES8311 example plays audio, then microphone-to-speaker echo |
| Battery, charging, PWR button | XPowersLib: XPowersLib.h, AXP2101 | Original 12_LVGL_AXP2101_ADC_Data; PMU initialization in HelloScreen |
| BOOT button | ESP32 core GPIO, active-low GPIO0 | pinMode(0, INPUT_PULLUP); digitalRead(0) == LOW; debounce in application |
| Wi-Fi | ESP32 core WiFi.h | Original 06_GFX_ESPWiFiAnalyzer; WiFi.begin(ssid, password) for connection |
| Bluetooth LE | ESP32 core BLEDevice.h | Core BLE examples; ESP32-S3 supports BLE, not Bluetooth Classic SPP |
| microSD | ESP32 core SD_MMC.h / FS.h | SD_Test example; CLK=2 CMD=1 DATA=3, 1-bit mode |
| UI widgets | Bundled LVGL 8.4 and lv_conf.h | Revision-specific LVGL examples |
| I2C / GPIO / UART | ESP32 core Wire.h / Arduino.h / HardwareSerial | Shared I2C SDA=15 SCL=14 |

The IMU contains both accelerometer and gyroscope; they are not separate chips.
Wi-Fi uses the local ignored WiFiCredentials.h copied from WatchFaceRTC.
The feature app writes UTC to the hardware RTC after NTP synchronization. RTC.begin itself does not set the clock. Use rtc.setDateTime(year, month, day, hour,
minute, second) deliberately; upstream time examples set a fixed demonstration
date on startup. Battery power is required for time retention without USB.

BOOT is a GPIO input. PWR is the AXP2101 power key: read PMU interrupt status
using getIrqStatus(), isPekeyShortPressIrq()/isPekeyLongPressIrq(), then clearIrqStatus(),
and enable the corresponding key IRQ masks when configuring an application.
It is not interchangeable with BOOT and a long press can switch power off.

Audio uses ES8311 control over shared I2C and ESP_I2S for samples:
MCLK=16, BCLK=9, WS=45, ESP32 output=8, ESP32 input=10, amplifier enable=46.
Follow the supplied ES8311 sketch for clock configuration, codec initialization,
I2S setup and capture/playback. Its local es8311.c/.h files are also installed as
WaveshareES8311 for reuse. The feature app starts Wi-Fi in the background and activates audio on its demo pages.

Display QSPI: CS=12, SCLK=11, D0=4, D1=5, D2=6, D3=7. Touch interrupt=21.
Do not carry over the old ESP32-C3 GPIO3 display-enable operation: GPIO3 is SD data.
Use pin_config.h from the selected revision's Mylibrary folder.

Supporting Adafruit_BusIO and Adafruit_XCA9554 libraries are also included.
Manufacturer UI helper libraries and licenses are preserved with each set.
Use the selected revision consistently for examples and libraries.

Source: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8
Hardware: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8




