/* DigiFrame — globals, cross-core command queue, background tasks, colors */
#pragma once

/**********************  3. GLOBALS  **********************************/
MatrixPanel_I2S_DMA *dma = nullptr;
AnimatedGIF gif;
WiFiClientSecure tgClient;
UniversalTelegramBot bot(BOT_TOKEN, tgClient);

/* ---- runtime config: seeded from config.h defines, overridden by
        /config.json on LittleFS (editable from the web dashboard) ---- */
String cfgWifiSsid   = WIFI_SSID;
String cfgWifiPass   = WIFI_PASS;
String botToken      = BOT_TOKEN;
String allowedChatId = ALLOWED_CHAT_ID;
/* weather location — fixed char buffers (not String) because the web
   handler (core 1) writes while weatherTask (core 0) reads; a short
   strlcpy race is harmless, a String heap realloc race is not. */
char cfgLat[16]      = LATITUDE;
char cfgLon[16]      = LONGITUDE;
int  tzOffsetSec     = TZ_OFFSET_SEC;   // UTC offset in seconds (runtime-configurable)
/* ---- Home Assistant / MQTT (off by default; editable at runtime) ---- */
bool   mqttEnable = MQTT_ENABLE;
String mqttHost   = MQTT_HOST;
int    mqttPort   = MQTT_PORT;
String mqttUser   = MQTT_USER;
String mqttPass   = MQTT_PASS;
volatile bool mqttConfigDirty = false;  // set by web/BLE (core 1), applied by mqttTask (core 0)
volatile bool weatherNow   = false;  // web handler asks for an immediate refetch
volatile bool tgTokenDirty = false;  // set by web handler (core 1), applied by tgTask (core 0)
bool          portalActive = false;  // setup hotspot + captive portal active
volatile bool wifiRetryNow = false;  // web handler asks for an immediate STA (re)connect

/* defined in later headers, called from the tasks below */
void handleTelegram();
void fetchWeather();

/* ---- cross-core command queue: the Telegram task AND the BLE config
        task (both core 0) post here; the render loop (core 1) consumes it —
        this is the ONLY safe way for core-0 work to touch LittleFS / the DMA
        panel / openGif / saveConfig. New actions from either task must go
        through postTgCmd(), never call the ctl* functions directly. ---- */
enum TgCmd { TGC_NONE, TGC_PLAY_GIF,
             TGC_MSG, TGC_PIN, TGC_STOP, TGC_CELEBRATE, TGC_BRIGHTNESS, TGC_TEST,
             /* config actions added for the BLE dashboard (see control.h) */
             TGC_DEL_GIF, TGC_INTERVAL, TGC_SET_WIFI, TGC_SET_LOC,
             TGC_SET_TG, TGC_TGTEST, TGC_GIF_COMMIT,
             /* typed special-days + Home Assistant config (JSON in strArg) */
             TGC_EVENT_ADD, TGC_EVENT_DEL, TGC_SET_MQTT, TGC_SET_TZ };
struct TgRequest {
  TgCmd    cmd     = TGC_NONE;
  String   strArg  = "";       // primary string (name / text / ssid / lat / token)
  String   strArg2 = "";       // secondary string (pass / lon / chat)
  uint8_t  intArg  = 0;        // brightness value, or GIF-upload pack flag
  uint8_t *buf     = nullptr;   // TGC_GIF_COMMIT: PSRAM buffer (core 1 frees it)
  size_t   bufLen  = 0;
};
volatile bool    tgReqReady = false;
TgRequest        tgReq;
SemaphoreHandle_t tgReqMutex = NULL;   // guards tgReq / tgReqReady

/* ---- background task handles (network work runs on core 0) ---- */
TaskHandle_t tgTaskHandle      = NULL;
TaskHandle_t weatherTaskHandle = NULL;
TaskHandle_t mqttTaskHandle    = NULL;
SemaphoreHandle_t logMutex     = NULL;   // guards logBuf / logHead / logSeq
void postTgCmd(TgCmd cmd, const String &str = "", uint8_t i = 0,
               const String &str2 = "", uint8_t *buf = nullptr, size_t buflen = 0) {
  if (!tgReqMutex) return;
  xSemaphoreTake(tgReqMutex, portMAX_DELAY);
  // single-slot queue: if an unconsumed GIF-upload commit is being
  // overwritten, free its buffer first so we don't leak PSRAM.
  if (tgReqReady && tgReq.cmd == TGC_GIF_COMMIT && tgReq.buf) free(tgReq.buf);
  tgReq.cmd     = cmd;
  tgReq.strArg  = str;
  tgReq.strArg2 = str2;
  tgReq.intArg  = i;
  tgReq.buf     = buf;
  tgReq.bufLen  = buflen;
  tgReqReady    = true;
  xSemaphoreGive(tgReqMutex);
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

/* ---- background task: Telegram polling on core 0 ---- */
void tgTask(void *pv) {
  for (;;) {
    if (tgTokenDirty) {                 // token changed on the dashboard —
      tgTokenDirty = false;             // apply it here so only this task
      bot.updateToken(botToken);        // ever touches the bot client
      logLine("TG token updated");
    }
    if (WiFi.status() == WL_CONNECTED) {
      static uint32_t lastBeat = 0;
      uint32_t ms = millis();
      if (ms - lastBeat > 60000) {
        lastBeat = ms;
        logLine("poll heartbeat (WiFi up, heap " + String(ESP.getFreeHeap() / 1024) + "KB)");
      }
      handleTelegram();
    } else {
      static uint32_t lastWarn = 0;
      if (millis() - lastWarn > 30000) {
        lastWarn = millis();
        logLine("WiFi DOWN — Telegram paused");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

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
