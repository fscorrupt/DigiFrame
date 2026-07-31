/* DigiFrame — scrolling text renderer */
#pragma once

/**********************  9. SCROLLING TEXT  ***************************/
uint32_t lastScrollAt = 0;
int currentScrollLine = 0;
String lastMeasuredStr = "";
String currentLineStr = "";
int measuredW = 0;

bool renderScroll(uint16_t color) {        // returns true if a new frame was drawn
  if (millis() - lastScrollAt < 45) return false;   // scroll speed limiter
  lastScrollAt = millis();
  
  if (scrollText != lastMeasuredStr) {
    lastMeasuredStr = scrollText;
    currentScrollLine = 0;
    scrollX = PANEL_W;
    currentLineStr = "";
  }
  
  if (currentLineStr == "") {
    int lineCount = 1;
    int startIdx = 0;
    for (int i = 0; i <= scrollText.length(); i++) {
      if (i == scrollText.length() || scrollText[i] == '\n') {
        if (lineCount - 1 == currentScrollLine) {
          currentLineStr = scrollText.substring(startIdx, i);
          break;
        }
        startIdx = i + 1;
        lineCount++;
      }
    }
    if (currentLineStr == "" && currentScrollLine > 0) {
       currentScrollLine = 0;
       return true; // Wait for next frame to re-evaluate
    }
    measuredW = getUTF8TextWidth(currentLineStr, 2);
  }

  dma->fillScreen(0);
  dma->setTextWrap(false);
  dma->setTextSize(2);
  dma->setTextColor(color);
  
  // Center vertically for a single size-2 line (approx 16px high)
  dma->setCursor(scrollX, 24);
  drawUTF8Text(scrollX, 24, currentLineStr, 2);
  
  drawSpark(4, 54, C_ACCENT);
  drawSpark(53, 54, C_ACCENT);
  scrollX--;
  
  if (scrollX < -measuredW) {
    scrollX = PANEL_W;
    currentScrollLine++;
    int totalLines = 1;
    for (int i=0; i<scrollText.length(); i++) if(scrollText[i]=='\n') totalLines++;
    if (currentScrollLine >= totalLines) currentScrollLine = 0;
    currentLineStr = ""; // Force recalculate next frame
  }
  return true;
}
