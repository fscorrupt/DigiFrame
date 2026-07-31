/* DigiFrame — scrolling text renderer */
#pragma once

/**********************  9. SCROLLING TEXT  ***************************/
uint32_t lastScrollAt = 0;
int currentScrollLine = 0;
String lastMeasuredStr = "";
String lines[20];
int totalLines = 0;
int measuredW = 0;
int scrollTextSize = 2;
int scrollMaxLines = 3;

bool renderScroll(uint16_t color) {
  if (millis() - lastScrollAt < 45) return false;
  lastScrollAt = millis();
  
  if (scrollText != lastMeasuredStr) {
    lastMeasuredStr = scrollText;
    currentScrollLine = 0;
    scrollX = PANEL_W;
    totalLines = 0;
    
    int startIdx = 0;
    for (int i = 0; i <= scrollText.length(); i++) {
      if (i == scrollText.length() || scrollText[i] == '\n') {
        if (totalLines < 20) {
          lines[totalLines] = scrollText.substring(startIdx, i);
          totalLines++;
        }
        startIdx = i + 1;
      }
    }
    if (totalLines <= 3) {
      scrollTextSize = 2;
      scrollMaxLines = 3;
    } else {
      scrollTextSize = 1;
      scrollMaxLines = 7;
    }
    
    if (totalLines == 0) return true;
    measuredW = getUTF8TextWidth(lines[0], scrollTextSize);
  }
  
  if (totalLines == 0) return true;

  dma->fillScreen(0);
  dma->setTextWrap(false);
  dma->setTextSize(scrollTextSize);
  dma->setTextColor(color);
  
  int pageIdx = currentScrollLine / scrollMaxLines;
  int linesInPage = totalLines - (pageIdx * scrollMaxLines);
  if (linesInPage > scrollMaxLines) linesInPage = scrollMaxLines;
  
  int lineHeight = (scrollTextSize == 2) ? 16 : 8;
  int lineSpacing = (scrollTextSize == 2) ? 2 : 1;
  int totalH = linesInPage * lineHeight + (linesInPage - 1) * lineSpacing;
  int startY = (64 - totalH) / 2;
  
  for (int i = 0; i < linesInPage; i++) {
    int globalLineIdx = pageIdx * scrollMaxLines + i;
    int y = startY + i * (lineHeight + lineSpacing);
    int x = (globalLineIdx == currentScrollLine) ? scrollX : 0;
    drawUTF8Text(x, y, lines[globalLineIdx], scrollTextSize);
  }
  
  drawSpark(4, 54, C_ACCENT);
  drawSpark(53, 54, C_ACCENT);
  scrollX--;
  
  if (scrollX < -measuredW) {
    scrollX = PANEL_W;
    currentScrollLine++;
    if (currentScrollLine >= totalLines) {
      currentScrollLine = 0;
    }
    measuredW = getUTF8TextWidth(lines[currentScrollLine], scrollTextSize);
  }
  return true;
}
