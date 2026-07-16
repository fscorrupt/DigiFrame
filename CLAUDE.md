# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

DigiFrame is an Arduino/ESP32-S3 firmware that drives a 64x64 HUB75 LED matrix as a "smart gift frame": NTP clock, Open-Meteo weather, ambient scene, GIF playback from LittleFS, scrolling messages, a Telegram bot for remote control (button menus), a local web dashboard for uploads/config, and a WiFi setup hotspot with an on-panel QR code.

## Build / Flash

Arduino IDE **or** arduino-cli (installed, reuses `%LOCALAPPDATA%\Arduino15`):

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=fatflash --output-dir build .
```

- **Board:** ESP32S3 Dev Module — Flash 16MB, PSRAM "OPI PSRAM", partition scheme "16M Flash (2MB APP / 12.5MB FATFS)" (`PartitionScheme=fatflash`).
- **Libraries** (Library Manager): `ESP32 HUB75 LED MATRIX PANEL DMA Display` (mrfaptastic), `Adafruit GFX Library`, `AnimatedGIF` (Larry Bank), `UniversalTelegramBot` (Brian Lough), `ArduinoJson`. QR codes use the `espressif__qrcode` component bundled with the ESP32 core (`#include <qrcode.h>` resolves to it — do NOT install the ricmoo "QRCode" library, it gets shadowed).
- **Flashing:** see FLASHING.md. `build/DigiFrame_flash_at_0x0.bin` (compact, flash at 0x0) preserves LittleFS; `build/DigiFrame.ino.merged.bin` (16MB padded) wipes it. App-only reflash lives at 0x10000. Typical:
  ```
  esptool --chip esp32s3 --port COM5 write-flash 0x0 build/DigiFrame_flash_at_0x0.bin        # keep GIFs/config
  esptool --chip esp32s3 --port COM5 write-flash 0x10000 build/DigiFrame.ino.bin              # app only, fastest
  esptool --chip esp32s3 --port COM5 write-flash 0x0 build/DigiFrame.ino.merged.bin           # factory reset (wipes LittleFS)
  ```
- **Default GIF pack:** `gifs/*.gif` are embedded in the app image via the auto-generated `default_gifs.h` (regenerate with `tools/make_default_gifs.ps1` after changing `gifs/`) and copied to LittleFS **once** on first boot (`seedDefaultGifs()`, marker `/.gifs_seeded`) — after that they are ordinary files the user can delete from the dashboard, and deletions stick. Additional GIFs are uploaded via the web dashboard (`http://digiframe.local`). Telegram GIF upload was removed intentionally.

There are no tests, linters, or CI. Primary verification is a clean arduino-cli compile.

## Runtime configuration

Compile-time **defaults** live in `config.h` (WiFi SSID/pass, `BOT_TOKEN`, `ALLOWED_CHAT_ID`, names, timezone, location, brightness, hotspot `AP_SSID`/`AP_PASS`). At runtime they are overridden by `/config.json` on LittleFS (keys `ssid`, `pass`, `tgToken`, `tgChat`, `bright`, `charMin`, `lat`, `lon`), editable from the web dashboard (`/api/wifi`, `/api/tgconfig`, `/api/loc`). Weather lat/lon live in fixed `char` buffers (`cfgLat`/`cfgLon`), not `String`, because core 1 writes them while `weatherTask` (core 0) reads. **Note:** `BOT_TOKEN` and `WIFI_PASS` are hardcoded defaults in `config.h` — treat as sensitive.

## Architecture

Single translation unit: `DigiFrame.ino` includes ordered `.h` files (order matters — later headers may call earlier ones; forward decls for `handleTelegram()`/`fetchWeather()` sit in `globals.h`). The actual include order in `DigiFrame.ino` is:

