# DigiFrame website

A static Progressive Web App that sets up and controls a DigiFrame LED smart
clock **over Web Bluetooth** — no backend, no accounts. It's the cloud-hosted
counterpart to the clock's on-device dashboard.

Scanning the QR code on the panel opens this site with the frame's Bluetooth
name in the URL fragment (`/#d=DigiFrame-XXXX`); tapping **Connect over
Bluetooth** pairs with the frame to provision WiFi and configure everything.

## Requirements

- **Web Bluetooth**: Chrome or Edge on **Android, Windows, macOS, Linux**.
  iOS/iPadOS Safari does **not** support Web Bluetooth — those users fall back
  to the frame's on-device dashboard (the page detects this and shows how).
- Served over **HTTPS** (required by Web Bluetooth). Any static host works.

## Files

| File | Purpose |
|---|---|
| `index.html` | app shell + dashboard markup + no-Bluetooth fallback |
| `style.css` | pink/dark theme matching the panel |
| `ble.js` | Web Bluetooth transport (UUIDs mirror `../BLE_PROTOCOL.md`) |
| `app.js` | UI wiring: connect gate, controls, GIF upload, live logs |
| `manifest.webmanifest`, `sw.js` | installable / offline PWA |
| `netlify.toml` | Netlify deploy config |

The BLE contract lives in [`../BLE_PROTOCOL.md`](../BLE_PROTOCOL.md); keep
`ble.js` and the firmware's `ble_config.h` in sync with it.

## Deploy

Everything here is static — just publish the `website/` folder.

- **Netlify**: New site → set *Base directory* and *Publish directory* to
  `website`. `netlify.toml` handles the rest.
- **Vercel**: Import the repo → *Root directory* = `website`, framework
  "Other", no build command.
- **Cloudflare Pages**: create a project → build command empty, *Build output
  directory* = `website`.

After deploying, set the firmware's `CLOUD_SITE_URL` in `config.h` to your
site URL so the on-panel QR points at it, then reflash.

## Local testing

Web Bluetooth needs a secure context. `localhost` counts as secure, so:

```
cd website
npx serve            # or: python -m http.server 8000
```

Open `http://localhost:3000` (or `:8000`) in Chrome/Edge with a powered-on
frame nearby. To test the QR flow, append `#d=DigiFrame-XXXX`.
