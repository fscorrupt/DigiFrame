/**********************************************************************
 *  DIGIFRAME  —  64x64 HUB75 Smart Gift Frame
 *  ESP32-S3 (N16R8)  +  P2.5 64x64 HUB75 panel
 *
 *  FEATURES
 *   - NTP clock (IST) + Open-Meteo weather (no API key needed)
 *   - Automatic night mode (dim glow after midnight)
 *   - Local web dashboard (http://digiframe.local): GIF upload, WiFi &
 *     Telegram config, brightness, messages, live logs
 *   - WiFi setup mode: if WiFi can't connect, the frame starts its own
 *     hotspot and shows a QR code on the panel — scan to join, then the
 *     config page opens (captive portal) to enter your WiFi. When the
 *     stored network comes back, it reconnects automatically.
 *   - Telegram bot remote control with tap-able button menus (/menu):
 *       /msg <text>        scroll a message for ~10 minutes
 *       /pin <text>        scroll a message until Stop
 *       /play <name>       play a stored GIF on loop (or tap ▶ Play GIF)
 *       /list  /del <name>
 *       /brightness <1-255> (or tap 💡 Brightness)
 *       /event add MM-DD <name>   /event del MM-DD   /events
 *       /party  /test  /stop  /status  /help
 *     (GIF upload is done on the dashboard, not through Telegram)
 *   - Special-day engine: at 00:00 local time on a stored date the frame
 *     auto-starts the celebration: looping cake GIF + rotating messages
 *     all day long.
 *
 *  FILES (Arduino IDE shows these as tabs; include order matters)
 *   config.h        user config + pin map (compile-time defaults)
 *   globals.h       globals, cross-core queue, background tasks, colors
 *   gif_player.h    GIF decode callbacks + open/close + character pack
 *   events_store.h  special days + persisted config on LittleFS
 *   weather.h       Open-Meteo fetch + weather icons
 *   scene.h         clock face + ambient scene
 *   scroll.h        scrolling text renderer
 *   party.h         party mode + test mode
 *   telegram.h      Telegram bot commands + menus
 *   web_portal.h    web dashboard + captive portal pages
 *   qr_display.h    on-panel QR codes for setup mode
 *   wifi_manager.h  WiFi connect, hotspot fallback, auto-reconnect
 *
 *  LIBRARIES (Arduino IDE -> Library Manager unless noted)
 *   - "ESP32 HUB75 LED MATRIX PANEL DMA Display" (mrfaptastic)
 *   - "Adafruit GFX Library"
 *   - "AnimatedGIF" (Larry Bank)
 *   - "UniversalTelegramBot" (Brian Lough)  + "ArduinoJson"
 *   (QR codes use the espressif__qrcode component bundled with the core)
 *
 *  BOARD SETTINGS (Tools menu)
 *   Board: "ESP32S3 Dev Module"
 *   Flash Size: 16MB | PSRAM: "OPI PSRAM"
 *   Partition Scheme: "16M Flash (2MB APP / 12.5MB FATFS)" or any
 *     16MB scheme with a large data partition (used by LittleFS)
 *
 *  CLI BUILD (see FLASHING.md)
 *   arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=fatflash --output-dir build .
 *
 *  FILES to place on LittleFS (upload via the dashboard, or once via
 *  the "ESP32 Sketch Data Upload" plugin):
 *   /cake.gif       64x64 birthday animation (celebration default)
 *   /idle.gif       optional cute idle animation
 *********************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <AnimatedGIF.h>
#include <UniversalTelegramBot.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "config.h"
#include "globals.h"
#include "gif_player.h"
#include "events_store.h"
#include "weather.h"
#include "scene.h"
#include "scroll.h"
#include "party.h"
#include "telegram.h"
#include "web_portal.h"
#include "qr_display.h"
#include "wifi_manager.h"

/**********************  13. SETUP  ***********************************/
void setup() {
  Serial.begin(115200);
  delay(300);

  /* ---- matrix ---- */
  HUB75_I2S_CFG::i2s_pins pins = { R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN,
                                   B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
                                   LAT_PIN, OE_PIN, CLK_PIN };
  HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, 1, pins);
  cfg.latch_blanking = 2;          // helps ghosting on some P2.5 panels
  cfg.clkphase = false;            // flip if you see column shift
  cfg.double_buff = true;          // back-buffer drawing; flip atomically → zero tearing/flicker
  dma = new MatrixPanel_I2S_DMA(cfg);
  dma->begin();
  dma->setBrightness8(DAY_BRIGHTNESS);
  dma->fillScreen(0);
  dma->setTextSize(1);
  dma->setTextColor(C_TIME);
  dma->setCursor(1, 12);
  dma->print("HELLO");

  /* ---- filesystem + persisted config (may override WiFi/TG creds) ----
     NB: the fatflash partition scheme labels the data partition "ffat"
     (not the LittleFS default "spiffs") — pass the label explicitly or
     the mount fails and nothing (config/GIFs) ever persists. */
  if (!LittleFS.begin(true, "/littlefs", 10, "ffat"))
    Serial.println("LittleFS mount failed!");
  loadEvents();
  loadConfig();
  dma->setBrightness8(userBrightness);

  /* ---- wifi (falls back to hotspot + on-screen QR portal) ---- */
  if (wifiConnect(20000)) {
    logLine("WiFi OK, IP " + WiFi.localIP().toString());
  } else {
    logLine("WiFi FAILED — starting setup hotspot");
    startPortal();
  }

  /* ---- time (IST) — SNTP keeps retrying once a network appears ---- */
  configTime(TZ_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");
  if (WiFi.status() == WL_CONNECTED) {
    time_t now = 0;
    uint32_t t0 = millis();
    while (now < 8 * 3600 && millis() - t0 < 15000) { now = time(nullptr); delay(200); }
    localtime_r(&now, &tmNow);
    logLine("Time sync: " + String(now > 8 * 3600 ? "OK" : "FAILED"));
  }

  /* ---- telegram ---- */
  tgClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  bot.updateToken(botToken);       // config.json may hold a newer token
  bot.longPoll = 0;                // never block the render loop
  logLine("Telegram ready, allowed chat_id=" + allowedChatId);

  /* ---- local dashboard: http://digiframe.local / http://192.168.4.1 ---- */
  setupWeb();
  if (WiFi.status() == WL_CONNECTED && MDNS.begin("digiframe"))
    MDNS.addService("http", "tcp", 80);

  /* ---- first weather ---- */
  if (WiFi.status() == WL_CONNECTED) fetchWeather();
  gif.begin(LITTLE_ENDIAN_PIXELS);

  /* ---- create mutexes and start background tasks on core 0 ---- */
  logMutex    = xSemaphoreCreateMutex();
  tgReqMutex  = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(tgTask,      "telegram", 8192, NULL, 1, &tgTaskHandle,      0);
  xTaskCreatePinnedToCore(weatherTask, "weather",  4096, NULL, 1, &weatherTaskHandle, 0);

  Serial.println("DigiFrame ready.");
}

