/**********************************************************************
 *  DIGIFRAME  —  64x64 HUB75 Smart Clock
 *  ESP32-S3 (N16R8)  +  P2.5 64x64 HUB75 panel
 *
 *  FEATURES
 *   - NTP clock + Open-Meteo weather (no API key needed) + living ambient scene
 *   - Automatic night mode (dim glow after midnight)
 *   - On-device dashboard (http://digiframe.local): GIF upload, WiFi,
 *     brightness, messages, logs
 *   - WiFi setup mode: if WiFi can't connect, the clock starts its own
 *     hotspot and shows a QR code on the panel — scan to join, then the
 *     config page opens (captive portal) to enter your WiFi. When the
 *     stored network comes back, it reconnects automatically.
 *   - Telegram bot remote control with tap-able button menus (/menu):
 *       /msg <text>        scroll a message for ~10 minutes
 *       /pin <text>        scroll a message until Stop
 *       /play <name>       play a stored GIF on loop (or tap ▶ Play GIF)
 *       /list  /del <name>
 *       /brightness <1-255> (or tap 💡 Brightness)
 *       /event add MM-DD <type> <message>   /event del MM-DD   /events
 *       /celebrate  /test  /stop  /status  /help
 *     (GIF upload is done on the dashboard, not through Telegram)
 *   - Special days: give a date a TYPE (custom / birthday) and a message; at
 *     00:00 the clock auto-runs the themed celebration (fireworks or cake +
 *     rotating banner) all day long. Manual preview via /celebrate.
 *   - Home Assistant integration over MQTT (optional): brightness, message,
 *     celebrate/stop, plus temperature/mode sensors, via MQTT discovery.
 *
 *  FILES (Arduino IDE shows these as tabs; include order matters)
 *   config.h        user config + pin map (compile-time defaults)
 *   globals.h       globals, cross-core queue, background tasks, colors
 *   gif_player.h    GIF decode callbacks + open/close + character pack
 *   events_store.h  special days + persisted config on LittleFS
 *   weather.h       Open-Meteo fetch + weather icons
 *   scene.h         clock face + ambient scene
 *   scroll.h        scrolling text renderer
 *   party.h         celebration (special-day) mode + test mode
 *   control.h       shared control layer (HTTP + MQTT call the same ctl*)
 *   web_portal.h    Dashboard + HTTP API routes
 *   mqtt_ha.h       Home Assistant integration via MQTT (optional)
 *   qr_display.h    on-panel QR codes for setup mode
 *   wifi_manager.h  WiFi connect, hotspot fallback, auto-reconnect
 *
 *  LIBRARIES (Arduino IDE -> Library Manager unless noted)
 *   - "ESP32 HUB75 LED MATRIX PANEL DMA Display" (mrfaptastic)
 *   - "Adafruit GFX Library"
 *   - "AnimatedGIF" (Larry Bank)
 *   - "ArduinoJson"
 *   - "PubSubClient" (Nick O'Leary)
 *   (QR codes use the espressif__qrcode component bundled with the core)
 *
 *  BOARD SETTINGS (Tools menu)
 *   Board: "ESP32S3 Dev Module"
 *   Flash Size: 16MB | PSRAM: "OPI PSRAM"
 *   Partition Scheme: "Custom" (uses this sketch's partitions.csv —
 *     4MB OTA app slots + ~7.9MB ffat data for LittleFS)
 *
 *  CLI BUILD (see FLASHING.md)
 *   arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc --output-dir build firmware/DigiFrame
 *
 *  FILES to place on LittleFS (upload via the dashboard, or once via
 *  the "ESP32 Sketch Data Upload" plugin):
 *   /cake.gif       64x64 animation used by the "birthday" celebration type
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
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "config.h"
#include "globals.h"
#include "utf8_emoji.h"
#include "gif_player.h"
#include "events_store.h"
#include "weather.h"
#include "scene.h"
#include "scroll.h"
#include "party.h"
#include "control.h"
#include "web_portal.h"
#include "mqtt_ha.h"
#include "qr_display.h"
#include "wifi_manager.h"

/* Boot-time memory probe. Internal (non-PSRAM) DRAM is the binding resource
   on this board — WiFi, the panel's DMA framebuffer and every TLS session
   compete for it, and PSRAM cannot substitute (see config.h). Allocators
   that need a *contiguous* block care about `largest`, not the total, so
   print both around anything big in setup(). */
