#include "WtxtClient.h"
#include "RootCA.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cJSON.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
QueueHandle_t mailbox = nullptr;
TaskHandle_t worker = nullptr;
std::atomic<bool> busy{false};
class MessageBuffer : public Stream {
public:
  String body; bool tooLarge = false;
  size_t write(uint8_t byte) override { return write(&byte,1); }
  size_t write(const uint8_t* data,size_t count) override {
    if (count > 4096-body.length()) { tooLarge=true; return 0; }
    return body.concat((const char*)data,count) ? count : 0;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
bool accept(const String& body,WtxtMessage& result) {
  cJSON* json=cJSON_ParseWithOpts(body.c_str(),nullptr,true);
  if (!json) return false;
  auto value=cJSON_GetObjectItemCaseSensitive(json,"value");
  auto stamp=cJSON_GetObjectItemCaseSensitive(json,"receivedAt");
  bool valid=cJSON_IsObject(json) && cJSON_IsString(value) && cJSON_IsString(stamp);
  if (valid) {
    String t(stamp->valuestring);
    valid=strlen(value->valuestring)<=512 && t.length()>=20 && t.length()<=64 &&
      t[4]=='-' && t[7]=='-' && t[10]=='T' && t[13]==':' && t[16]==':';
    if (valid) {
      int zone=19;
      while (zone<(int)t.length() && (t[zone]=='.' || isDigit(t[zone]))) ++zone;
      String z=t.substring(zone);
      valid=z=="Z" || (z.length()==6 && (z[0]=='+' || z[0]=='-') && z[3]==':');
    }
    if (valid) {
      strlcpy(result.value,value->valuestring,sizeof(result.value));
      strlcpy(result.timestamp,stamp->valuestring,sizeof(result.timestamp));
      result.hasValue=true; result.updatedMs=millis();
    }
  }
  cJSON_Delete(json); return valid;
}
void run(void*) {
  WtxtMessage result;
  for (;;) {
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    strlcpy(result.status,"Fetching...",sizeof(result.status));
    xQueueOverwrite(mailbox,&result);
    WiFiClientSecure client; client.setCACert(ROOT_CA); client.setHandshakeTimeout(8);
    HTTPClient http; http.setConnectTimeout(4000); http.setTimeout(4000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    const char* status="Request setup failed";
    if (WiFi.status()!=WL_CONNECTED) status="Wi-Fi offline";
    else if (http.begin(client,"https://wtxt.azurewebsites.net/value")) {
      int code=http.GET();
      if (code==HTTP_CODE_OK) {
        int expected=http.getSize(); MessageBuffer buffer;
        if (expected>4096) status="Response too large";
        else {
          int count=http.writeToStream(&buffer);
          if (buffer.tooLarge) status="Response too large";
          else if (count<0 || (expected>=0 && buffer.body.length()!=(size_t)expected)) status="Incomplete response";
          else status=accept(buffer.body,result) ? "Updated" : "Invalid message JSON";
        }
      } else {
        snprintf(result.status,sizeof(result.status),"%s %d",code>0 ? "HTTP" : "Request error",code);
        status=nullptr;
      }
    }
    http.end();
    if (status) strlcpy(result.status,status,sizeof(result.status));
    xQueueOverwrite(mailbox,&result); busy.store(false);
  }
}
}
void beginWtxt() {
  if (mailbox) return;
  mailbox=xQueueCreate(1,sizeof(WtxtMessage));
  if (mailbox) {
    WtxtMessage initial; xQueueOverwrite(mailbox,&initial);
    if (xTaskCreate(run,"wtxt",16384,nullptr,1,&worker)!=pdPASS) worker=nullptr;
  }
}
void requestWtxt() {
  beginWtxt();
  if (worker && !busy.exchange(true)) xTaskNotifyGive(worker);
}
void readWtxt(WtxtMessage& result) {
  if (mailbox) xQueuePeek(mailbox,&result,0);
  if (mailbox && !worker) strlcpy(result.status,"Network task unavailable",sizeof(result.status));
}
bool wtxtBusy() { return busy.load(); }
