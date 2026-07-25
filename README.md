# DigiFrame ⏰

A **64×64 HUB75 LED matrix smart clock** running on an ESP32-S3. It shows an
NTP clock, live weather, a living ambient scene, looping GIFs, scrolling
messages, and runs themed celebrations on your special days. Configure and
control it from a **cloud dashboard over Bluetooth**, the **clock's own web
dashboard** on your WiFi, a **Telegram bot**, or **Home Assistant** (MQTT).

> Open source — contributions welcome.

## Features

- **Clock + weather** — NTP time and Open-Meteo weather (no API key), with an
  animated ambient scene and automatic night dimming.
- **GIFs** — a default pack is embedded and seeded to flash on first boot;
  upload your own. Any `c_*.gif` joins a "character pack" that makes random
  cameos.
- **Messages** — scroll a note for a while, or pin one until you stop it.
- **Special days** — give a date a **type** (`custom` → fireworks, `birthday` →
  cake + confetti) and a message; at midnight the clock runs that themed
  celebration all day. Add them from any dashboard or Telegram.
- **Telegram bot** — control playback, messages, brightness, special days and
  more from anywhere, with tap-able button menus.
- **Home Assistant** — optional MQTT integration with auto-discovery: brightness,
  a message box, celebrate/stop buttons, and temperature/mode sensors.
- **Three ways to configure**: cloud site over Bluetooth, the on-device web
  dashboard, or Telegram.
- **OTA firmware updates** from the on-device dashboard.

## Hardware

- ESP32-S3 (tested on N16R8: 16 MB flash, 8 MB OPI PSRAM)
- 64×64 HUB75 LED matrix (P2.5)
- 5 V power supply sized for the panel

Pin map is in [`config.h`](config.h). Wire `E` for 1/32-scan 64×64 panels.

## Build & flash

Full instructions in [`FLASHING.md`](FLASHING.md). In short:

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom --output-dir build .
```

Board: **ESP32S3 Dev Module**, Flash 16 MB, PSRAM **OPI PSRAM**, Partition
scheme **Custom** (uses the sketch's `partitions.csv` — 4 MB OTA app slots +
~7.9 MB data). Libraries (Library Manager): `ESP32 HUB75 LED MATRIX PANEL DMA
Display`, `Adafruit GFX Library`, `AnimatedGIF`, `UniversalTelegramBot`,
`ArduinoJson`, `NimBLE-Arduino`, `PubSubClient`.

## Setup & control

On first boot, if the frame can't reach a stored WiFi it opens a hotspot and
shows a QR code.

### 1. Cloud dashboard over Bluetooth (Android / Chrome / Edge)

The panel QR opens a **cloud-hosted static site** (in [`website/`](website/))
that pairs with the frame over **Web Bluetooth** — no backend, no account. Use
it to set WiFi, brightness, messages, GIFs, weather location, Telegram config,
and watch live logs. Host it yourself (Netlify/Vercel/Cloudflare Pages — see
[`website/README.md`](website/README.md)) and point `CLOUD_SITE_URL` in
`config.h` at your URL.

> iPhone/iPad Safari doesn't support Web Bluetooth — those users use path 2.

### 2. On-device dashboard on your WiFi (any browser, incl. iPhone)

Join the `DigiFrame` hotspot (or, once online, open `http://digiframe.local`)
and use the frame's built-in dashboard — the same controls, served locally.
This is also the recovery path if Bluetooth is unavailable.

### 3. Telegram bot (anywhere)

Create a bot with @BotFather, set the token + your chat id (via any dashboard),
and control the clock remotely. Send `/menu` for the button menu.

### 4. Home Assistant (MQTT)

Enable MQTT and set your broker (e.g. the Mosquitto add-on) from any dashboard.
The clock announces itself to Home Assistant via MQTT discovery as a device with
brightness, a message text box, celebrate/stop buttons, and temperature/mode
sensors. Off by default.

## How it works

Single Arduino sketch, dual-core FreeRTOS. **Core 1** owns the LED panel,
GIF decoder, web server and mode state; **core 0** runs Telegram polling,
weather fetches, the NimBLE host, and the MQTT client. A shared
[`control.h`](control.h) layer holds one implementation per action, so every
front-end (HTTP dashboard, Bluetooth, Telegram, Home Assistant) behaves
identically; core-0 tasks marshal work to core 1 through a command queue (they
never touch LittleFS or the panel directly).

The BLE contract is documented in [`BLE_PROTOCOL.md`](BLE_PROTOCOL.md).
Architecture details and invariants are in [`CLAUDE.md`](CLAUDE.md).

## Configuration & security

`config.h` holds only compile-time **defaults** (placeholders like
`YOUR_WIFI_SSID` / `YOUR_BOT_TOKEN`); real values are entered at runtime and
persisted to `/config.json` on the device. Treat any token/password you flash
in as sensitive and don't commit real ones.

Both config surfaces are **unauthenticated**: the web dashboard is LAN-only,
and the BLE service uses "just works" pairing (short range, same room). If you
deploy DigiFrame somewhere less private, enable passkey pairing in
`ble_config.h`. See the security note in `BLE_PROTOCOL.md`.

## Roadmap

- **Smart widgets** — now-playing, stock/finance tickers, and other
  at-a-glance widgets on the clock face.
- **Cloud relay backend** — optional server so the website can reach the clock
  from anywhere (and cover iPhone), instead of Bluetooth-only. Today Telegram
  and Home Assistant fill that role.
- Passkey BLE pairing shown on the panel, on by default.

## License

See `LICENSE` (add one before publishing — MIT is a good default for this).
