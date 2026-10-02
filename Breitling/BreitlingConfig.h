#pragma once
#include <stdint.h>
// Same Eastern timezone and background NTP service as WatchFaceRTC.
constexpr char TIME_ZONE[]="EST5EDT,M3.2.0/2,M11.1.0/2";
constexpr char NTP_SERVER_1[]="pool.ntp.org";
constexpr char NTP_SERVER_2[]="time.nist.gov";
constexpr uint32_t WIFI_RETRY_MS=30000;
constexpr int SCREEN_ROTATION=0;
