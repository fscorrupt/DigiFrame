# DigiFrame BLE protocol

Single source of truth for the Bluetooth Low Energy config service, shared by
the firmware (`ble_config.h`) and the cloud website (`website/ble.js`). Keep
the UUIDs and payload shapes in these two files in sync with this document.

All GATT characteristic values are capped at **512 bytes** by the ATT spec, so
every read/notify payload stays under that. Writes are chunked where needed.

## Service

| Role | UUID |
|---|---|
| Config service | `6b1d0001-2f4a-4d9b-9c3e-1a2b3c4d5e6f` |

The frame advertises this service UUID and a device name `DigiFrame-XXXX`
(last two bytes of the WiFi MAC). Web Bluetooth should filter by the service
UUID; `namePrefix: "DigiFrame"` is an optional extra filter.

## Characteristics

| Name | UUID (suffix `-2f4a-4d9b-9c3e-1a2b3c4d5e6f`) | Properties | Payload |
|---|---|---|---|
| `status` | `6b1d0002-…` | read, notify | JSON status object (below) |
| `wifi`   | `6b1d0003-…` | write | JSON `{ "ssid": "...", "pass": "..." }` |
| `control`| `6b1d0004-…` | write | JSON command (below) |
| `gifs`   | `6b1d0005-…` | read, notify | JSON array of filenames, e.g. `["/cake.gif","/c_cat.gif"]` |
| `upload` | `6b1d0006-…` | write, write-no-response | framed GIF upload (below) |
| `log`    | `6b1d0007-…` | read, notify | plain-text log tail (last ~500 bytes) |
| `events` | `6b1d0008-…` | read, notify | JSON array of special days (below) |

### `status` object

```json
{
  "ssid": "HomeWiFi",
  "chat": "123456789",
  "token": "123456...cdef",   // masked
  "lat": "12.97",
  "lon": "77.59",
  "bright": 100,
  "interval": 20,             // random-cameo minutes (0 = off)
  "mode": 0,                  // Mode enum
  "heap": 210,                // free heap KB
  "ip": "192.168.1.42",
  "wifi": "connected, IP 192.168.1.42",
  "mqttEn": false,            // Home Assistant / MQTT enabled
  "mqttHost": "192.168.1.10",
  "mqttPort": 1883,
  "mqttUser": ""
}
```

### `events` array

```json
[ { "date": "12-25", "type": "custom",   "message": "MERRY CHRISTMAS" },
  { "date": "03-14", "type": "birthday", "message": "HAPPY BIRTHDAY" } ]
```

The firmware refreshes `status`, `gifs`, `log`, and `events` once per second
while a client is connected, and notifies on change.

### `control` commands (write JSON)

| Command | JSON |
|---|---|
| Scroll message (~10 min) | `{"op":"msg","text":"HELLO"}` |
| Pin message (until stop) | `{"op":"pin","text":"HELLO"}` |
| Brightness (1–255) | `{"op":"brightness","v":120}` |
| Play a stored GIF | `{"op":"play","name":"cake.gif"}` |
| Delete a GIF | `{"op":"del","name":"cake.gif"}` |
| Random-cameo interval (min, 0=off) | `{"op":"interval","min":20}` |
| Celebrate now (empty = today's day, else generic) | `{"op":"celebrate","type":"birthday","message":"HI"}` |
| Back to clock | `{"op":"stop"}` |
| Weather location | `{"op":"loc","lat":"12.97","lon":"77.59"}` |
| Telegram config | `{"op":"tgconfig","token":"...","chat":"..."}` |
| Send a Telegram test | `{"op":"tgtest"}` |
| Add/update a special day | `{"op":"event_add","date":"12-25","type":"custom","message":"MERRY CHRISTMAS"}` |
| Delete a special day | `{"op":"event_del","date":"12-25"}` |
| Home Assistant / MQTT config | `{"op":"mqtt","enable":true,"host":"192.168.1.10","port":1883,"user":"","pass":""}` |

Each `control` write maps to the same `ctl*` function as the matching
`/api/*` HTTP endpoint, so BLE and the on-device dashboard behave identically.

### `upload` framing (GIF upload)

Each write is `[1-byte opcode][payload]`:

| Opcode | Meaning | Payload |
|---|---|---|
| `0x01` | START | JSON `{"name":"cat","pack":0}` (`pack:1` → character-pack `c_` prefix) |
| `0x02` | DATA  | raw GIF bytes (send with **write-with-response** for flow control) |
| `0x03` | END   | none → firmware writes the buffered file to LittleFS |

The firmware buffers the whole file in PSRAM (max 512 KB) and commits it on
the render core. Keep each DATA write ≤ (negotiated MTU − 4) bytes; a safe
default is ~180 bytes if the platform doesn't expose the MTU.

## Security

The service is currently **unauthenticated** ("just works" pairing), matching
the frame's LAN-only HTTP dashboard, which is also unauthenticated. BLE range
is short (same room), but if you deploy DigiFrame somewhere less private,
enable passkey pairing in `ble_config.h` and show the code on the panel.
Tracked as a future hardening item.
