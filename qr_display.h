/* DigiFrame — on-panel QR codes for setup-hotspot mode */
#pragma once
#include <qrcode.h>   // espressif__qrcode component, bundled with the ESP32 core

/**********************  12c. SETUP QR DISPLAY  ***********************/
/* While the setup hotspot is up (MODE_SETUP) the panel shows a QR code.
 * Before a phone joins, it encodes the CLOUD DASHBOARD URL (with this
 * frame's BLE name in the fragment) — scanning opens the site, which pairs
 * over Web Bluetooth to provision WiFi (Android/desktop Chrome). iPhone or
 * no-Bluetooth users instead join the "DigiFrame" hotspot; once a station
 * connects the QR switches to http://192.168.4.1 so the on-device
 * dashboard opens directly if the captive popup didn't appear.
 *
 * The URL is longer than the old WIFI: payload, so we allow up to version 6
 * (41x41 modules). qrToPanel scales by PANEL_W/size (2x at v<=3, 1x above),
 * always centered. QR wants dark-on-light, so the background is lit and
 * modules are unlit LEDs. Static image — redraw only when the text changes. */

String qrLastText = "";

/* esp_qrcode_generate() hands the finished code to this callback. */
static void qrToPanel(esp_qrcode_handle_t qrcode) {
  int size  = esp_qrcode_get_size(qrcode);          // 21/25/29 modules
  int scale = PANEL_W / size;                       // 2 for all of those
  int off   = (PANEL_W - size * scale) / 2;
  uint16_t bg = dma->color565(140, 140, 150);       // readable, not blinding
  for (int b = 0; b < 2; b++) {                     // paint both DMA buffers
    dma->fillScreen(bg);
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++)
        if (esp_qrcode_get_module(qrcode, x, y))
          dma->fillRect(off + x * scale, off + y * scale, scale, scale, 0);
    dma->flipDMABuffer();
  }
}

void renderSetupQR() {
  String txt = (WiFi.softAPgetStationNum() > 0)
                 ? String("http://192.168.4.1")
                 : String(CLOUD_SITE_URL) + "/#d=" + bleDeviceName();
  if (txt == qrLastText) return;          // static image — redraw only on change
  qrLastText = txt;

  esp_qrcode_config_t cfg = {
    .display_func       = qrToPanel,
    .max_qrcode_version = 6,              // up to 41x41 -> 41 px at 1x (fits the URL)
    .qrcode_ecc_level   = ESP_QRCODE_ECC_LOW,
  };
  if (esp_qrcode_generate(&cfg, txt.c_str()) != ESP_OK) {
    logLine("QR encode failed for: " + txt);
    return;
  }
  logLine("QR shown: " + txt);
}
