#include "FeatureTests.h"
#include "SwipeGesture.h"
#include "BoardDisplay.h"
#include "AudioDemo.h"
#include "WtxtClient.h"
#include "TestConfig.h"
#include "WiFiCredentials.h"
#include <Arduino_DriveBus_Library.h>
#include <Adafruit_XCA9554.h>
#include <SensorQMI8658.hpp>
#include <SensorPCF85063.hpp>
#include <XPowersLib.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <SD_MMC.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <atomic>
#include <vector>

namespace {
enum Page { HOME, CLOCK, WIFI, WTXT, TOUCH, ACCEL, GYRO, SPEAKER, MICROPHONE, BUTTONS, POWER, STORAGE, BLUETOOTH, PAGE_COUNT };
const char* titles[] = {"Board check","Clock + RTC","Wi-Fi","WTXT client","Touch","Accelerometer","Gyroscope","Speaker","Microphone","Buttons","Battery + power","microSD","Bluetooth LE"};
constexpr uint16_t BG=0x0000, WHITE=0xFFFF, CYAN=0x07FF, GREEN=0x07E0, DIM=0x8410, RED=0xF800, GOLD=0xFFE0;
BoardDisplay display;
class PsramCanvas : public Arduino_Canvas {
public:
  PsramCanvas(Arduino_G* output) : Arduino_Canvas(LCD_WIDTH,LCD_HEIGHT,output) {}
  bool begin(int32_t speed=GFX_NOT_DEFINED) override {
    (void)speed;
    _framebuffer=(uint16_t*)heap_caps_malloc(LCD_WIDTH*LCD_HEIGHT*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return _framebuffer!=nullptr;
  }
};
PsramCanvas frame(&display.graphics());
Arduino_GFX* g=&display.graphics();
bool frameReady=false, displayReady=false, touchReady=false, imuReady=false, rtcReady=false, pmuReady=false;
SensorQMI8658 imu; SensorPCF85063 rtc; XPowersPMU pmu; Adafruit_XCA9554 expander;
AudioDemo audio;
std::shared_ptr<Arduino_IIC_DriveBus> touchBus=std::make_shared<Arduino_HWIIC>(IIC_SDA,IIC_SCL,&Wire);
#if BOARD_AMOLED_V2
Arduino_CST816x touch(touchBus,CST816T_DEVICE_ADDRESS);
#else
Arduino_FT3x68 touch(touchBus,FT3168_DEVICE_ADDRESS);
#endif
Page page=HOME;
uint32_t lastDraw=0,lastTouchPoll=0,lastSensorPoll=0,lastPmuPoll=0,lastWifiAttempt=0,lastWtxtFetch=0;
bool dirty=true, wifiConfigured=false, wasConnected=false, ntpStarted=false, ntpSynced=false, clockFromRtc=false;
std::atomic<bool> ntpEvent{false};
Preferences prefs;
float ax=0,ay=0,az=0,gx=0,gy=0,gz=0;
bool accValid=false,gyroValid=false;
uint32_t lastAcc=0,lastGyro=0;
int touchX=-1,touchY=-1,startX=0,startY=0;
bool contact=false;
SwipeGesture swipe;
uint32_t contactStart=0,lastContact=0,taps=0,swipes=0,touchErrors=0;
int maxDx=0,maxDy=0;
const char* gesture="Touch the screen";
bool bootRaw=false,bootDown=false; uint32_t bootChanged=0,bootCount=0,pwrShort=0,pwrLong=0;
unsigned brightness=160;
String sdStatus="Tap to mount a card"; bool sdMounted=false; uint32_t sdFiles=0; uint64_t sdSizeMB=0;
bool bleReady=false,bleAdvertising=false;
BLEServer* bleServer=nullptr; BLEScan* bleScan=nullptr;
std::atomic<bool> bleScanning{false}; std::atomic<int> bleCount{-1};
WtxtMessage wtxt;
String displayedWtxt;
std::vector<String> wtxtLines;
int wtxtSize=2; bool wtxtBorder=false; uint16_t wtxtColor=WHITE;
size_t wtxtFirstLine=0; uint32_t nextWtxtScroll=0;

void text(const char* value,int y,int size=2,uint16_t color=WHITE,bool center=false) {
  g->setTextSize(size); g->setTextColor(color); g->setTextWrap(false);
  int x=24;
  if (center) { int16_t x1,y1; uint16_t w,h; g->getTextBounds(value,0,0,&x1,&y1,&w,&h); x=(LCD_WIDTH-w)/2; }
  g->setCursor(x,y); g->print(value);
}
void line(int y,const char* fmt,...) {
  char buffer[80]; va_list args; va_start(args,fmt); vsnprintf(buffer,sizeof(buffer),fmt,args); va_end(args);
  text(buffer,y);
}
void action(const char* label,int y=354) {
  g->fillRoundRect(20,y,328,46,10,0x1926);
  g->drawRoundRect(20,y,328,46,10,CYAN);
  text(label,y+15,2,CYAN,true);
}
void indicator(const char* label,bool ok,int y) {
  g->fillCircle(30,y+7,5,ok?GREEN:RED); g->setCursor(46,y); g->setTextColor(WHITE); g->setTextSize(2); g->print(label);
  g->setCursor(280,y); g->setTextColor(ok?GREEN:RED); g->print(ok?"OK":"FAIL");
}
void switchPage(int direction) {
  audio.stop();
  page=(Page)((page+direction+PAGE_COUNT)%PAGE_COUNT);
  if (page!=BLUETOOTH && bleReady) {
    bleScan->stop(); bleScanning.store(false);
    BLEDevice::getAdvertising()->stop(); bleAdvertising=false;
  }
  if (page==MICROPHONE) audio.begin();
  dirty=true;
}
void bleCompleted(BLEScanResults result) { bleCount.store(result.getCount()); bleScanning.store(false); }
void startBle() {
  if (!bleReady) {
    BLEDevice::init("S3 Feature Test");
    bleServer=BLEDevice::createServer();
    BLEService* service=bleServer->createService("ceeb0001-34c0-4f34-8933-394d69981234");
    auto characteristic=service->createCharacteristic("ceeb0002-34c0-4f34-8933-394d69981234",BLECharacteristic::PROPERTY_READ);
    characteristic->setValue("ESP32-S3 feature test"); service->start();
    BLEDevice::getAdvertising()->addServiceUUID(service->getUUID());
    bleScan=BLEDevice::getScan(); bleScan->setActiveScan(true); bleScan->setInterval(100); bleScan->setWindow(80);
    bleReady=true;
  }
  if (bleScanning.load()) return;
  BLEDevice::getAdvertising()->start(); bleAdvertising=true;
  bleScan->clearResults(); bleScanning.store(true); bleCount.store(-1);
  if (!bleScan->start(4,bleCompleted)) bleScanning.store(false);
}
void testSD() {
  if (!sdMounted) {
    SD_MMC.end(); SD_MMC.setPins(SDMMC_CLK,SDMMC_CMD,SDMMC_DATA);
    sdMounted=SD_MMC.begin("/sdcard",true) && SD_MMC.cardType()!=CARD_NONE;
    if (!sdMounted) { sdStatus="No card / mount failed"; return; }
    sdSizeMB=SD_MMC.cardSize()/(1024*1024); sdFiles=0;
    File root=SD_MMC.open("/");
    if (!root) { sdStatus="Mounted; root read failed"; return; }
    for (File f=root.openNextFile(); f; f=root.openNextFile()) { ++sdFiles; f.close(); if(sdFiles>=1000)break; }
    root.close(); sdStatus="Mounted; root readable";
  } else {
    String path="/s3-test-"+String(millis())+".txt";
    if (SD_MMC.exists(path)) { sdStatus="Test name exists; retry"; return; }
    const char* payload="ESP32-S3 SD round-trip\n";
    File file=SD_MMC.open(path,FILE_WRITE);
    if (!file) { sdStatus="Cannot create test file"; return; }
    bool ok=file.print(payload)==strlen(payload); file.close();
    file=SD_MMC.open(path,FILE_READ);
    String readback=file?file.readString():""; if(file)file.close();
    ok=ok && readback==payload;
    bool removed=SD_MMC.remove(path);
    sdStatus=ok?(removed?"Write/read/delete PASS":"Read PASS; delete failed"):"SD round-trip FAILED";
  }
}
void tap(int x,int y) {
  if (y>=410) { if(x<100)switchPage(-1); else if(x>268)switchPage(1); return; }
  if (page==MICROPHONE) {
    if(y>=290 && y<338 && !audio.capturing && !audio.playing) audio.record();
    else if(y>=354 && y<=400 && !audio.capturing && !audio.playing) audio.playback();
  } else if (y>=354 && y<=400) {
    switch(page) {
      case CLOCK: if(WiFi.status()==WL_CONNECTED)configTzTime(TIME_ZONE,NTP_SERVER_1,NTP_SERVER_2); break;
      case WIFI: if(wifiConfigured){WiFi.reconnect();lastWifiAttempt=millis();} break;
      case WTXT: if(WiFi.status()==WL_CONNECTED && ntpSynced){requestWtxt();lastWtxtFetch=millis();} break;
      case TOUCH: taps=0;swipes=0;touchErrors=0;break;
      case SPEAKER: audio.tone();break;
      case BUTTONS: bootCount=pwrShort=pwrLong=0;break;
      case POWER: brightness=brightness==80?160:brightness==160?240:80; display.setBrightness(brightness);break;
      case STORAGE: sdStatus="Testing card..."; dirty=true; testSD();break;
      case BLUETOOTH: startBle();break;
      default: break;
    }
  }
  dirty=true;
}
void pollTouch() {
  uint32_t now=millis(); if(!touchReady || now-lastTouchPoll<10)return; lastTouchPoll=now;
  int fingers=touch.IIC_Read_Device_Value(touch.Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER);
  if(fingers<0){++touchErrors;return;}
  if(fingers>0) {
    int x=touch.IIC_Read_Device_Value(touch.Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    int y=touch.IIC_Read_Device_Value(touch.Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
    if(x<0 || x>=LCD_WIDTH || y<0 || y>=LCD_HEIGHT){++touchErrors;return;}
    auto event=swipe.sample(true,x,y,now);
    touchX=x;touchY=y;contact=swipe.down;
    if(event==SwipeGesture::NEXT || event==SwipeGesture::PREVIOUS) {
      ++swipes;gesture=event==SwipeGesture::NEXT?"Swipe left":"Swipe right";
      switchPage(event==SwipeGesture::NEXT?1:-1);
    }
  } else {
    auto event=swipe.sample(false,0,0,now);contact=swipe.down;
    if(event==SwipeGesture::TAP){++taps;gesture="Tap";tap(touchX,touchY);}
  }
}
void pollButtons() {
  uint32_t now=millis();bool down=digitalRead(0)==LOW;
  if(down!=bootRaw){bootRaw=down;bootChanged=now;}
  if(now-bootChanged>=30 && bootDown!=bootRaw){bootDown=bootRaw;if(bootDown)++bootCount;dirty=true;}
  if(pmuReady && now-lastPmuPoll>=100) {
    lastPmuPoll=now;pmu.getIrqStatus();
    if(pmu.isPekeyShortPressIrq()){++pwrShort;dirty=true;}
    if(pmu.isPekeyLongPressIrq()){++pwrLong;dirty=true;}
    pmu.clearIrqStatus();
  }
}
void updateNetwork() {
  if(wifiConfigured) {
    bool connected=WiFi.status()==WL_CONNECTED;
    if(connected && !wasConnected) {configTzTime(TIME_ZONE,NTP_SERVER_1,NTP_SERVER_2);ntpStarted=true;dirty=true;}
    if(!connected && millis()-lastWifiAttempt>=WIFI_RETRY_MS){lastWifiAttempt=millis();WiFi.reconnect();}
    wasConnected=connected;
  }
  if(ntpEvent.exchange(false)) {
    ntpSynced=true;clockFromRtc=false;
    if(rtcReady) {
      time_t epoch=time(nullptr); tm utc{};gmtime_r(&epoch,&utc);
      rtc.setDateTime(utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec);
      if(!prefs.getBool("rtcUTC",false))prefs.putBool("rtcUTC",true);
    }
    dirty=true;
  }
  if(page==WTXT && WiFi.status()==WL_CONNECTED && ntpSynced && !wtxtBusy() &&
     (!lastWtxtFetch || millis()-lastWtxtFetch>=20000)) {requestWtxt();lastWtxtFetch=millis();}
  readWtxt(wtxt);
}
void wrapWtxt() {
  if(displayedWtxt==wtxt.value && !wtxtLines.empty())return;
  displayedWtxt=wtxt.value; wtxtLines.clear(); wtxtSize=2;wtxtBorder=false;wtxtColor=WHITE;
  String plain;
  for(size_t i=0;i<displayedWtxt.length();) {
    if(displayedWtxt[i]=='<') {
      int end=displayedWtxt.indexOf('>',i+1);
      if(end>=0) {
        String tag=displayedWtxt.substring(i+1,end);tag.trim();if(tag.endsWith("/")){tag.remove(tag.length()-1);tag.trim();}tag.toLowerCase();
        bool recognized=true;
        if(tag=="small")wtxtSize=2;else if(tag=="medium")wtxtSize=3;else if(tag=="large")wtxtSize=4;
        else {
          if(tag=="red")wtxtColor=RED;else if(tag=="green")wtxtColor=GREEN;else if(tag=="blue")wtxtColor=0x001F;
          else if(tag=="cyan")wtxtColor=CYAN;else if(tag=="magenta")wtxtColor=0xF81F;else if(tag=="yellow")wtxtColor=GOLD;
          else if(tag=="white")wtxtColor=WHITE;else recognized=false;
          if(recognized)wtxtBorder=true;
        }
        if(recognized){i=end+1;continue;}
      }
    }
    plain+=displayedWtxt[i++];
  }
  // Fixed-width GFX bitmap font: wrap words, then split overlong words.
  unsigned columns=320/(6*wtxtSize);String current,word;
  auto append=[&]() {
    if(word.isEmpty())return;
    if(!current.isEmpty() && current.length()+word.length()+1>columns){wtxtLines.push_back(current);current="";}
    if(!current.isEmpty())current+=' ';
    for(size_t i=0;i<word.length();++i){if(current.length()>=columns){wtxtLines.push_back(current);current="";}current+=word[i];}
    word="";
  };
  for(size_t i=0;i<plain.length();++i) {
    char c=plain[i];if(c=='\r')continue;
    if(c==' ' || c=='\t' || c=='\n'){append();if(c=='\n'){wtxtLines.push_back(current);current="";}}
    else word+=c;
  }
  append();wtxtLines.push_back(current);wtxtFirstLine=0;nextWtxtScroll=millis()+4000;
}
void drawClock() {
  time_t epoch=time(nullptr);bool valid=epoch>=1704067200;tm local{};if(valid)localtime_r(&epoch,&local);
  int cx=184,cy=188;float radius=96;
  g->drawCircle(cx,cy,radius,DIM);
  for(int tick=0;tick<60;++tick) {
    float a=tick*2*PI/60;int r0=tick%5?92:85;
    g->drawLine(cx+sin(a)*r0,cy-cos(a)*r0,cx+sin(a)*radius,cy-cos(a)*radius,tick%5?DIM:CYAN);
  }
  if(valid) {
    auto hand=[&](float fraction,int length,uint16_t color){float a=fraction*2*PI;g->drawLine(cx,cy,cx+sin(a)*length,cy-cos(a)*length,color);};
    hand((local.tm_hour%12+local.tm_min/60.f)/12,48,WHITE);
    hand((local.tm_min+local.tm_sec/60.f)/60,75,WHITE);hand(local.tm_sec/60.f,86,GREEN);
    g->fillCircle(cx,cy,4,CYAN);
    char t[40];strftime(t,sizeof(t),"%H:%M:%S",&local);text(t,294,3,WHITE,true);
    strftime(t,sizeof(t),"%a %b %d  %Z",&local);text(t,328,2,CYAN,true);
  } else text("Waiting for time",310,2,GOLD,true);
  text(ntpSynced?"NTP synced; RTC UTC":clockFromRtc?"Offline RTC time":"Waiting for NTP",66,2,DIM,true);
  if (rtcReady) { auto r=rtc.getDateTime(); char value[40]; snprintf(value,sizeof(value),"RTC %02d:%02d:%02d %s",r.getHour(),r.getMinute(),r.getSecond(),prefs.getBool("rtcUTC",false)?"UTC":"raw"); text(value,86,1,DIM,true); }
  action("Sync internet time");
}
void drawWtxt() {
  if(!wtxt.hasValue)text("Waiting for text",148,3,WHITE,true);
  else {
    wrapWtxt();int visible=236/(8*wtxtSize+4);
    size_t maxStart=wtxtLines.size()>(size_t)visible?wtxtLines.size()-visible:0;
    if(maxStart && (int32_t)(millis()-nextWtxtScroll)>=0){wtxtFirstLine=wtxtFirstLine<maxStart?wtxtFirstLine+1:0;nextWtxtScroll=millis()+((wtxtFirstLine==0 || wtxtFirstLine==maxStart)?4000:2000);}
    if(wtxtBorder)g->drawRoundRect(12,68,344,242,14,wtxtColor);
    for(int i=0;i<visible && wtxtFirstLine+i<wtxtLines.size();++i)text(wtxtLines[wtxtFirstLine+i].c_str(),80+i*(8*wtxtSize+4),wtxtSize,WHITE,true);
    String timestamp=String(wtxt.timestamp).substring(0,10)+" "+String(wtxt.timestamp).substring(11,19);
    text(timestamp.c_str(),314,1,DIM,true);
    String stamp(wtxt.timestamp);int z=19;while(z<(int)stamp.length()&&(stamp[z]=='.'||isDigit(stamp[z])))++z;
    String zone=stamp.substring(z);if(zone=="Z"||zone=="+00:00")zone="UTC";
    text(zone.c_str(),329,1,DIM,true);
  }
  const char* status=WiFi.status()!=WL_CONNECTED?"Wi-Fi offline":!ntpSynced?"Waiting for NTP":wtxt.status;
  text(status,66,1,GOLD,true);action(wtxtBusy()?"Fetching...":"Refresh text");
}
void drawSensor(bool acceleration) {
  bool valid=acceleration?accValid:gyroValid;uint32_t age=millis()-(acceleration?lastAcc:lastGyro);
  text(!imuReady?"IMU not found":!valid?"Waiting for sample":age>1000?"Samples stale":"Live samples",76,2,valid&&age<=1000?GREEN:RED);
  float values[]={acceleration?ax:gx,acceleration?ay:gy,acceleration?az:gz};
  uint16_t colors[]={RED,GREEN,CYAN};
  for(int i=0;i<3;++i) {
    int y=128+i*65;line(y,"%c %8.3f %s",'X'+i,values[i],acceleration?"g":"dps");
    g->drawFastHLine(34,y+35,300,DIM);g->drawFastVLine(184,y+28,15,DIM);
    int bar=constrain((int)(values[i]*(acceleration?60:2)),-148,148);
    g->fillRect(bar<0?184+bar:184,y+31,abs(bar),8,colors[i]);
  }
  text(acceleration?"Tilt; rest flat = ~1 g":"Rotate; still = ~0 dps",344,1,DIM);
  text("X red   Y green   Z cyan",374,1,DIM);
}
void drawPage() {
  if(!displayReady)return;
  g->fillScreen(BG);g->drawFastHLine(20,51,328,DIM);
  text(titles[page],22,2,CYAN,true);
  switch(page) {
    case HOME:
      indicator("Display",displayReady,78);indicator("Touch",touchReady,108);indicator("IMU",imuReady,138);
      indicator("RTC",rtcReady,168);indicator("Power chip",pmuReady,198);indicator("PSRAM",ESP.getPsramSize()>0,228);
      line(280,"Flash %u MB",ESP.getFlashChipSize()/(1024*1024));line(312,"RAM free %u KB",ESP.getFreeHeap()/1024);
      line(354,"Uptime %lu s",millis()/1000);text("Swipe to test every feature",389,1,DIM,true);break;
    case CLOCK:drawClock();break;
    case WIFI:
      text(!wifiConfigured?"Set WiFiCredentials.h":WiFi.status()==WL_CONNECTED?"Connected":"Connecting...",80,2,WiFi.status()==WL_CONNECTED?GREEN:GOLD);
      { String ssid=wifiConfigured?WIFI_SSID:"No SSID configured"; if(ssid.length()>26)ssid=ssid.substring(0,23)+"..."; text(ssid.c_str(),122,2,WHITE); }
      line(166,"IP %s",WiFi.localIP().toString().c_str());line(202,"RSSI %d dBm",WiFi.RSSI());
      line(238,"MAC %s",WiFi.macAddress().c_str());text(ntpSynced?"Internet time: synced":"Internet time: waiting",278,2,DIM);
      text("2.4 GHz; background reconnect",318,1,DIM);action("Reconnect Wi-Fi");break;
    case WTXT:drawWtxt();break;
    case TOUCH:
      text(touchReady?"Touch driver ready":"Touch driver FAILED",76,2,touchReady?GREEN:RED);
      line(120,"X %d  Y %d",touchX,touchY);line(158,"Taps %lu",taps);line(194,"Swipes %lu",swipes);
      line(230,"Read errors %lu",touchErrors);text(gesture,268,2,CYAN);
      if(contact) {g->drawCircle(touchX,constrain(touchY,70,330),10,GREEN);}
      text("Left/right swipe changes page",316,1,DIM);action("Reset touch counters");break;
    case ACCEL:drawSensor(true);break;
    case GYRO:drawSensor(false);break;
    case SPEAKER:
      text("ES8311 speaker test",92,2,WHITE);text("440 Hz / 0.7 seconds",140,2,CYAN);
      text("Volume 35%",188,2,WHITE);text(audio.status,240,1,GOLD);
      text("Tap and listen for a tone",300,1,DIM);action("Play test tone");break;
    case MICROPHONE:
      { bool receiving=audio.ready && audio.lastInputMs && millis()-audio.lastInputMs<1000; text(receiving?"Live microphone":audio.ready?"Waiting for samples":"Audio unavailable",76,2,receiving?GREEN:GOLD); }
      line(118,"RMS %u  Peak %u",audio.rms,audio.peak);
      g->drawRoundRect(24,158,320,30,4,DIM);g->fillRect(26,160,min(316,(int)(audio.rms*316/8000)),26,GREEN);
      text("Speak; the level should rise",204,1,DIM);line(236,"Recorded %.2f s",audio.recordingBytes()/(float)(AUDIO_RATE*4));
      text(audio.status,267,1,GOLD);action(audio.capturing?"Recording...":"Record 2 seconds",292);
      action(audio.playing?"Playing...":"Play recording");break;
    case BUTTONS:
      text("Press the side buttons",80,2,WHITE);line(132,"BOOT %s",bootDown?"DOWN":"UP");line(174,"BOOT presses %lu",bootCount);
      line(218,"PWR short %lu",pwrShort);line(260,"PWR long %lu",pwrLong);
      text("PWR long hold can turn off",311,1,GOLD);action("Reset button counters");break;
    case POWER:
      if(!pmuReady)text("AXP2101 not found",80,2,RED);
      else {
        bool battery=pmu.isBatteryConnect();text(battery?"Battery connected":"No battery detected",80,2,battery?GREEN:GOLD);
        line(126,"Battery %u mV",pmu.getBattVoltage());line(166,"Charge %d %%",pmu.getBatteryPercent());
        line(206,"USB input %u mV",pmu.getVbusVoltage());text(pmu.isCharging()?"Charging":"Not charging",250,2,WHITE);
      }
      line(302,"Brightness %u / 255",brightness);action("Cycle brightness");break;
    case STORAGE:
      text(sdMounted?"Card mounted":"Card not mounted",80,2,sdMounted?GREEN:GOLD);line(128,"Capacity %llu MB",sdSizeMB);
      line(174,"Root entries %lu",sdFiles);text(sdStatus.c_str(),224,1,GOLD);
      text("Test creates a temporary file",282,1,DIM);text("Reads it back, then deletes it",304,1,DIM);
      action(sdMounted?"Write / read / delete":"Mount and inspect card");break;
    case BLUETOOTH:
      text(bleAdvertising?"Advertising":"BLE idle",80,2,bleAdvertising?GREEN:DIM);text("Name: S3 Feature Test",124,2,WHITE);
      text(bleScanning.load()?"Scanning nearby devices...":"4-second active scan",168,1,CYAN);
      line(214,"Devices found %d",bleCount.load());line(258,"Connections %u",bleServer?bleServer->getConnectedCount():0);
      text("Find us with a phone BLE app",314,1,DIM);action("Advertise + scan BLE");break;
    default:break;
  }
  char count[24];snprintf(count,sizeof(count),"%d / %d",page+1,PAGE_COUNT);
  text("<",420,2,CYAN);g->setCursor(332,420);g->print(">");text(count,421,1,DIM,true);
  if(frameReady)frame.flush();lastDraw=millis();dirty=false;
}
}

void beginFeatureTests() {
  Serial.begin(921600);Serial.setTxTimeoutMs(0);pinMode(0,INPUT_PULLUP);pinMode(PA,OUTPUT);digitalWrite(PA,LOW);
  Wire.begin(IIC_SDA,IIC_SCL);Wire.setTimeOut(25);
#if BOARD_AMOLED_V2
  // Manufacturer V2 touch example resets board peripherals through XCA9554.
  if(expander.begin(0x20)) {
    for(int i=0;i<3;++i){expander.pinMode(i,OUTPUT);expander.digitalWrite(i,LOW);}delay(20);
    for(int i=0;i<3;++i)expander.digitalWrite(i,HIGH);delay(30);
  }
#endif
  displayReady=display.begin();
  rtcReady=rtc.begin(Wire,IIC_SDA,IIC_SCL);imuReady=imu.begin(Wire,QMI8658_L_SLAVE_ADDRESS,IIC_SDA,IIC_SCL);
  pmuReady=pmu.begin(Wire,AXP2101_SLAVE_ADDRESS,IIC_SDA,IIC_SCL);
  if(imuReady) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,SensorQMI8658::ACC_ODR_125Hz,SensorQMI8658::LPF_MODE_0);
    imu.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS,SensorQMI8658::GYR_ODR_112_1Hz,SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();imu.enableGyroscope();
  }
  if(pmuReady) {
    pmu.enableBattVoltageMeasure();pmu.enableVbusVoltageMeasure();
    pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ|XPOWERS_AXP2101_PKEY_LONG_IRQ);pmu.clearIrqStatus();
  }
  touchReady=touch.begin();
  if(touchReady)touch.IIC_Write_Device_State(touch.Arduino_IIC_Touch::Device::TOUCH_DEVICE_INTERRUPT_MODE,touch.Arduino_IIC_Touch::Device_Mode::TOUCH_DEVICE_INTERRUPT_PERIODIC);
  setenv("TZ","UTC0",1);tzset();prefs.begin("s3-feature",false);
  // Only seed system time from a clock previously saved by this app in UTC.
  if(rtcReady && prefs.getBool("rtcUTC",false) && rtc.isClockIntegrityGuaranteed()) {
    auto date=rtc.getDateTime();tm utc{};utc.tm_year=date.getYear()-1900;utc.tm_mon=date.getMonth()-1;utc.tm_mday=date.getDay();
    utc.tm_hour=date.getHour();utc.tm_min=date.getMinute();utc.tm_sec=date.getSecond();
    time_t epoch=mktime(&utc);
    if(epoch>=1704067200 && date.getMonth()>=1 && date.getMonth()<=12 && date.getDay()>=1 && date.getDay()<=31) {
      timeval tv{epoch,0};settimeofday(&tv,nullptr);clockFromRtc=true;
    }
  }
  setenv("TZ",TIME_ZONE,1);tzset();
  sntp_set_time_sync_notification_cb([](timeval*){ntpEvent.store(true);});
  wifiConfigured=WIFI_SSID[0] && strcmp(WIFI_SSID,"YOUR_WIFI_NAME")!=0;
  if(wifiConfigured){WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);lastWifiAttempt=millis();}
  if(displayReady){frameReady=frame.begin();if(frameReady)g=&frame;}
  drawPage();
  if(Serial)Serial.printf("Feature pages ready. Display=%d Touch=%d IMU=%d RTC=%d PMU=%d\n",displayReady,touchReady,imuReady,rtcReady,pmuReady);
}
void updateFeatureTests() {
  pollTouch();pollButtons();updateNetwork();
  if(Serial.available()){char c=Serial.read();if(c=='>' || c=='n')switchPage(1);else if(c=='<' || c=='p')switchPage(-1);else if(c=='t')tap(184,377);else if(c=='r' && page==MICROPHONE)tap(184,315);}
  uint32_t now=millis();
  if(imuReady && now-lastSensorPoll>=20) {
    lastSensorPoll=now;
    if(imu.getDataReady()) {
      if(imu.getAccelerometer(ax,ay,az)){accValid=true;lastAcc=now;}
      if(imu.getGyroscope(gx,gy,gz)){gyroValid=true;lastGyro=now;}
    }
  }
  audio.tick();
  uint32_t interval=(page==ACCEL || page==GYRO || page==TOUCH || page==MICROPHONE)?150:500;
  if(dirty || now-lastDraw>=interval)drawPage();
  static uint32_t lastLog=0;
  if(now-lastLog>=5000 && Serial && Serial.availableForWrite()>=256) {
    lastLog=now;Serial.printf("PAGE=%d %s WIFI=%d NTP=%d Touch=%d Heap=%u Accel=%.2f,%.2f,%.2f Gyro=%.2f,%.2f,%.2f Audio=%d RMS=%u Recorded=%u WTXT=%s\n",page,titles[page],WiFi.status()==WL_CONNECTED,ntpSynced,touchReady,ESP.getFreeHeap(),ax,ay,az,gx,gy,gz,audio.ready,audio.rms,(unsigned)audio.recordingBytes(),wtxt.status);
  }
  delay(2);
}





