/* DigiFrame — Live location tracking map downloader and renderer */
#pragma once
#include <HTTPClient.h>
#include <JPEGDEC.h>

JPEGDEC jpeg;

// JPEGDEC drawing callback
int jpegDraw(JPEGDRAW *pDraw) {
  if (trackerMutex) xSemaphoreTake(trackerMutex, portMAX_DELAY);
  for (int y = 0; y < pDraw->iHeight; y++) {
    for (int x = 0; x < pDraw->iWidth; x++) {
      uint16_t color = pDraw->pPixels[y * pDraw->iWidth + x];
      // Dim the background to make text readable (divide RGB565 by 2)
      uint16_t r = (color >> 11) & 0x1F;
      uint16_t g = (color >> 5) & 0x3F;
      uint16_t b = color & 0x1F;
      r >>= 1; g >>= 1; b >>= 1;
      uint16_t dimmed = (r << 11) | (g << 5) | b;
      dma->drawPixel(pDraw->x + x, pDraw->y + y, dimmed);
    }
  }
  if (trackerMutex) xSemaphoreGive(trackerMutex);
  return 1;
}

// Background HTTP downloader running on Core 0
void trackerTask(void *pv) {
  for (;;) {
    if (trackerMapDirty && tMapUrl.length() > 0 && WiFi.status() == WL_CONNECTED) {
      trackerMapDirty = false;
      logLine("Tracker: downloading map...");
      
      HTTPClient http;
      http.setTimeout(10000); // 10s timeout
      if (http.begin(tMapUrl)) {
        int code = http.GET();
        if (code == 200) {
          int len = http.getSize();
          if (len > 0) {
            // Allocate PSRAM buffer if needed
            if (trackerImgAlloc < len) {
              if (trackerMutex) xSemaphoreTake(trackerMutex, portMAX_DELAY);
              if (trackerImgBuf) free(trackerImgBuf);
              trackerImgBuf = (uint8_t*)ps_malloc(len);
              trackerImgAlloc = trackerImgBuf ? len : 0;
              if (trackerMutex) xSemaphoreGive(trackerMutex);
            }
            if (trackerImgBuf) {
              WiFiClient *stream = http.getStreamPtr();
              int c = stream->readBytes(trackerImgBuf, len);
              if (trackerMutex) xSemaphoreTake(trackerMutex, portMAX_DELAY);
              trackerImgSize = c;
              if (trackerMutex) xSemaphoreGive(trackerMutex);
              logLine("Tracker: map downloaded (" + String(c) + " bytes)");
            } else {
              logLine("Tracker: PSRAM malloc failed");
            }
          }
        } else {
          logLine("Tracker HTTP failed: " + String(code));
        }
        http.end();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Render mode, running on Core 1 in loop()
void renderTracker() {
  dma->fillScreen(0);
  
  // 1. Draw the dimmed map background
  if (trackerMutex) xSemaphoreTake(trackerMutex, portMAX_DELAY);
  if (trackerImgBuf && trackerImgSize > 0) {
    if (jpeg.openRAM(trackerImgBuf, trackerImgSize, jpegDraw)) {
      jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
      jpeg.decode(0, 0, 0);
      jpeg.close();
    }
  }
  if (trackerMutex) xSemaphoreGive(trackerMutex);

  // 2. Draw text on top
  dma->setTextColor(dma->color565(255, 255, 255));
  drawUTF8Text(2, 4, tPerson, 1);
  
  dma->setTextColor(dma->color565(100, 255, 100)); // Bright Green
  String distStr = tDist + " (" + tEta + ")";
  drawUTF8Text(2, 20, distStr, 1);

  dma->setTextColor(dma->color565(255, 200, 100)); // Orange
  drawUTF8Text(2, 36, tStreet, 1);
}
