# WTXT

Displays the JSON `value` from https://wtxt.azurewebsites.net/value
on the same ESP32-C3 / round 240 x 240 GC9A01 used by the sibling apps.
Fetches immediately after Wi-Fi and time synchronization, then every 10 seconds.
The message's `receivedAt` date and time appear at the bottom, with the service's
timezone (UTC for the current endpoint). Fractional seconds are omitted.
Network or JSON errors keep the last successful message and its timestamp,
with an error status above the timestamp.

Expected response: `{"value":"Hello from PowerShell","length":21,"receivedAt":"2026-10-01T17:06:35.3389307+00:00"}`.
The `length` metadata is not displayed. JSON parsing uses ESP32's bundled cJSON.

Uses LovyanGFX with SCLK 6, MOSI 7, DC 2, CS 10 and display enable GPIO 3.
WiFiCredentials.h was copied from Breitling; edit it if your network changes.
RootCA.h contains the endpoint's DigiCert Global Root G2 trust anchor;
HTTPS certificates are verified using NTP-synchronized time.

Optional directives in the message are removed from the displayed text:

- `<red/>`, `<green/>`, `<blue/>`, `<cyan/>`, `<magenta/>`, `<yellow/>`,
  and `<white/>` draw a five-pixel ring around the screen in that color.
- `<small/>`, `<medium/>`, and `<large/>` select FreeSans 9-, 12-, and 18-point
  fonts respectively (approximately 18, 24, and 36 pixels).

For example, `<red/><large/>Hello !!!` displays `Hello !!!` in large white
text with a red ring. Plain tags such as `<red><large>` also work. Recognized
tags are case-insensitive and can occur anywhere; the last color and size
directives apply to the entire message. Unknown tags remain visible.
Every message starts with small text and no ring unless directives override them.

Text uses native FreeSans glyphs, with word wrapping measured in pixels.
The visible line count adjusts to fit the selected font. Longer messages
automatically scroll upward one line every two seconds, pausing for four seconds
at the beginning and end before repeating. An unchanged network refresh keeps
the scrolling position; new text restarts from the beginning. The built-in font
is intended for ASCII text. Accepts JSON responses up to 4096 bytes and message
text up to 512 bytes. Chunked responses and responses without Content-Length
are supported through a bounded response buffer.

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

The build defaults to ESP32C3 Dev Module with USB CDC enabled. Serial logging
uses 115200 baud. Change REFRESH_MS in wtxt.ino to adjust polling.
