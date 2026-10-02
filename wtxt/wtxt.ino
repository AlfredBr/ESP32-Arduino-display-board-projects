#include "BoardDisplay.h"
#include "WiFiCredentials.h"
#include "RootCA.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <vector>
#include <cJSON.h>

BoardDisplay screen;
const char* VALUE_URL = "https://wtxt.azurewebsites.net/value";
constexpr uint32_t REFRESH_MS = 20000;
constexpr uint32_t WIFI_RETRY_MS = 30000;
String lastValue;
String lastStatus;
String lastTimestamp;
String timestampDate;
String timestampTime;
bool hasValue = false;
bool wasConnected = false;
bool fetched = false;
uint32_t lastFetch = 0;
uint32_t lastRetry = 0;
constexpr int TEXT_WIDTH = 172;
int visibleLines = 6;
int messageSize = 0;
bool showRing = false;
uint16_t ringColor = TFT_WHITE;
constexpr uint32_t SCROLL_STEP_MS = 2000;
constexpr uint32_t SCROLL_PAUSE_MS = 4000;
std::vector<String> textLines;
String displayedValue;
size_t firstLine = 0;
uint32_t nextScroll = 0;

void selectMessageFont() {
  switch (messageSize) {
    case 1: screen.setFont(&fonts::FreeSans12pt7b); break;
    case 2: screen.setFont(&fonts::FreeSans18pt7b); break;
    default: screen.setFont(&fonts::FreeSans9pt7b); break;
  }
}

String parseDirectives(const String& value) {
  messageSize = 0;
  showRing = false;
  ringColor = TFT_WHITE;
  String text;
  for (size_t i = 0; i < value.length();) {
    if (value[i] == '<') {
      const int end = value.indexOf('>', i + 1);
      if (end >= 0) {
        String tag = value.substring(i + 1, end);
        tag.trim();
        if (tag.endsWith("/")) { tag.remove(tag.length() - 1); tag.trim(); }
        tag.toLowerCase();
        bool recognized = true;
        if (tag == "small") messageSize = 0;
        else if (tag == "medium") messageSize = 1;
        else if (tag == "large") messageSize = 2;
        else {
          if (tag == "red") ringColor = TFT_RED;
          else if (tag == "green") ringColor = TFT_GREEN;
          else if (tag == "blue") ringColor = TFT_BLUE;
          else if (tag == "cyan") ringColor = TFT_CYAN;
          else if (tag == "magenta") ringColor = TFT_MAGENTA;
          else if (tag == "yellow") ringColor = TFT_YELLOW;
          else if (tag == "white") ringColor = TFT_WHITE;
          else recognized = false;
          if (recognized) showRing = true;
        }
        if (recognized) { i = end + 1; continue; }
      }
    }
    text += value[i++];
  }
  return text;
}

// HTTPClient decodes chunked responses into this bounded buffer.
class MessageBuffer : public Stream {
public:
  String body;
  bool tooLarge = false;
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    if (size > 4096 - body.length()) { tooLarge = true; return 0; }
    return body.concat(reinterpret_cast<const char*>(data), size) ? size : 0;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

void wrapText(const String& value) {
  textLines.clear();
  String line;
  String word;
  // Add a word, splitting oversized words at measured character widths.
  auto addWord = [&]() {
    if (word.isEmpty()) return;
    String candidate = line.isEmpty() ? word : line + " " + word;
    if (!line.isEmpty() && screen.textWidth(candidate) > TEXT_WIDTH) {
      textLines.push_back(line);
      line = "";
    }
    if (!line.isEmpty()) line += " ";
    for (size_t i = 0; i < word.length(); ++i) {
      if (!line.isEmpty() && screen.textWidth(line + word[i]) > TEXT_WIDTH) {
        textLines.push_back(line);
        line = "";
      }
      line += word[i];
    }
    word = "";
  };
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '\r') continue;
    if (c == ' ' || c == '\t' || c == '\n') {
      addWord();
      if (c == '\n') {
        textLines.push_back(line);
        line = "";
      }
    } else {
      word += c;
    }
  }
  addWord();
  textLines.push_back(line);
}

void drawText(const String& value, const String& status) {
  screen.fillScreen(TFT_BLACK);
  screen.setTextColor(TFT_CYAN, TFT_BLACK);
  screen.setTextSize(1);
  screen.setFont(&fonts::Font2);
  screen.setTextDatum(middle_center);
  screen.drawString("WTXT", 120, 30);

  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  if (textLines.empty() || value != displayedValue) {
    displayedValue = value;
    const String text = parseDirectives(value);
    selectMessageFont();
    // Keep glyphs within the 132-pixel message area at every size.
    visibleLines = 132 / screen.fontHeight();
    if (visibleLines < 1) visibleLines = 1;
    wrapText(text);
    firstLine = 0;
    nextScroll = millis() + SCROLL_PAUSE_MS;
  }
  selectMessageFont();
  if (showRing) {
    for (int radius = 115; radius <= 119; ++radius) screen.drawCircle(120, 120, radius, ringColor);
  }
  const int count = textLines.size() < static_cast<size_t>(visibleLines) ? textLines.size() : visibleLines;
  const int lineHeight = screen.fontHeight();
  const int top = 110 - (count - 1) * lineHeight / 2;
  for (int i = 0; i < count; ++i) screen.drawString(textLines[firstLine + i], 120, top + i * lineHeight);
  screen.setFont(&fonts::Font2);
  screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (hasValue) {
    if (status != "Updated") screen.drawString(status, 120, 184);
    screen.drawString(timestampDate, 120, 200);
    // Keep the timezone visible even when the API supplies an offset.
    const int width = screen.textWidth(timestampTime);
    if (width > 100) screen.setTextSize(100.0f / width);
    screen.drawString(timestampTime, 120, 216);
    screen.setTextSize(1);
  } else {
    screen.drawString(status, 120, 210);
  }
}

