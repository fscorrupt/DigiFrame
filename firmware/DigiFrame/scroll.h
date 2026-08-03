/* DigiFrame — scrolling text renderer */
#pragma once

/**********************  9. SCROLLING TEXT  ***************************/
uint32_t lastScrollAt = 0;
int currentScrollLine = 0;
String lastMeasuredStr = "";
String lines[20];
int lineWidths[20];
int totalLines = 0;
int measuredW = 0;
int scrollTextSize = 2;
int scrollMaxLines = 3;

bool renderScroll(uint16_t color, bool clearScreen = true) {
  if (millis() - lastScrollAt < 45) return false;
  lastScrollAt = millis();
  
  if (scrollText != lastMeasuredStr) {
    lastMeasuredStr = scrollText;
    currentScrollLine = 0;
    scrollX = 40;
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
    scrollTextSize = 1;
    scrollMaxLines = 7;
    
    for (int i = 0; i < totalLines; i++) {
      lineWidths[i] = getUTF8TextWidth(lines[i], scrollTextSize);
    }
    
    if (totalLines == 0) return true;
    measuredW = lineWidths[0];
  }
  
  if (totalLines == 0) return true;

  if (clearScreen) {
    dma->fillScreen(0);
    dma->setTextColor(color);
  } else {
    dma->setTextColor(color, 0);
  }
  dma->setTextWrap(false);
  dma->setTextSize(scrollTextSize);
  
  int pageIdx = currentScrollLine / scrollMaxLines;
  int linesInPage = totalLines - (pageIdx * scrollMaxLines);
  if (linesInPage > scrollMaxLines) linesInPage = scrollMaxLines;
  
  int lineHeight = (scrollTextSize == 2) ? 16 : 8;
  int lineSpacing = (scrollTextSize == 2) ? 2 : 1;
  int totalH = linesInPage * lineHeight + (linesInPage - 1) * lineSpacing;
  int startY = 1; // Render at the top of the screen
  
  for (int i = 0; i < linesInPage; i++) {
    int globalLineIdx = pageIdx * scrollMaxLines + i;
    int y = startY + i * (lineHeight + lineSpacing);
    int lineW = lineWidths[globalLineIdx];
    int x;
    if (lineW <= PANEL_W) {
      x = (PANEL_W - lineW) / 2;
    } else {
      int excess = lineW - PANEL_W;
      int drawX = (scrollX > 0) ? 0 : scrollX;
      if (drawX < -excess) drawX = -excess;
      
      x = (globalLineIdx == currentScrollLine) ? drawX : 0;
    }
    drawUTF8Text(x, y, lines[globalLineIdx], scrollTextSize);
  }
  
  drawSpark(4, 54, C_ACCENT);
  drawSpark(53, 54, C_ACCENT);
  scrollX--;
  
  int excess = measuredW - PANEL_W;
  if (excess < 0) excess = 0;
  
  if (scrollX < -excess - 30) {
    scrollX = 40;
    currentScrollLine++;
    if (currentScrollLine >= totalLines) {
      currentScrollLine = 0;
    }
    measuredW = lineWidths[currentScrollLine];
  }
  return true;
}