```
config → globals → gif_player → events_store → weather → scene → scroll → party → telegram → web_portal → qr_display → wifi_manager
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
| `party.h` | party mode + `/test` mode |
| `telegram.h` | bot commands, reply keyboard, inline keyboards, callback queries |
| `web_portal.h` | dashboard HTML + `/api/*` handlers + captive-portal redirect (endpoints: `GET /`, `GET /api/logs`, `GET /api/list`, `GET /api/config`, `POST /api/msg`, `/api/brightness`, `/api/play`, `/api/del`, `/api/interval`, `/api/party`, `/api/stop`, `/api/upload`, `/api/tgtest`, `/api/wifi`, `/api/tgconfig`, `/api/loc`, `/api/ota`) |
| `qr_display.h` | `renderSetupQR()` — QR on the panel in `MODE_SETUP` |
| `wifi_manager.h` | `wifiConnect`, `startPortal`/`stopPortal`, `wifiManagerTick` |

Everything runs on a dual-core FreeRTOS setup. The critical structural fact is the **core split and command queue** — get this wrong and you will race LittleFS against the DMA renderer.

- **Core 1 (`loop()`):** render loop. Owns the HUB75 DMA panel, AnimatedGIF decoder, mode state (`MODE_CLOCK/MSG/GIF/PARTY/TEST/SETUP`), `WebServer` (port 80), DNSServer processing, and `wifiManagerTick()`.
- **Core 0 tasks:** `tgTask` (Telegram polling; also applies dashboard token changes via the `tgTokenDirty` flag — only this task touches the bot client), `weatherTask`.
- **Cross-core handoff:** `tgTask` parses commands and calls `postTgCmd(...)` (single `TgRequest` slot guarded by `tgReqMutex`); `loop()` drains it and performs `openGif`/mode changes on core 1. New Telegram actions must follow this pattern.
- **Web → WiFi handoff:** `/api/wifi` sets `wifiRetryNow`; `wifiManagerTick()` (core 1) performs the actual reconnect.
- **OTA** (`/api/ota` in `web_portal.h`): flashes an uploaded app image (`DigiFrame.ino.bin`) into the spare OTA slot via `Update.h` (the `fatflash` scheme has `app0`/`app1`), then reboots. It suspends both core-0 tasks for the duration and rejects non-app images (checks the `esp_app_desc_t` magic `0xABCD5432` at offset 0x20 of the first chunk). Like the rest of the dashboard it is unauthenticated LAN-only. After an OTA the device may boot from `app1` — a serial app-only flash at 0x10000 then needs otadata cleared (flash the `_0x0` image, or `esptool erase-region 0xe000 0x2000`).
- **Logging:** `logLine()` → mutex-guarded ring buffer (`logBuf`, 40 lines) shown on the dashboard, mirrored to Serial.

### WiFi / setup-portal flow

Boot: `wifiConnect(20s)` → on failure `startPortal()` (AP `DigiFrame`/`digiframe123` + captive DNS + `MODE_SETUP` QR). While the portal is up, STA retries stored creds every 30 s; any successful connect (old router back, or new creds saved) triggers `stopPortal()`. At runtime, a sustained outage of `WIFI_FAIL_PORTAL_MIN` (5 min) reopens the portal. The QR shows join credentials first, then switches to `http://192.168.4.1` once a station connects.

### Mode invariants worth preserving

- **Double-buffered DMA** (`cfg.double_buff = true`). Draw a full frame, then `dma->flipDMABuffer()` — do not draw incrementally on the visible buffer or you will tear. The setup QR is static: it paints **both** buffers once and redraws only when its text changes.
- **Night mode** is applied every second in `loop()` and is intentionally **skipped during `MODE_PARTY` and `MODE_SETUP`** (QR must stay scannable).
- **Auto-party trigger** compares `todayMMDD()` to `lastPartyDate`, and is skipped while `portalActive`. Manual `/party` deliberately does **not** set `lastPartyDate` — preserve this distinction.
- **GIF playback:** `gifIsUserPlay` distinguishes user plays (loop forever) from ambient cameos (play once). On decode error, close and fall back to `MODE_CLOCK`.
