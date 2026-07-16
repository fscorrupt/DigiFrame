/* DigiFrame — Open-Meteo fetch + weather icons */
#pragma once

/**********************  7. WEATHER  **********************************/
void fetchWeather() {
  WiFiClientSecure c;
  c.setInsecure();
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + cfgLat +
               "&longitude=" + cfgLon + "&current=temperature_2m,weather_code";
  if (http.begin(c, url) && http.GET() == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      wTemp = doc["current"]["temperature_2m"].as<float>();
      wCode = doc["current"]["weather_code"].as<int>();
    }
  }
  http.end();
}
// tiny 9x8 procedural weather icons at (x,y)
void drawWeatherIcon(int x, int y) {
  uint16_t YEL = dma->color565(255, 220, 100);
  uint16_t GRY = dma->color565(150, 160, 175);
  uint16_t BLU = dma->color565(120, 180, 255);
  uint16_t WHT = dma->color565(230, 235, 245);
  if (wCode < 0) return;
  if (wCode == 0) {                                  // clear
    dma->fillCircle(x + 4, y + 3, 2, YEL);
    dma->drawPixel(x + 4, y,     YEL); dma->drawPixel(x + 4, y + 6, YEL);
    dma->drawPixel(x + 1, y + 3, YEL); dma->drawPixel(x + 7, y + 3, YEL);
  } else if (wCode <= 3) {                           // partly cloudy
    dma->fillCircle(x + 2, y + 2, 2, YEL);
    dma->fillRoundRect(x + 2, y + 3, 7, 4, 2, GRY);
  } else if (wCode <= 48) {                          // fog
    for (int i = 0; i < 3; i++) dma->drawFastHLine(x, y + 1 + i * 3, 9, GRY);
  } else if (wCode <= 82) {                          // drizzle / rain
    dma->fillRoundRect(x + 1, y, 7, 4, 2, GRY);
    dma->drawPixel(x + 2, y + 6, BLU); dma->drawPixel(x + 4, y + 7, BLU);
    dma->drawPixel(x + 6, y + 6, BLU);
  } else if (wCode <= 86) {                          // snow (unlikely in BLR!)
    dma->fillRoundRect(x + 1, y, 7, 4, 2, GRY);
    dma->drawPixel(x + 3, y + 6, WHT); dma->drawPixel(x + 5, y + 7, WHT);
  } else {                                           // thunderstorm
    dma->fillRoundRect(x + 1, y, 7, 4, 2, GRY);
    dma->drawPixel(x + 4, y + 5, YEL); dma->drawPixel(x + 3, y + 6, YEL);
    dma->drawPixel(x + 4, y + 7, YEL);
  }
}

/* ---------- animated footer weather icon (~11x10, two-tone) ----------
   Clear day: sun with rotating rays. Clear night: moon + twinkling star.
   Cloudy: sun peeking behind a gently drifting cloud. Fog: shimmering
   waves. Rain/snow: falling drops/flakes. Storm: flashing bolt. */
void drawWeatherIconAnim(int x, int y, uint32_t f) {
  if (wCode < 0) return;
  bool night = (tmNow.tm_hour >= 20 || tmNow.tm_hour < 5);
  uint16_t SUN1 = dma->color565(255, 214, 90), SUN2 = dma->color565(255, 160, 70);
  uint16_t CL1  = dma->color565(225, 230, 245), CL2 = dma->color565(150, 160, 180);
  uint16_t BLU  = dma->color565(110, 180, 255), WHT = dma->color565(235, 240, 250);
  uint16_t BOLT = dma->color565(255, 220, 80),  MOON = dma->color565(225, 228, 240);

  if (wCode == 0) {
    if (night) {                                   // moon + twinkling star
      dma->fillCircle(x + 5, y + 4, 3, MOON);
      dma->fillCircle(x + 7, y + 3, 3, 0);
      if ((f / 8) % 2) dma->drawPixel(x + 10, y + 1, WHT);
      else             dma->drawPixel(x + 1, y + 7, WHT);
    } else {                                       // sun, rays rotate +/x
      dma->fillCircle(x + 5, y + 4, 2, SUN1);
      dma->drawPixel(x + 5, y + 4, SUN2);          // warm core
      if ((f / 6) % 2) {
        dma->drawPixel(x + 5, y,     SUN1); dma->drawPixel(x + 5, y + 8, SUN1);
        dma->drawPixel(x + 1, y + 4, SUN1); dma->drawPixel(x + 9, y + 4, SUN1);
      } else {
        dma->drawPixel(x + 2, y + 1, SUN2); dma->drawPixel(x + 8, y + 1, SUN2);
        dma->drawPixel(x + 2, y + 7, SUN2); dma->drawPixel(x + 8, y + 7, SUN2);
      }
    }
  } else if (wCode <= 3) {                         // sun peeking, cloud drifts
    dma->fillCircle(x + 3, y + 3, 2, SUN1);
    int d = (f / 10) % 4; if (d == 3) d = 1;       // 0,1,2,1 gentle drift
    dma->fillRoundRect(x + 2 + d, y + 4, 8, 4, 2, CL1);
    dma->drawFastHLine(x + 3 + d, y + 7, 6, CL2);  // shaded underside
  } else if (wCode <= 48) {                        // fog: shimmering waves
    for (int b = 0; b < 3; b++) {
      int off = ((f / 8) + b * 2) % 4;
      dma->drawFastHLine(x + off, y + 2 + b * 3, 7, (b % 2) ? CL2 : CL1);
    }
  } else if (wCode <= 67 || (wCode >= 80 && wCode <= 82)) {  // rain
    dma->fillRoundRect(x + 1, y, 9, 4, 2, CL1);
    dma->drawFastHLine(x + 2, y + 3, 7, CL2);
    for (int k = 0; k < 3; k++) {
      int ry = y + 5 + (int)((f / 3 + k * 2) % 5);
      dma->drawPixel(x + 2 + k * 3, ry, BLU);
    }
  } else if (wCode <= 86) {                        // snow: drifting flakes
    dma->fillRoundRect(x + 1, y, 9, 4, 2, CL1);
    dma->drawFastHLine(x + 2, y + 3, 7, CL2);
    for (int k = 0; k < 3; k++) {
      int ry = y + 5 + (int)((f / 5 + k * 2) % 5);
      dma->drawPixel(x + 2 + k * 3 + (int)((f / 7 + k) % 2), ry, WHT);
    }
  } else {                                         // storm: flashing bolt
    dma->fillRoundRect(x + 1, y, 9, 4, 2, CL2);
    if ((f / 5) % 3 != 0) {
      dma->drawPixel(x + 5, y + 4, BOLT);
      dma->drawPixel(x + 4, y + 5, BOLT);
      dma->drawPixel(x + 5, y + 6, BOLT);
      dma->drawPixel(x + 4, y + 7, BOLT);
    }
  }
}
