# DigiFrame ⏰

A **64×64 HUB75 LED matrix smart clock** running on an ESP32-S3. It shows an
NTP clock, live weather, a living ambient scene, looping GIFs, scrolling
messages, and runs themed celebrations on your special days. Configure and
control it from the **clock's own web dashboard** on your WiFi, or **Home Assistant** (MQTT).

> **Note on this fork**: This version of DigiFrame has been extensively modified from the original upstream repository. Key differences include:
> - **Telegram Removed**: The Telegram bot integration has been completely removed to slim down the firmware and reduce dependencies.
> - **Night Mode Added**: A new Night Mode has been added which hides animations and dims the screen. It can be toggled via Home Assistant (MQTT).
> - **Color & GIF Rendering Fixes**: Fixed general GIF colors by swapping `AnimatedGIF` to Big Endian mode, and adapted the default pin configuration for an RGB matrix panel. Completely overhauled how transparent GIFs are composited, ensuring transparent pixels render perfectly over a pitch-black background without any white ghosting or artifacts.
> - **Calendar Integration**: Added robust support for integrating Home Assistant calendar events via MQTT. The calendar layout is highly optimized to save space, and the time is displayed vertically.
> - **Emoji & Multiline Support**: Built-in, lightweight UTF-8 text rendering engine that converts multi-byte Unicode strings to CP437, natively supporting German umlauts (Ä, Ö, Ü). Includes a custom 8x8 pixel-art engine that draws native emojis (e.g. ❤️, ⚠️, 🗑️, ☀️, ☁️) for calendar and HA messages. Send messages with `\n` to automatically wrap them onto multiple lines which then scroll elegantly across the screen **line by line** for improved readability.
> - **Localization & Time Formats**: Added German language support and a 12/24-hour time format toggle.
> - **Performance Enhancements**: Various performance optimizations across the UI and system.

