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
volatile bool weatherNow   = false;  // web handler asks for an immediate refetch
volatile bool tgTokenDirty = false;  // set by web handler (core 1), applied by tgTask (core 0)
bool          portalActive = false;  // setup hotspot + captive portal active
volatile bool wifiRetryNow = false;  // web handler asks for an immediate STA (re)connect

/* defined in later headers, called from the tasks below */
void handleTelegram();
void fetchWeather();

/* ---- cross-core command queue: Telegram task (core 0) posts here,
        render loop (core 1) consumes it — avoids LittleFS + mode races ---- */
enum TgCmd { TGC_NONE, TGC_PLAY_GIF,
             TGC_MSG, TGC_PIN, TGC_STOP, TGC_PARTY, TGC_BRIGHTNESS, TGC_TEST };
struct TgRequest {
  TgCmd   cmd      = TGC_NONE;
  String  strArg   = "";
  uint8_t intArg   = 0;
};
volatile bool    tgReqReady = false;
TgRequest        tgReq;
SemaphoreHandle_t tgReqMutex = NULL;   // guards tgReq / tgReqReady

/* ---- background task handles (network work runs on core 0) ---- */
TaskHandle_t tgTaskHandle      = NULL;
TaskHandle_t weatherTaskHandle = NULL;
SemaphoreHandle_t logMutex     = NULL;   // guards logBuf / logHead / logSeq
void postTgCmd(TgCmd cmd, const String &str = "", uint8_t i = 0) {
  if (!tgReqMutex) return;
  xSemaphoreTake(tgReqMutex, portMAX_DELAY);
  tgReq.cmd    = cmd;
  tgReq.strArg = str;
  tgReq.intArg = i;
  tgReqReady   = true;
  xSemaphoreGive(tgReqMutex);
}

enum Mode { MODE_CLOCK, MODE_MSG, MODE_GIF, MODE_PARTY, MODE_TEST, MODE_SETUP };
Mode mode = MODE_CLOCK;

String   scrollText     = "";
int      scrollX        = PANEL_W;
uint32_t msgEndsAt      = 0;          // millis when /msg expires (0 = pinned)
String   currentGifPath = "";
bool     gifOpen        = false;
bool     gifIsUserPlay  = false;   // true = /play or /party, false = cameo (plays once)
File     fsGifFile;

String   lastPartyDate  = "";         // "MM-DD" already celebrated today
String   partyMsg       = "";
uint8_t  partyPhase     = 0;          // 0 = gif, 1 = message
uint32_t partyPhaseAt   = 0;

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
#define C_HEART  dma->color565(255,  80, 120)
