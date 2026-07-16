# Flashing DigiFrame

Prebuilt binaries live in `build/` after a compile.

## Build (command line)

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=fatflash --output-dir build .
```

(Or Arduino IDE: Sketch → Export Compiled Binary with the board settings
from the header of `DigiFrame.ino`.)

## Flash — pick ONE of these

### 0) OTA — over WiFi, no cable (easiest once the frame is running)

Open the dashboard (http://digiframe.local) → **Firmware (OTA)** →
pick `build/DigiFrame.ino.bin` → Update firmware. The panel shows
"UPDATING" with progress and the frame reboots into the new firmware.
Stored GIFs/config are untouched.

Only the plain app image works here — the `_0x0` / `merged` images are
rejected on purpose ("not an app image").

> Serial caveat after an OTA: the frame may now boot from the second
> app slot (`app1`), so a later **serial** app-only flash at `0x10000`
> can appear to do nothing. Either flash the `_0x0` image (it resets
> the boot selection) or first run
> `esptool --chip esp32s3 --port COM5 erase-region 0xe000 0x2000`.

The serial options below — replace `COM5` with your port (check Device
Manager). Hold BOOT while plugging in if the port doesn't appear.

### A) Full image, keeps stored GIFs/config (recommended)

`DigiFrame_flash_at_0x0.bin` contains bootloader + partition table +
app and stops before the data partition, so LittleFS (GIFs, events,
config) survives:

```
esptool --chip esp32s3 --port COM5 write-flash 0x0 build/DigiFrame_flash_at_0x0.bin
```

Also works with a browser flasher (e.g. https://espressif.github.io/esptool-js/)
— address `0x0`.

### B) App only (fastest, after the first flash)

```
esptool --chip esp32s3 --port COM5 write-flash 0x10000 build/DigiFrame.ino.bin
```

### C) Factory reset — wipes EVERYTHING including stored GIFs

`DigiFrame.ino.merged.bin` is the padded 16 MB image; flashing it
erases the LittleFS partition too:

```
esptool --chip esp32s3 --port COM5 write-flash 0x0 build/DigiFrame.ino.merged.bin
```

## First boot / new WiFi

If the frame can't reach the stored WiFi it starts a hotspot and shows
a QR code on the panel:

1. Scan the QR → phone joins `DigiFrame` (pass `digiframe123`).
2. The config page pops up (or open http://192.168.4.1, or scan the QR
   again once connected — it switches to the URL).
3. Enter your WiFi in the **WiFi** section → Save & connect.
4. The frame connects and returns to the clock; the dashboard is then
   at http://digiframe.local. If the old network reappears instead, the
   frame rejoins it automatically.
