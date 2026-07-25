/**********************************************************************
 *  DIGIFRAME  —  64x64 HUB75 Smart Clock
 *  ESP32-S3 (N16R8)  +  P2.5 64x64 HUB75 panel
 *
 *  FEATURES
 *   - NTP clock + Open-Meteo weather (no API key needed) + living ambient scene
 *   - Automatic night mode (dim glow after midnight)
 *   - Cloud dashboard over Web Bluetooth + on-device dashboard
 *     (http://digiframe.local): GIF upload, WiFi, brightness, messages, logs
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
 *   control.h       shared control layer (HTTP + BLE call the same ctl*)
 *   telegram.h      Telegram bot commands + menus
 *   web_portal.h    web dashboard + captive portal pages
 *   ble_config.h    BLE config service (cloud dashboard over Web Bluetooth)
 *   mqtt_ha.h       Home Assistant integration over MQTT (optional)
 *   qr_display.h    on-panel QR codes for setup mode
 *   wifi_manager.h  WiFi connect, hotspot fallback, auto-reconnect
 *
 *  LIBRARIES (Arduino IDE -> Library Manager unless noted)
 *   - "ESP32 HUB75 LED MATRIX PANEL DMA Display" (mrfaptastic)
 *   - "Adafruit GFX Library"
 *   - "AnimatedGIF" (Larry Bank)
 *   - "UniversalTelegramBot" (Brian Lough)  + "ArduinoJson"
 *   - "NimBLE-Arduino" (h2zero)  + "PubSubClient" (Nick O'Leary)
 *   (QR codes use the espressif__qrcode component bundled with the core)
 *
 *  BOARD SETTINGS (Tools menu)
 *   Board: "ESP32S3 Dev Module"
 *   Flash Size: 16MB | PSRAM: "OPI PSRAM"
 *   Partition Scheme: "Custom" (uses this sketch's partitions.csv —
 *     4MB OTA app slots + ~7.9MB ffat data for LittleFS)
 *
 *  CLI BUILD (see FLASHING.md)
 *   arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom --output-dir build firmware/DigiFrame
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
#include "control.h"
#include "telegram.h"
#include "web_portal.h"
#include "ble_config.h"
#include "mqtt_ha.h"
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
  seedDefaultGifs();          // one-time copy of the embedded default pack
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
  configTime(tzOffsetSec, 0, "pool.ntp.org", "time.google.com");
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

  /* ---- BLE config service: the cloud dashboard pairs over Web Bluetooth
     (advertises regardless of WiFi state, so it works during setup too) ---- */
  bleInit();

  /* ---- first weather ---- */
  if (WiFi.status() == WL_CONNECTED) fetchWeather();
  gif.begin(LITTLE_ENDIAN_PIXELS);

  /* ---- create mutexes and start background tasks on core 0 ---- */
  logMutex    = xSemaphoreCreateMutex();
  tgReqMutex  = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(tgTask,      "telegram", 8192, NULL, 1, &tgTaskHandle,      0);
  xTaskCreatePinnedToCore(weatherTask, "weather",  4096, NULL, 1, &weatherTaskHandle, 0);
  xTaskCreatePinnedToCore(mqttTask,    "mqtt",     6144, NULL, 1, &mqttTaskHandle,    0);

  Serial.println("DigiFrame ready.");
}

/**********************  14. MAIN LOOP  *******************************/
void loop() {
  web.handleClient();              // local dashboard / captive portal
  wifiManagerTick();               // portal DNS, STA retries, auto-recover
  bleTick();                       // refresh BLE status/gifs/log for the cloud site

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
      bool night = (tmNow.tm_hour >= NIGHT_START_HR && tmNow.tm_hour < NIGHT_END_HR);
      uint8_t target = userBrightness;
      if (night && target > NIGHT_BRIGHTNESS) target = NIGHT_BRIGHTNESS;
      dma->setBrightness8(target);
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

  /* --- weather every 20 min — handled by weatherTask on core 0 --- */

  /* --- telegram every 3 s — handled by tgTask on core 0 --- */

  /* --- consume a pending command posted from core 0 (Telegram or BLE) on
         this core, where LittleFS / DMA / openGif are safe. All the real
         work lives in control.h so HTTP + BLE + Telegram stay in sync. --- */
  if (tgReqReady && tgReqMutex && xSemaphoreTake(tgReqMutex, 0) == pdTRUE) {
    TgRequest req = tgReq;
    tgReqReady = false;
    tgReq.buf  = nullptr;          // ownership moved to local `req`
    xSemaphoreGive(tgReqMutex);
    switch (req.cmd) {
      case TGC_PLAY_GIF:   ctlPlayGif(req.strArg);                       break;
      case TGC_MSG:        ctlSendMsg(req.strArg, false);                break;
      case TGC_PIN:        ctlSendMsg(req.strArg, true);                 break;
      case TGC_STOP:       ctlStop();                                    break;
      case TGC_CELEBRATE:  (req.strArg.length() || req.strArg2.length())
                             ? startCelebration(req.strArg, req.strArg2)
                             : ctlCelebrate();                            break;
      case TGC_TEST:       startTest(req.strArg);                        break;
      case TGC_BRIGHTNESS: ctlSetBrightness(req.intArg);                 break;
      case TGC_DEL_GIF:    ctlDelGif(req.strArg);                        break;
      case TGC_INTERVAL:   ctlSetInterval(req.strArg.toInt());           break;
      case TGC_SET_WIFI:   ctlSetWifi(req.strArg, req.strArg2);          break;
      case TGC_SET_LOC:    ctlSetLoc(req.strArg, req.strArg2);           break;
      case TGC_SET_TG:     ctlSetTg(req.strArg, req.strArg2);            break;
      case TGC_TGTEST:     ctlTgTest();                                  break;
      case TGC_GIF_COMMIT:
        ctlCommitGif(req.strArg, req.intArg != 0, req.buf, req.bufLen);
        if (req.buf) free(req.buf);                                      break;
      case TGC_EVENT_ADD:  ctlAddEventJson(req.strArg);                  break;
      case TGC_EVENT_DEL:  ctlDelEvent(req.strArg);                     break;
      case TGC_SET_MQTT:   ctlSetMqttJson(req.strArg);                  break;
      case TGC_SET_TZ:     ctlSetTz(req.strArg.toInt());                break;
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
