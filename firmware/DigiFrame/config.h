/* DigiFrame — user config + pin map (compile-time DEFAULTS; runtime overrides live in /config.json on LittleFS) */
#pragma once

/**********************  1. USER CONFIG  ******************************/
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

// Create a bot with @BotFather on Telegram, paste the token here.
#define BOT_TOKEN "YOUR_BOT_TOKEN"
// Send /start to your bot, then visit
// https://api.telegram.org/bot<TOKEN>/getUpdates to find your chat id.
// Only THIS chat id can control the frame (security!).
#define ALLOWED_CHAT_ID "YOUR_CHAT_ID"

#define TZ_OFFSET_SEC 19800 // default UTC+5:30 (override at runtime)
#define LATITUDE "12.97"    // default location — set your own from the dashboard
#define LONGITUDE "77.59"

#define DAY_BRIGHTNESS 100  // 0-255
#define NIGHT_BRIGHTNESS 3  // midnight sleep glow — very dim (1 = dimmest visible)
#define CELEBRATE_BRIGHTNESS 140  // brightness during a special-day celebration
#define NIGHT_START_HR 0 // 00:00
#define NIGHT_END_HR 7   // 07:00
#define MSG_MINUTES 10   // how long /msg scrolls

/* ---- setup hotspot: started when WiFi can't connect. The panel shows a
   QR code that joins this network; a captive portal then opens the
   config page (http://192.168.4.1) to set the real WiFi. ---- */
#define AP_SSID "DigiFrame"
#define AP_PASS "digiframe123" // >= 8 chars (WPA2)
#define WIFI_FAIL_PORTAL_MIN 5 // runtime outage (min) before portal reopens

/* ---- cloud dashboard: the on-panel setup QR points here. Scanning it
   opens the site, which pairs with the frame over Web Bluetooth (Android /
   desktop Chrome/Edge) to provision WiFi and configure everything. iPhone
   users, or anyone without Web Bluetooth, fall back to the on-device
   dashboard (join the "DigiFrame" hotspot -> http://192.168.4.1). Keep this
   short — it has to fit in the panel QR. ---- */
#define CLOUD_SITE_URL "https://digiframe.pages.dev"

/* ---- Home Assistant integration over MQTT (all overridable at runtime from
   the dashboard). Off by default so non-HA users are unaffected. Point
   MQTT_HOST at your broker (e.g. the Mosquitto add-on) and enable it. The
   clock announces itself to Home Assistant via MQTT discovery. ---- */
#define MQTT_ENABLE false
#define MQTT_HOST ""          // broker IP/hostname, e.g. "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASS ""

/**********************  2. PIN MAP  **********************************/
/* HUB75 (adjust freely if your wiring differs — every GPIO works,
   just avoid 0, 19/20 (USB), 26-32 (flash), 33-37 (PSRAM on N16R8),
   43/44 (UART), 45/46 (strapping). */
#define R1_PIN 4
#define G1_PIN 5
#define B1_PIN 6
#define R2_PIN 7
#define G2_PIN 15
#define B2_PIN 16
#define A_PIN 18
#define B_PIN 8
#define C_PIN 9
#define D_PIN 10
#define E_PIN 42 // REQUIRED on 64x64 (1/32 scan) — must be wired!
#define LAT_PIN 40
#define OE_PIN 2
#define CLK_PIN 41

#define PANEL_W 64
#define PANEL_H 64