> Source-available for **DIY / noncommercial** use — contributions welcome. Commercial use needs a [separate license](#license).

[![License: PolyForm Noncommercial](https://img.shields.io/badge/license-PolyForm%20Noncommercial-blue)](LICENSE.md)
[![Sponsor](https://img.shields.io/badge/Sponsor-%E2%9D%A4-EA4AAA?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/manoharc07)

<p align="center">
  <img src="images/clock-poster-photo.png" alt="DigiFrame — a 64×64 LED matrix smart clock" width="400">
  <img src="images/clock-with-frame.jpeg" alt="DigiFrame in its 3D-printed glass frame" width="400">
</p>

<p align="center">
  <img src="images/clock-view.gif" alt="DigiFrame clock face — time, weather and the ambient scene" width="420">
  <br><em>The clock face — time, weather and the living ambient scene.</em>
</p>

<p align="center">
  <img src="images/clock-live-video.gif" alt="DigiFrame running live in its glass frame" width="280">
  <br><em>Clock in action.</em>
</p>



## Features

- **Clock + weather** — NTP time and Open-Meteo weather (no API key), with an
  animated ambient scene and automatic night dimming.
- **GIFs** — a default pack is embedded and seeded to flash on first boot;
  upload your own (storage capacity up to ~300 GIFs). The dashboard shows **animated visual previews** of every uploaded GIF. Any `c_*.gif` joins a "character pack" that makes random
  cameos.
- **Messages** — scroll a note for a while, or pin one until you stop it.
- **Special days** — give a date a **type** (`custom` → fireworks, `birthday` →
  cake + confetti) and a message; at midnight the clock runs that themed
  celebration all day. Add them from the dashboard.
- **Customizable Colors** — independently change the color of the clock's hours, minutes, colon, seconds, date, temperature, **calendar time**, and **calendar text** natively from the web dashboard.
- **Live Status Tracking & System Overview** — the web dashboard polls the device in real-time, showing you exactly what the physical frame is rendering (Clock, specific GIF, or Message). It also features a real-time **System Overview** widget detailing total GIF storage capacity, free Heap (RAM), and PSRAM!
- **Home Assistant** — optional MQTT integration with auto-discovery: brightness,
  a message box, celebrate/stop buttons, and temperature/mode sensors.
- **Configurable**: via the on-device web dashboard.
- **OTA firmware updates** from the on-device dashboard.

## Hardware

- ESP32-S3 dev board (tested on N16R8: 16 MB flash, 8 MB OPI PSRAM)
- 64×64 HUB75 RGB LED matrix, 2.5 mm pitch — e.g. the **Waveshare P2.5 64×64** panel
- **5 V power supply** for the panel (≈2–4 A; sized for brightness), plus the
  16-pin HUB75 ribbon + a few jumper wires
- Optional: the 3D-printed enclosure below + a 3.5 mm thin glass front

## Wiring

The panel is driven over the 16-pin **HUB75E** header on the panel's **input**
side (the arrow points *away* from IN; some panels label it `J1`/`IN`). The
GPIO assignments live in [`config.h`](firmware/DigiFrame/config.h) — change them freely, just
avoid the reserved pins noted there. Defaults for the ESP32-S3:

| HUB75 signal | Meaning            | ESP32-S3 GPIO |
|--------------|--------------------|:-------------:|
| R1           | red (top half)     | 4  |
| G1           | green (top half)   | 5  |
| B1           | blue (top half)    | 6  |
| R2           | red (bottom half)  | 7  |
| G2           | green (bottom half)| 15 |
| B2           | blue (bottom half) | 16 |
| A            | row address A      | 18 |
| B            | row address B      | 8  |
| C            | row address C      | 9  |
| D            | row address D      | 10 |
| E            | row address E      | 42 |
| CLK          | pixel clock        | 41 |
| LAT / STB    | latch              | 40 |
| OE           | output enable      | 2  |
| GND          | ground             | GND (shared) |

HUB75E header layout (pin 1 is usually marked on the connector):

```
      ┌──────────┐
 R1 → │ 1     2  │ ← G1
 B1 → │ 3     4  │ ← GND
 R2 → │ 5     6  │ ← G2
 B2 → │ 7     8  │ ← E      (E on 1/32-scan 64×64 panels; GND on 1/16 panels)
  A → │ 9    10  │ ← B
  C → │11    12  │ ← D
 CLK→ │13    14  │ ← LAT
 OE → │15    16  │ ← GND
      └──────────┘

## Configuration & Tuning

The firmware is designed to be highly configurable via the `config.h` file before compilation. If you are experiencing issues with colors appearing "crushed", missing, or inverted at lower brightness settings, you may need to adjust the `PANEL_COLOR_DEPTH`:

- **PANEL_COLOR_DEPTH**: This defines how many bits are used per color channel (default is 8). 
  - `8` (64 KB RAM): The maximum quality. Extremely smooth gradients, but consumes significant internal DRAM.
  - `5` (40 KB RAM): The library default. Excellent balance of performance and color, though some panels may struggle to display dim mid-tones correctly at this depth.
  - If you encounter memory allocation errors (ESP32 crashing on boot) when using the dashboard/WiFi, lower this value to `6` or `5`.

## Home Assistant Setup
```

**Power & grounding (important):**
- Power the **panel from the 5 V supply**, into its power lugs/terminals — do
  **not** run the panel off the ESP32's 5 V pin (it can't supply the current).
- The ESP32-S3 is powered over USB (or its own 5 V).
- Tie the **PSU ground, panel ground, and ESP32 ground together** (common GND) —
  the HUB75 `GND` pins above cover the signal ground.
- `E` is **required** for 64×64 (1/32-scan) panels. Keep signal jumpers short
  (< ~15 cm) or use a HUB75 adapter board to avoid flicker/ghosting.

On first power-up the panel shows `HELLO`, then the WiFi **setup QR** — follow
*Setup & control* below.

## Enclosure (3D-printable)

A two-part frame is in [`stl/glass-frame/`](stl/glass-frame):

- `frame-box-v1.stl` — the body that holds the panel and electronics
- `frame-lid-v1.stl` — the back lid

It's designed around the **Waveshare P2.5 64×64** panel with a **3.5 mm thin
glass** sheet at the front (the glass diffuses the LEDs and gives a clean
finish). Print in PLA/PETG; the glass drops into the front recess, the panel
sits behind it, and the ESP32 + wiring tuck inside before the lid closes.

## Build & flash

Full instructions in [`FLASHING.md`](FLASHING.md). In short:

**1. Install Dependencies:**  
You can automatically install all the required libraries by running the included setup script from PowerShell:
```powershell
.\install_deps.ps1
```

**2. Compile & Upload:**
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc --build-path .\build_cache --output-dir build firmware/DigiFrame
```

Board: **ESP32S3 Dev Module**, Flash 16 MB, PSRAM **OPI PSRAM**, Partition
scheme **Custom** (uses the sketch's `partitions.csv` — 4 MB OTA app slots +
~7.9 MB data).

## Setup & control

On first boot, if the frame can't reach a stored WiFi it opens a hotspot and
shows a QR code.

### 1. On-device dashboard (any browser, incl. iPhone)

Scanning the panel QR joins the `DigiFrame` hotspot directly; the captive page
then opens at `http://192.168.4.1`. Enter your WiFi there, and once the frame
is online the same dashboard lives at `http://digiframe.local` — GIF upload and deletion,
brightness, messages, weather location, color configuration, and live logs.

### 4. Home Assistant (MQTT)

Enable MQTT and set your broker (e.g. the Mosquitto add-on) from any dashboard.
The clock announces itself to Home Assistant via MQTT discovery as a device with
brightness, a message text box, night mode controls, celebrate/stop buttons, and temperature/mode
sensors. 

**See [HomeAssistant.md](HomeAssistant.md)** for copy-paste YAML examples on how to sync your Google Calendar and control the frame's Night Mode via automations!

## How it works

Single Arduino sketch, dual-core FreeRTOS. **Core 1** owns the LED panel,
GIF decoder, web server and mode state; **core 0** runs
weather fetches and the MQTT client. A shared
[`control.h`](firmware/DigiFrame/control.h) layer holds one implementation per action, so every
front-end (HTTP dashboard, Home Assistant) behaves
identically; core-0 tasks marshal work to core 1 through a command queue (they
never touch LittleFS or the panel directly).

Architecture details and invariants are in [`CLAUDE.md`](CLAUDE.md).

## Configuration & security

`config.h` holds only compile-time **defaults** (placeholders like
`YOUR_WIFI_SSID` / `YOUR_BOT_TOKEN`); real values are entered at runtime and
persisted to `/config.json` on the device. Treat any token/password you flash
in as sensitive and don't commit real ones.

The web dashboard is **unauthenticated** and LAN-only — anyone on your network
can reach it. Keep that in mind before exposing the frame on a shared or public
network, and never port-forward it.

## Roadmap

- **Smart widgets** — now-playing, stock/finance tickers, and other
  at-a-glance widgets on the clock face.
- **Cloud relay backend** — an outbound connection from the frame to a broker,
  so a cloud page can reach the clock from anywhere. A browser can't call the
  frame's LAN API from an `https://` page (mixed content), so a relay is the
  only route. Today Home Assistant fills that role.

## Support the project

DigiFrame is free for DIY / noncommercial use. If it helped you or you just want
to say thanks, you can sponsor me on GitHub ❤️ — it funds more features:

[![Sponsor](https://img.shields.io/badge/Sponsor%20on%20GitHub-support-EA4AAA?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/manoharc07)

## License

**Source-available, noncommercial** — free for personal, hobby/DIY, educational,
and research use under the [PolyForm Noncommercial License 1.0.0](LICENSE.md).

**Commercial use** (selling the device or firmware, or bundling it into a paid
product or service) **requires a separate license** — contact
[@manoharc07](https://github.com/manoharc07) to arrange terms.
