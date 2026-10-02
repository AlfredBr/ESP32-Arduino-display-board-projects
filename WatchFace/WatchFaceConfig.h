#pragma once

// Reference-photo mode: clock starts here on every power-up/reset.
// This is a running demonstration, not network-synchronized local time.
constexpr int START_YEAR = 2026;
constexpr int START_MONTH = 9;
constexpr int START_DAY = 29;
constexpr int START_HOUR = 21;
constexpr int START_MINUTE = 44;
constexpr int START_SECOND = 6;

// Display sample only: the board has no heart-rate sensor configured.
constexpr int SAMPLE_HEART_RATE = 57;
constexpr int SCREEN_ROTATION = 0;  // 0, 1, 2, or 3.