bool acceptMessage(const String& body) {
  cJSON* json = cJSON_ParseWithOpts(body.c_str(), nullptr, true);
  if (!json) return false;
  const cJSON* valueField = cJSON_GetObjectItemCaseSensitive(json, "value");
  const cJSON* timeField = cJSON_GetObjectItemCaseSensitive(json, "receivedAt");
  const bool valid = cJSON_IsObject(json) && cJSON_IsString(valueField) &&
                     cJSON_IsString(timeField);
  if (!valid) { cJSON_Delete(json); return false; }
  const String value(valueField->valuestring);
  const String timestamp(timeField->valuestring);
  cJSON_Delete(json);
  if (value.length() > 512 || timestamp.length() < 20 || timestamp.length() > 64 ||
      timestamp[4] != '-' || timestamp[7] != '-' || timestamp[10] != 'T' ||
      timestamp[13] != ':' || timestamp[16] != ':') return false;
  // Ignore fractional seconds, but retain the service's timezone.
  int zoneStart = 19;
  while (zoneStart < static_cast<int>(timestamp.length()) &&
         (timestamp[zoneStart] == '.' || isDigit(timestamp[zoneStart]))) ++zoneStart;
  String zone = timestamp.substring(zoneStart);
  if (zone == "Z" || zone == "+00:00") zone = "UTC";
  else if (zone.length() != 6 || (zone[0] != '+' && zone[0] != '-') || zone[3] != ':') return false;
  if (!hasValue || value != lastValue || timestamp != lastTimestamp || lastStatus != "Updated") {
    lastValue = value;
    lastTimestamp = timestamp;
    timestampDate = timestamp.substring(0, 10);
    timestampTime = timestamp.substring(11, 19) + " " + zone;
    hasValue = true;
    lastStatus = "Updated";
    drawText(lastValue, lastStatus);
    Serial.println(lastValue);
    Serial.println(lastTimestamp);
  }
  return true;
}

void updateScroll() {
  if (textLines.size() <= static_cast<size_t>(visibleLines) ||
      static_cast<int32_t>(millis() - nextScroll) < 0) return;
  const size_t lastStart = textLines.size() - visibleLines;
  firstLine = firstLine < lastStart ? firstLine + 1 : 0;
  drawText(displayedValue, lastStatus);
  nextScroll = millis() + ((firstLine == 0 || firstLine == lastStart)
                         ? SCROLL_PAUSE_MS : SCROLL_STEP_MS);
}

void showStatus(const String& status) {
  if (status == lastStatus) return;
  lastStatus = status;
  drawText(hasValue ? lastValue : "Waiting for text", status);
  Serial.println(status);
}

void fetchValue() {
  WiFiClientSecure client;
  client.setCACert(ROOT_CA);
  client.setHandshakeTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, VALUE_URL)) {
    showStatus("Request setup failed");
    return;
  }
  const int code = http.GET();
  if (code == HTTP_CODE_OK) {
    // Allow room for JSON escaping and metadata around a 512-byte message.
    const int expectedSize = http.getSize();
    if (expectedSize > 4096) {
      showStatus("Unexpected response size");
    } else {
      MessageBuffer response;
      const int bytesRead = http.writeToStream(&response);
      if (response.tooLarge) {
        showStatus("Response too large");
      } else if (bytesRead < 0 ||
                 (expectedSize >= 0 && response.body.length() != static_cast<size_t>(expectedSize))) {
        showStatus("Incomplete response");
      } else {
        if (!acceptMessage(response.body)) showStatus("Invalid message JSON");
      }
    }
  } else {
    showStatus(code > 0 ? "HTTP " + String(code) : "Request failed");
    Serial.println(HTTPClient::errorToString(code));
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  screen.init();
  screen.setRotation(0);
  showStatus("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Certificate validation requires the current time.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    wasConnected = false;
    showStatus("Connecting to Wi-Fi");
    if (now - lastRetry >= WIFI_RETRY_MS) {
      lastRetry = now;
      WiFi.reconnect();
    }
  } else if (time(nullptr) < 1700000000) {
    showStatus("Synchronizing time");
  } else {
    if (!wasConnected) { fetched = false; wasConnected = true; }
    if (!fetched || now - lastFetch >= REFRESH_MS) {
      fetched = true;
      lastFetch = now;
      fetchValue();
    }
  }
  updateScroll();
  delay(20);
}
