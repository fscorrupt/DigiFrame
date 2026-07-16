/* DigiFrame — on-panel QR codes for setup-hotspot mode */
#pragma once
#include <qrcode.h>   // espressif__qrcode component, bundled with the ESP32 core

/**********************  12c. SETUP QR DISPLAY  ***********************/
/* While the setup hotspot is up (MODE_SETUP) the panel shows a QR code.
 * Before a phone joins, it encodes the hotspot credentials — one scan
 * joins the network and the captive portal pops the config page. Once a
 * station is connected it switches to the portal URL, so scanning again
 * opens http://192.168.4.1 directly if the captive popup didn't appear.
 *
 * Version <= 3 (29x29 modules max) fits both texts at ECC_LOW and draws
 * at 2x = 58x58 px, centered. QR wants dark-on-light, so the background
 * is lit and modules are unlit LEDs. The image is static — we only
 * redraw (both DMA buffers) when the encoded text changes. */

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
                 : String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASS + ";;";
  if (txt == qrLastText) return;          // static image — redraw only on change
  qrLastText = txt;

  esp_qrcode_config_t cfg = {
    .display_func       = qrToPanel,
    .max_qrcode_version = 3,              // 29x29 -> 58x58 px at 2x
    .qrcode_ecc_level   = ESP_QRCODE_ECC_LOW,
  };
  if (esp_qrcode_generate(&cfg, txt.c_str()) != ESP_OK) {
    logLine("QR encode failed for: " + txt);
    return;
  }
  logLine("QR shown: " + txt);
}
