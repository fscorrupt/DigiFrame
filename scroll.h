/* DigiFrame — scrolling text renderer */
#pragma once

/**********************  9. SCROLLING TEXT  ***************************/
uint32_t lastScrollAt = 0;
bool renderScroll(uint16_t color) {        // returns true if a new frame was drawn
  if (millis() - lastScrollAt < 45) return false;   // scroll speed limiter
  lastScrollAt = millis();
  dma->fillScreen(0);
  dma->setTextWrap(false);
  dma->setTextSize(2);
  dma->setTextColor(color);
  dma->setCursor(scrollX, 25);
  dma->print(scrollText);
  drawHeart(4, 54, C_HEART);
  drawHeart(53, 54, C_HEART);
  scrollX--;
  // Measure exact rendered width once per text change using getTextBounds,
  // so the loop point is pixel-perfect regardless of string content.
  static String lastMeasured = "";
  static int    measuredW    = 0;
  if (scrollText != lastMeasured) {
    int16_t x1, y1; uint16_t tw, th;
    dma->getTextBounds(scrollText, 0, 0, &x1, &y1, &tw, &th);
    measuredW    = (int)tw;
    lastMeasured = scrollText;
  }
  if (scrollX < -measuredW) scrollX = PANEL_W;
  return true;
}
