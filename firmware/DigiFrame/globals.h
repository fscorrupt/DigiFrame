/* DigiFrame — globals, cross-core command queue, background tasks, colors */
#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>

/**********************  3. GLOBALS  **********************************/
MatrixPanel_I2S_DMA *dma = nullptr;
AnimatedGIF gif;

/* ---- runtime config: seeded from config.h defines, overridden by
        /config.json on LittleFS (editable from the web dashboard) ---- */
String cfgWifiSsid   = WIFI_SSID;
String cfgWifiPass   = WIFI_PASS;
String ntpServer     = "pool.ntp.org";
/* weather location — fixed char buffers (not String) because the web
   handler (core 1) writes while weatherTask (core 0) reads; a short
   strlcpy race is harmless, a String heap realloc race is not. */
char cfgLat[16]      = LATITUDE;
char cfgLon[16]      = LONGITUDE;
int  tzOffsetSec     = TZ_OFFSET_SEC;   // UTC offset in seconds (runtime-configurable)
bool use24h          = (TIME_FORMAT == 24);
int  displayRotation = ROTATION;
int  cfgLang         = LANGUAGE;
/* ---- Home Assistant / MQTT (off by default; editable at runtime) ---- */
bool   mqttEnable = MQTT_ENABLE;
String mqttHost   = MQTT_HOST;
int    mqttPort   = MQTT_PORT;
String mqttUser   = MQTT_USER;
String mqttPass   = MQTT_PASS;
volatile bool mqttConfigDirty = false;  // set by web/BLE (core 1), applied by mqttTask (core 0)
volatile bool weatherNow   = false;  // web handler asks for an immediate refetch
bool          portalActive = false;  // setup hotspot + captive portal active
volatile bool wifiRetryNow = false;  // web handler asks for an immediate STA (re)connect
bool          swapColors   = false;  // swap G and B lines
volatile bool isNightMode  = false;  // updated once per second in loop()

uint8_t       cfgNightStart = 0;     // 0-23 (default 00:00)
uint8_t       cfgNightEnd   = 7;     // 0-23 (default 07:00)
uint8_t       cfgNightDays  = 127;   // bitmask 0=Sun..6=Sat (127 = all days)
uint8_t       cfgNightOverride = 0;  // 0=Auto, 1=Force ON, 2=Force OFF

/* defined in later headers, called from the tasks below */
void fetchWeather();

/* ---- cross-core command queue: the MQTT and Web tasks run on core 0,
        but only core 1 can safely update the screen state/LittleFS.
        Core 0 queues commands here; loop() on core 1 pops and executes
        them safely. All UI handlers on core 0 MUST perform their actions
        through postAction(), never call the ctl* functions directly. ---- */
enum ActionCmd { CMD_NONE, CMD_PLAY_GIF, CMD_MSG, CMD_PIN, CMD_STOP, CMD_CELEBRATE, CMD_BRIGHTNESS, CMD_TEST, CMD_NIGHTMODE };
struct ActionRequest {
  ActionCmd  cmd     = CMD_NONE;
  String   strArg  = "";       // primary string (name / text / ssid / lat / token)
  String   strArg2 = "";       // secondary string (pass / lon / chatid)
  int      intArg  = 0;        // numeric (brightness)
};
volatile bool hasActionReq = false;
ActionRequest actionReq;

/* ---- FreeRTOS task handles and mutexes ---- */
TaskHandle_t weatherTaskHandle = NULL;
TaskHandle_t mqttTaskHandle    = NULL;
SemaphoreHandle_t logMutex = NULL;
SemaphoreHandle_t actionMutex = NULL;

void postAction(ActionCmd cmd, String s1 = "", String s2 = "", int i1 = 0) {
  if (actionMutex && xSemaphoreTake(actionMutex, portMAX_DELAY)) {
    actionReq.cmd = cmd; actionReq.strArg = s1; actionReq.strArg2 = s2; actionReq.intArg = i1;
    hasActionReq = true;
    xSemaphoreGive(actionMutex);
  }
}

enum Mode { MODE_CLOCK, MODE_MSG, MODE_GIF, MODE_CELEBRATE, MODE_TEST, MODE_SETUP };
Mode mode = MODE_CLOCK;

String   scrollText     = "";
int      scrollX        = PANEL_W;
uint32_t msgEndsAt      = 0;          // millis when /msg expires (0 = pinned)
String   currentGifPath = "";
bool     gifOpen        = false;
bool     gifIsUserPlay  = false;   // true = /play or /celebrate, false = cameo (plays once)
File     fsGifFile;

/* ---- celebration (special-day) mode: merged party + special days ---- */
String   lastCelebDate  = "";         // "MM-DD" already celebrated today
String   celebMsg       = "";
String   celebType      = "custom";   // "custom" | "birthday" — drives the visual theme
uint8_t  celebPhase     = 0;          // 0 = visual, 1 = message banner
uint32_t celebPhaseAt   = 0;

/* ---- /test mode: cycles through every screen type ---- */
uint8_t  testStep       = 0;
uint32_t testStepAt     = 0;
int      testSavedWCode = -1;
String   testChat       = "";

float    wTemp          = NAN;
int      wCode          = -1;
uint32_t lastWeatherAt  = 0;
uint32_t lastBotAt      = 0;
uint32_t lastSecondAt   = 0;
uint32_t lastIdleAt     = 0;
uint8_t  userBrightness = DAY_BRIGHTNESS;
struct tm tmNow;

WebServer web(80);                    // local dashboard at http://digiframe.local

/* ---- in-memory log ring buffer, viewable from the dashboard ---- */
#define LOG_LINES 40
String   logBuf[LOG_LINES];
int      logHead = 0;
uint32_t logSeq  = 0;
void logLine(const String &s) {
  uint32_t sec = millis() / 1000;
  char ts[16];
  snprintf(ts, sizeof(ts), "[%lu:%02lu] ", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  if (logMutex) xSemaphoreTake(logMutex, portMAX_DELAY);
  logBuf[logHead] = String(ts) + s;
  logHead = (logHead + 1) % LOG_LINES;
  logSeq++;
  if (logMutex) xSemaphoreGive(logMutex);
  Serial.println(s);                  // also to Serial if a cable is attached
}
File     webUpload;
uint32_t charEveryMs    = 10UL * 60000UL;   // random character cameo interval

/* ---- background task: weather fetch on core 0 ---- */
void weatherTask(void *pv) {
  vTaskDelay(pdMS_TO_TICKS(2000));        // let setup() finish first
  uint32_t lastFetch = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED &&
        (weatherNow || !lastFetch || millis() - lastFetch > 20UL * 60000UL)) {
      weatherNow = false;
      fetchWeather();
      lastFetch = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(5000));      // short tick so weatherNow applies fast
  }
}

/**********************  4. COLORS  ***********************************/
#define C_TIME   dma->color565(255, 179, 222)   // pastel pink
#define C_TEMP   dma->color565(173, 216, 255)   // pastel blue
#define C_DATE   dma->color565(200, 190, 255)   // lavender
#define C_MSG    dma->color565(255, 210, 150)   // warm peach
#define C_ACCENT dma->color565(255,  80, 120)    // neutral highlight (seconds head, sparkles)