/**********************  14. MAIN LOOP  *******************************/
void loop() {
  web.handleClient();              // local dashboard / captive portal
  wifiManagerTick();               // portal DNS, STA retries, auto-recover

  uint32_t ms = millis();

  /* --- once per second: time, night mode, celebration trigger --- */
  if (ms - lastSecondAt >= 1000) {
    lastSecondAt = ms;
    time_t now = time(nullptr);
    localtime_r(&now, &tmNow);

    // night mode (skipped during party; setup QR must stay scannable)
    if (mode != MODE_PARTY && mode != MODE_SETUP) {
      bool night = (tmNow.tm_hour >= NIGHT_START_HR && tmNow.tm_hour < NIGHT_END_HR);
      dma->setBrightness8(night ? NIGHT_BRIGHTNESS : userBrightness);
    }

    // auto-start celebration at 00:00 on a special day (not while in setup)
    String today = todayMMDD();
    if (!portalActive && today != lastPartyDate) {
      for (int i = 0; i < numEvents; i++) {
        if (events[i].date == today) {
          lastPartyDate = today;
          startParty(events[i].name);
          break;
        }
      }
    }
    // party day sanity: if an auto-started birthday party is no longer
    // today's date (i.e. the day rolled over), return to clock.
    // lastPartyDate is only set by the auto-trigger, so a manual /party
    // test (lastPartyDate stays "") is never force-exited here.
    if (mode == MODE_PARTY && lastPartyDate.length() && todayMMDD() != lastPartyDate) {
      closeGif(); mode = MODE_CLOCK;
      logLine("party auto-ended (date rolled over)");
    }
  }

  /* --- weather every 20 min — handled by weatherTask on core 0 --- */

  /* --- telegram every 3 s — handled by tgTask on core 0 --- */

  /* --- consume pending Telegram command on this core (safe for LittleFS/DMA) --- */
  if (tgReqReady && tgReqMutex && xSemaphoreTake(tgReqMutex, 0) == pdTRUE) {
    TgRequest req = tgReq;
    tgReqReady = false;
    xSemaphoreGive(tgReqMutex);
    switch (req.cmd) {
      case TGC_PLAY_GIF:
        if (openGif(req.strArg, true)) mode = MODE_GIF;
        break;
      case TGC_MSG:
        scrollText = req.strArg; scrollX = PANEL_W;
        closeGif(); mode = MODE_MSG;
        msgEndsAt = millis() + MSG_MINUTES * 60000UL;
        break;
      case TGC_PIN:
        scrollText = req.strArg; scrollX = PANEL_W;
        closeGif(); mode = MODE_MSG; msgEndsAt = 0;
        break;
      case TGC_STOP:
        if (mode == MODE_TEST) wCode = testSavedWCode;
        closeGif(); mode = MODE_CLOCK;
        break;
      case TGC_PARTY:
        startParty(req.strArg);
        break;
      case TGC_TEST:
        startTest(req.strArg);
        break;
      case TGC_BRIGHTNESS:
        userBrightness = req.intArg;
        dma->setBrightness8(userBrightness);
        break;
      default: break;
    }
  }

  /* --- render by mode --- */
  switch (mode) {
    case MODE_CLOCK: {
      static uint32_t lastFace = 0;
      if (ms - lastFace > 66) { lastFace = ms; renderClock(); }  // ~15 fps scene
      // random character cameo: open the GIF and switch to MODE_GIF for one pass;
      // loop() drives playback naturally — no blocking busy-wait
      if (charEveryMs && ms - lastIdleAt > charEveryMs) {
        lastIdleAt = ms;
        String p = pickRandomChar();
        if (p.length() && openGif(p)) mode = MODE_GIF;  // returns to MODE_CLOCK when GIF ends
      }
      break;
    }
    case MODE_MSG:
      if (renderScroll(C_MSG)) dma->flipDMABuffer();
      if (msgEndsAt && ms > msgEndsAt) { mode = MODE_CLOCK; }
      break;
    case MODE_GIF:
      if (gifOpen) {
        int res = gif.playFrame(true, NULL);
        blitGifCanvas();
        dma->flipDMABuffer();
        int err = gif.getLastError();
        if (err != GIF_SUCCESS && err != GIF_EMPTY_FRAME) {
          // A GIF that can't be decoded would otherwise loop forever on a
          // black screen — report it once and return to the clock.
          // (GIF_EMPTY_FRAME is harmless — some GIFs have blank frames.)
          logLine("GIF decode error err=" + String(err) + " (" + currentGifPath + ") -> clock");
          closeGif();
          mode = MODE_CLOCK;
        } else if (res == 0) {
          if (gifIsUserPlay) {
            gif.reset();
          } else {
            closeGif();
            mode = MODE_CLOCK;
          }
        }
      } else mode = MODE_CLOCK;
      break;
    case MODE_PARTY:
      runParty();
      break;
    case MODE_TEST:
      runTest();
      break;
    case MODE_SETUP:
      renderSetupQR();             // static QR; redraws only when it changes
      break;
  }
}
