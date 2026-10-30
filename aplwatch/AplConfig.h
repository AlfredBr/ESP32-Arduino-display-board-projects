#pragma once
#include <stdint.h>
// Same Eastern timezone and background NTP service as WatchFaceRTC.
constexpr char TIME_ZONE[] = "EST5EDT,M3.2.0/2,M11.1.0/2";
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr bool USE_24_HOUR = true;            // Digital face: 15:07 rather than 3:07.
constexpr uint32_t LIGHT_REFRESH_S = 10;      // Re-render the light fan as the hands move.
constexpr uint32_t FRAME_MS = 33;             // Seconds-hand frame interval (~30 fps).
constexpr uint8_t BRIGHTNESS = 160;           // Panel brightness, 0-255.