static void heapReport(const char *where) {
  Serial.printf("[heap] %-22s internal free %6u  largest %6u | psram free %7u\n",
                where,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.flush();
  delay(20);        // USB-CDC: let the line drain in case the next call abort()s

}

/**********************  13. SETUP  ***********************************/
void setup() {
  Serial.begin(115200);
  delay(300);

  /* ---- filesystem + persisted config (may override WiFi/TG creds) ----
     NB: the fatflash partition scheme labels the data partition "ffat"
     (not the LittleFS default "spiffs") — pass the label explicitly or
     the mount fails and nothing (config/GIFs) ever persists. */
  if (!LittleFS.begin(true, "/littlefs", 10, "ffat"))
    Serial.println("LittleFS mount failed!");
  loadConfig();
  loadEvents();
  loadCalendar();
  seedDefaultGifs();          // one-time copy of the embedded default pack
  heapReport("after LittleFS");

  /* ---- matrix ---- */
  int8_t r1 = R1_PIN, g1 = G1_PIN, b1 = B1_PIN;
  int8_t r2 = R2_PIN, g2 = G2_PIN, b2 = B2_PIN;
  if (colorOrder == 1) { // RBG
      g1 = B1_PIN; b1 = G1_PIN;
      g2 = B2_PIN; b2 = G2_PIN;
  } else if (colorOrder == 2) { // GRB
      r1 = G1_PIN; g1 = R1_PIN;
      r2 = G2_PIN; g2 = R2_PIN;
  } else if (colorOrder == 3) { // GBR
      r1 = G1_PIN; g1 = B1_PIN; b1 = R1_PIN;
      r2 = G2_PIN; g2 = B2_PIN; b2 = R2_PIN;
  } else if (colorOrder == 4) { // BRG
      r1 = B1_PIN; g1 = R1_PIN; b1 = G1_PIN;
      r2 = B2_PIN; g2 = R2_PIN; b2 = G2_PIN;
  } else if (colorOrder == 5) { // BGR
      r1 = B1_PIN; b1 = R1_PIN;
      r2 = B2_PIN; b2 = R2_PIN;
  }

  HUB75_I2S_CFG::i2s_pins pins = { 
    r1, g1, b1, 
    r2, g2, b2, 
    A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
    LAT_PIN, OE_PIN, CLK_PIN 
  };
  HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, 1, pins);
  cfg.latch_blanking = 2;          // helps ghosting on some P2.5 panels
  cfg.clkphase = false;            // flip if you see column shift
  cfg.double_buff = true;          // back-buffer drawing; flip atomically → zero tearing/flicker
  cfg.setPixelColorDepthBits(PANEL_COLOR_DEPTH);   // internal-DRAM budget — see config.h
  dma = new MatrixPanel_I2S_DMA(cfg);
  dma->begin();
  heapReport("after dma->begin()");
  dma->setBrightness8(userBrightness);
  dma->setRotation(displayRotation / 90);
  dma->fillScreen(0);
  dma->setTextSize(1);
  dma->setTextColor(C_TIME);
  dma->setCursor(1, 12);
  dma->print("HELLO");

  /* ---- wifi (falls back to hotspot + on-screen QR portal) ---- */
  if (wifiConnect(20000)) {
    logLine("WiFi OK, IP " + WiFi.localIP().toString());
  } else {
    logLine("WiFi FAILED — starting setup hotspot");
    startPortal();
  }

  /* ---- time (IST) — SNTP keeps retrying once a network appears ---- */
  configTime(tzOffsetSec, 0, "pool.ntp.org", "time.google.com");
  if (WiFi.status() == WL_CONNECTED) {
    time_t now = 0;
    uint32_t t0 = millis();
    while (now < 8 * 3600 && millis() - t0 < 15000) { now = time(nullptr); delay(200); }
    localtime_r(&now, &tmNow);
    logLine("Time sync: " + String(now > 8 * 3600 ? "OK" : "FAILED"));
  }
  /* ---- local dashboard: http://digiframe.local / http://192.168.4.1 ---- */
  setupWeb();
  if (WiFi.status() == WL_CONNECTED && MDNS.begin("digiframe"))
    MDNS.addService("http", "tcp", 80);

  /* ---- first weather ---- */
  if (WiFi.status() == WL_CONNECTED) fetchWeather();
  gif.begin(LITTLE_ENDIAN_PIXELS);

  logMutex    = xSemaphoreCreateMutex();
  actionMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(weatherTask, "weather",  4096, NULL, 1, &weatherTaskHandle, 0);
  xTaskCreatePinnedToCore(mqttTask,    "mqtt",     6144, NULL, 1, &mqttTaskHandle,    0);

  heapReport("end of setup()");
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

    // night mode (skipped during a celebration; setup QR must stay scannable).
    // Night is a *cap*, not a fixed level: it dims down to NIGHT_BRIGHTNESS,
    // but a lower manual setting still wins — so the slider works at night.
    if (mode != MODE_CELEBRATE && mode != MODE_SETUP) {
      if (cfgNightOverride == 1) isNightMode = true;
      else if (cfgNightOverride == 2) isNightMode = false;
      else {
        // Auto schedule
        bool dayEnabled = (cfgNightDays & (1 << tmNow.tm_wday));
        bool inTime = false;
        if (cfgNightStart < cfgNightEnd) {
          inTime = (tmNow.tm_hour >= cfgNightStart && tmNow.tm_hour < cfgNightEnd);
        } else {
          inTime = (tmNow.tm_hour >= cfgNightStart || tmNow.tm_hour < cfgNightEnd);
        }
        isNightMode = (dayEnabled && inTime);
      }
      uint8_t target = userBrightness;
      if (isNightMode) {
        target = 1; // absolute minimum visible brightness
      }
      dma->setBrightness8(target);
    } else {
      isNightMode = false;
    }

    // auto-start the celebration at 00:00 on a special day (not while in setup)
    String today = todayMMDD();
    if (!portalActive && today != lastCelebDate) {
      for (int i = 0; i < numEvents; i++) {
        if (events[i].date == today) {
          lastCelebDate = today;
          startCelebration(events[i].type, events[i].message);
          break;
        }
      }
    }
    // celebration day sanity: if an auto-started celebration is no longer
    // today's date (i.e. the day rolled over), return to clock.
    // lastCelebDate is only set by the auto-trigger, so a manual /celebrate
    // (lastCelebDate stays "") is never force-exited here.
    if (mode == MODE_CELEBRATE && lastCelebDate.length() && todayMMDD() != lastCelebDate) {
      closeGif(); mode = MODE_CLOCK;
      logLine("celebration auto-ended (date rolled over)");
    }
  }

  /* --- consume a pending command posted from core 0 (Web/MQTT) on
         core 1. Touching LittleFS/DMA is only safe here. The actual
         work lives in control.h so HTTP + MQTT stay in sync. --- */
  if (hasActionReq && actionMutex && xSemaphoreTake(actionMutex, 0) == pdTRUE) {
    ActionRequest req = actionReq;
    hasActionReq = false;
    xSemaphoreGive(actionMutex);
    switch (req.cmd) {
      case CMD_PLAY_GIF:   ctlPlayGif(req.strArg);                       break;
      case CMD_MSG:        ctlSendMsg(req.strArg, false);                break;
      case CMD_PIN:        ctlSendMsg(req.strArg, true);                 break;
      case CMD_STOP:       ctlStop();                                    break;
      case CMD_CELEBRATE:  (req.strArg.length() || req.strArg2.length())
                             ? startCelebration(req.strArg, req.strArg2)
                             : ctlCelebrate();                            break;
      case CMD_TEST:       startTest(req.strArg);                        break;
      case CMD_BRIGHTNESS: ctlSetBrightness(req.intArg);                 break;
      case CMD_NIGHTMODE:
        cfgNightOverride = req.intArg;
        saveConfig();
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
    case MODE_CELEBRATE:
      runCelebration();
      break;
    case MODE_TEST:
      runTest();
      break;
    case MODE_SETUP:
      renderSetupQR();             // static QR; redraws only when it changes
      break;
  }
}
