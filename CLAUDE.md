# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

DigiFrame is an open-source Arduino/ESP32-S3 firmware that drives a 64x64 HUB75 LED matrix as a **smart clock**: NTP clock, Open-Meteo weather, a neutral living ambient scene, GIF playback from LittleFS, scrolling messages, typed **special days** (date + type + message → themed celebration; merges the old "party mode"), a Telegram bot, a cloud dashboard over Web Bluetooth + a local web dashboard, optional **Home Assistant integration over MQTT**, and a WiFi setup hotspot with an on-panel QR code. Being repositioned from a personal "gift frame" — keep it generic, no personal/gift references.

## Build / Flash

Arduino IDE **or** arduino-cli (installed, reuses `%LOCALAPPDATA%\Arduino15`):

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom --output-dir build .
```

- **Board:** ESP32S3 Dev Module — Flash 16MB, PSRAM "OPI PSRAM", partition scheme **Custom** (`PartitionScheme=custom`), which uses the sketch's `partitions.csv` — a 16 MB layout with **4 MB OTA app slots** (app0/app1) + ~7.9 MB `ffat` data. (The old `fatflash` scheme's 2 MB app got tight after adding NimBLE; `partitions.csv` doubles it for future features.) Arduino IDE reports "% of 16 MB" for Custom, but the real ceiling is the 4 MB app slot. Growing the app slots moved the data partition, so the first flash of this layout wipes LittleFS once (GIFs/config re-seed on next boot).
- **Libraries** (Library Manager): `ESP32 HUB75 LED MATRIX PANEL DMA Display` (mrfaptastic), `Adafruit GFX Library`, `AnimatedGIF` (Larry Bank), `UniversalTelegramBot` (Brian Lough), `ArduinoJson`, `NimBLE-Arduino` (h2zero — the BLE config service), `PubSubClient` (Nick O'Leary — the Home Assistant MQTT client). QR codes use the `espressif__qrcode` component bundled with the ESP32 core (`#include <qrcode.h>` resolves to it — do NOT install the ricmoo "QRCode" library, it gets shadowed).
- **Flashing:** see FLASHING.md. `build/DigiFrame_flash_at_0x0.bin` (compact, flash at 0x0) preserves LittleFS; `build/DigiFrame.ino.merged.bin` (16MB padded) wipes it. App-only reflash lives at 0x10000. Typical:
  ```
  esptool --chip esp32s3 --port COM5 write-flash 0x0 build/DigiFrame_flash_at_0x0.bin        # keep GIFs/config
  esptool --chip esp32s3 --port COM5 write-flash 0x10000 build/DigiFrame.ino.bin              # app only, fastest
  esptool --chip esp32s3 --port COM5 write-flash 0x0 build/DigiFrame.ino.merged.bin           # factory reset (wipes LittleFS)
  ```
- **Default GIF pack:** `gifs/*.gif` are embedded in the app image via the auto-generated `default_gifs.h` (regenerate with `tools/make_default_gifs.ps1` after changing `gifs/`) and copied to LittleFS **once** on first boot (`seedDefaultGifs()`, marker `/.gifs_seeded`) — after that they are ordinary files the user can delete from the dashboard, and deletions stick. Additional GIFs are uploaded via the web dashboard (`http://digiframe.local`). Telegram GIF upload was removed intentionally.

There are no tests, linters, or CI. Primary verification is a clean arduino-cli compile.

## Runtime configuration

Compile-time **defaults** live in `config.h` (WiFi SSID/pass, `BOT_TOKEN`, `ALLOWED_CHAT_ID`, timezone, location, brightness, hotspot `AP_SSID`/`AP_PASS`, `CLOUD_SITE_URL` for the setup QR, and `MQTT_*` for Home Assistant — all placeholders, no personal data). At runtime they are overridden by `/config.json` on LittleFS (keys `ssid`, `pass`, `tgToken`, `tgChat`, `bright`, `charMin`, `lat`, `lon`, `mqttEn`, `mqttHost`, `mqttPort`, `mqttUser`, `mqttPass`), editable from the web dashboard, BLE, and (for some) Telegram. Weather lat/lon live in fixed `char` buffers (`cfgLat`/`cfgLon`), not `String`, because core 1 writes them while `weatherTask` (core 0) reads. **Special days** persist separately in `/events.json` as `{d,t,m}` = date/type/message (type = `custom`|`birthday`); no default events are seeded. **Note:** any `BOT_TOKEN`/`WIFI_PASS`/`MQTT_PASS` you compile in are sensitive — the shipped defaults are placeholders, keep them that way in commits.

