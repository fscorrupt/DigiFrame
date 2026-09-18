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

    // Start with the first line that actually needs scrolling
    currentScrollLine = 0;
    for (int i = 0; i < totalLines; i++) {
      if (lineWidths[i] > PANEL_W) {
        currentScrollLine = i;
        break;
      }
    }
    measuredW = lineWidths[currentScrollLine];
    scrollX = 0;
  }
  
  if (totalLines == 0) return true;

  int pageIdx = currentScrollLine / scrollMaxLines;
  int linesInPage = totalLines - (pageIdx * scrollMaxLines);
  if (linesInPage > scrollMaxLines) linesInPage = scrollMaxLines;
  
  int lineHeight = (scrollTextSize == 2) ? 16 : 8;
  int lineSpacing = (scrollTextSize == 2) ? 2 : 1;
  int totalH = linesInPage * lineHeight + (linesInPage - 1) * lineSpacing;
  int startY = 1; // Render at the top of the screen

  if (clearScreen) {
    dma->fillScreen(0);
    dma->setTextColor(color);
  } else {
    // Solid black backing across the message area so GIF doesn't bleed through
    dma->fillRect(0, 0, PANEL_W, totalH + 2, 0);
    dma->setTextColor(color, 0);
  }
  dma->setTextWrap(false);
  dma->setTextSize(scrollTextSize);
  
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
  
  if (clearScreen) {
    drawSpark(4, 54, C_ACCENT);
    drawSpark(53, 54, C_ACCENT);
  }

  bool anyNeedsScroll = false;
  for (int i = 0; i < totalLines; i++) {
    if (lineWidths[i] > PANEL_W) {
      anyNeedsScroll = true;
      break;
    }
  }

  if (anyNeedsScroll) {
    scrollX--;
    int excess = measuredW - PANEL_W;
    if (excess < 0) excess = 0;
    
    if (scrollX < -excess - 25) {
      // Find the next line that exceeds PANEL_W
      int nextLine = currentScrollLine;
      for (int step = 1; step <= totalLines; step++) {
        int idx = (currentScrollLine + step) % totalLines;
        if (lineWidths[idx] > PANEL_W) {
          nextLine = idx;
          break;
        }
      }
      currentScrollLine = nextLine;
      measuredW = lineWidths[currentScrollLine];
      scrollX = 0;
    }
  }
  return true;
}
