#pragma once
#include <stdint.h>

// America/New_York: automatic Eastern Standard / Daylight Time.
// POSIX TZ notation uses the opposite sign from a UTC offset.
constexpr char TIME_ZONE[] = "EST5EDT,M3.2.0/2,M11.1.0/2";
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr uint32_t WIFI_RETRY_MS = 30000;

// Display sample only: the board has no heart-rate sensor configured.
constexpr int SAMPLE_HEART_RATE = 57;
constexpr int SCREEN_ROTATION = 0;  // 0, 1, 2, or 3.