## Architecture

Single translation unit: `DigiFrame.ino` includes ordered `.h` files (order matters — later headers may call earlier ones; forward decls for `handleTelegram()`/`fetchWeather()` sit in `globals.h`). The actual include order in `DigiFrame.ino` is:

```
config → globals → gif_player → events_store → weather → scene → scroll → party → control → telegram → web_portal → ble_config → mqtt_ha → qr_display → wifi_manager
```

Preserve this order when adding a new header — e.g. anything using the DMA panel or `logLine` must come after `globals.h`; anything driving `MODE_SETUP` must come after `qr_display.h`.

| File | Contents |
|---|---|
| `config.h` | user config + pin map (compile-time defaults) |
| `globals.h` | globals, runtime config strings, TgCmd queue, `logLine`, `tgTask`/`weatherTask`, colors |
| `gif_player.h` | GIF decode callbacks, `openGif`/`closeGif`, character pack, `seedDefaultGifs` |
| `default_gifs.h` | auto-generated embedded default GIF pack (do not edit — run `tools/make_default_gifs.ps1`) |
| `events_store.h` | `/events.json` special days + `/config.json` persisted config |
| `weather.h` | Open-Meteo fetch + weather icons |
| `scene.h` | clock face + ambient scene (sprites, `renderClock`) — the big one |
| `scroll.h` | scrolling text renderer |
| `party.h` | **celebration** (special-day) mode + `/test` mode: `startCelebration(type,message)`/`runCelebration()`; visual by type (`birthday`→cake+confetti, `custom`→fireworks) then a scrolling message banner |
| `control.h` | **shared control layer**: one `ctl*` function per action (msg, brightness, play/del/upload GIF, interval, celebrate/stop, wifi, loc, tg, tgtest, add/del/list special days, MQTT config, status/list/logs JSON). HTTP handlers, BLE, Telegram and MQTT all call these — they run on core 1. |
| `telegram.h` | bot commands, reply keyboard, inline keyboards, callback queries |
| `web_portal.h` | dashboard HTML + `/api/*` handlers (thin wrappers over `control.h`) + OTA + captive-portal redirect (endpoints: `GET /`, `GET /api/logs`, `GET /api/list`, `GET /api/config`, `POST /api/msg`, `/api/brightness`, `/api/play`, `/api/del`, `/api/interval`, `/api/celebrate`, `/api/stop`, `/api/events`, `/api/eventdel`, `/api/upload`, `/api/tgtest`, `/api/wifi`, `/api/tgconfig`, `/api/loc`, `/api/mqtt`, `/api/ota`) |
| `ble_config.h` | NimBLE GATT config service (the cloud dashboard's Bluetooth path): `status`/`gifs`/`log`/`events` read-notify chars refreshed by `bleTick()` on core 1; `wifi`/`control`/`upload` write chars whose callbacks (core 0) `postTgCmd()` to core 1. UUIDs/payloads in `BLE_PROTOCOL.md`. |
| `mqtt_ha.h` | optional **Home Assistant integration over MQTT** (`PubSubClient`, `mqttTask` on core 0): MQTT discovery for brightness/message/celebrate/stop + temperature/mode sensors; commands `postTgCmd()` to core 1. Off unless enabled + a broker host is set. |
| `qr_display.h` | `renderSetupQR()` — QR on the panel in `MODE_SETUP` (encodes `CLOUD_SITE_URL/#d=<bleName>`) |
| `wifi_manager.h` | `wifiConnect`, `startPortal`/`stopPortal`, `wifiManagerTick` |

Everything runs on a dual-core FreeRTOS setup. The critical structural fact is the **core split and command queue** — get this wrong and you will race LittleFS against the DMA renderer.

- **Core 1 (`loop()`):** render loop. Owns the HUB75 DMA panel, AnimatedGIF decoder, mode state (`MODE_CLOCK/MSG/GIF/CELEBRATE/TEST/SETUP`), `WebServer` (port 80), DNSServer processing, and `wifiManagerTick()`.
- **Core 0 tasks:** `tgTask` (Telegram polling; also applies dashboard token changes via the `tgTokenDirty` flag — only this task touches the bot client), `weatherTask`, and `mqttTask` (Home Assistant — only active when MQTT is enabled with a broker host).
- **Cross-core handoff:** `tgTask`, the **NimBLE host task**, and **`mqttTask`** (all core 0) parse input and call `postTgCmd(...)` (single `TgRequest` slot guarded by `tgReqMutex`); `loop()` drains it and calls the `control.h` `ctl*` functions on core 1 (`openGif`/mode/LittleFS/`saveConfig`). New actions from either task must follow this pattern — never touch LittleFS/DMA from core 0. `TgRequest` carries a second string (wifi pass / lon / chat) and a PSRAM `buf` for GIF uploads (freed by core 1 after `TGC_GIF_COMMIT`).
- **Hybrid config, one implementation:** the HTTP dashboard (`web_portal.h`, core 1) calls `ctl*` directly; the BLE service (`ble_config.h`, core 0) marshals to them via the queue. Both front-ends therefore behave identically — add new config actions in `control.h`, then wire a thin HTTP handler and a BLE `control` opcode.
- **BLE reads:** `status`/`gifs`/`log` characteristic values are set from `bleTick()` on core 1 (safe LittleFS access), so the BLE stack only ever serves already-prepared bytes.
- **Web → WiFi handoff:** `/api/wifi` (or the BLE `wifi` char) → `ctlSetWifi` sets `wifiRetryNow`; `wifiManagerTick()` (core 1) performs the actual reconnect.
- **OTA** (`/api/ota` in `web_portal.h`): flashes an uploaded app image (`DigiFrame.ino.bin`) into the spare OTA slot via `Update.h` (the custom partition table has `app0`/`app1`, now 4 MB each), then reboots. It suspends the core-0 tasks (Telegram, weather, MQTT) for the duration and rejects non-app images (checks the `esp_app_desc_t` magic `0xABCD5432` at offset 0x20 of the first chunk). Like the rest of the dashboard it is unauthenticated LAN-only. After an OTA the device may boot from `app1` — a serial app-only flash at 0x10000 then needs otadata cleared (flash the `_0x0` image, or `esptool erase-region 0xe000 0x2000`).
- **Logging:** `logLine()` → mutex-guarded ring buffer (`logBuf`, 40 lines) shown on the dashboard, mirrored to Serial.

### WiFi / setup-portal flow

Boot: `wifiConnect(20s)` → on failure `startPortal()` (AP `DigiFrame`/`digiframe123` + captive DNS + `MODE_SETUP` QR). While the portal is up, STA retries stored creds every 30 s; any successful connect (old router back, or new creds saved over HTTP **or BLE**) triggers `stopPortal()`. At runtime, a sustained outage of `WIFI_FAIL_PORTAL_MIN` (5 min) reopens the portal.

The `MODE_SETUP` QR now encodes `CLOUD_SITE_URL/#d=<bleName>` (config.h). Scanning it opens the cloud site, which pairs over **Web Bluetooth** to provision WiFi (Chrome/Edge on Android/desktop). iPhone/no-Bluetooth users instead join the `DigiFrame` hotspot and use the on-device dashboard — so the QR switches to `http://192.168.4.1` once a station connects. `bleInit()` runs in `setup()` and advertises regardless of WiFi state, so BLE setup works before the frame is on any network.

### Hybrid config paths (why both HTTP and BLE)

The rich UI is a **cloud-hosted static PWA** in `website/` (deploy to Netlify/Vercel/Cloudflare). A cloud HTTPS page can't call the frame's HTTP LAN API (mixed content), so it talks to the frame over **Web Bluetooth**. The frame **also** keeps its on-device HTTP dashboard + `/api/*`, which any LAN browser (incl. iPhone Safari) can use. Both go through `control.h`. "Control from anywhere" remains the Telegram bot; a cloud relay backend is future work. `website/` and `BLE_PROTOCOL.md` are the client side; keep `ble_config.h` and `website/ble.js` in sync with the protocol doc.

### Mode invariants worth preserving

- **Double-buffered DMA** (`cfg.double_buff = true`). Draw a full frame, then `dma->flipDMABuffer()` — do not draw incrementally on the visible buffer or you will tear. The setup QR is static: it paints **both** buffers once and redraws only when its text changes.
- **Night mode** is applied every second in `loop()` and is intentionally **skipped during `MODE_CELEBRATE` and `MODE_SETUP`** (QR must stay scannable).
- **Auto-celebration trigger** compares `todayMMDD()` to `lastCelebDate` and, on a match, runs `startCelebration(day.type, day.message)`; skipped while `portalActive`. Manual `/celebrate` (and the BLE/HA celebrate) deliberately does **not** set `lastCelebDate` — preserve this distinction so a manual preview isn't force-ended at date rollover.
- **GIF playback:** `gifIsUserPlay` distinguishes user plays (loop forever) from ambient cameos (play once). On decode error, close and fall back to `MODE_CLOCK`.
