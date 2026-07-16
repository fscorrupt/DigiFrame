/* DigiFrame — party mode + test mode */
#pragma once

/**********************  11. PARTY MODE  ******************************/
void startParty(const String &who) {
  mode = MODE_PARTY;
  partyMsg = "HAPPY BIRTHDAY " + who + "!";
  partyMsg.toUpperCase();
  partyPhase = 0;
  partyPhaseAt = millis();
  dma->setBrightness8(PARTY_BRIGHTNESS);
  bool haveCake = openGif("/cake.gif", true);
  logLine("Party start (" + who + "), cake.gif=" + String(haveCake ? "yes" : "MISSING"));
  if (!haveCake) {
    partyPhase = 1;              // no gif on FS -> just scroll text
    scrollText = partyMsg;
    scrollX = PANEL_W;
  }
}
void runParty() {
  static uint32_t lastPartyTick = 0;
  if (millis() - lastPartyTick > 66) {   // keep animations ticking at ~15fps
    lastPartyTick = millis();
    frameNo++;
  }
  // alternate: 45 s of cake (GIF if present, else procedural scene), then message
  if (partyPhase == 0) {
    if (gifOpen) {
      int res = gif.playFrame(true, NULL);
      blitGifCanvas();
      dma->flipDMABuffer();
      if (res == 0) gif.reset();
    } else {
      dma->fillScreen(0);
      drawCakeAndCat(frameNo);
      dma->flipDMABuffer();
    }
    if (millis() - partyPhaseAt > 45000) {
      closeGif();
      partyPhase = 1;
      partyPhaseAt = millis();
      scrollText = partyMsg;
      scrollX = PANEL_W;
      logLine("party -> phase 1 (message)");
    }
  } else {
    if (renderScroll(C_MSG)) {
      // fireworks drawn on the same back buffer, then flip once
      drawFirework(12, 10, frameNo,      dma->color565(255, 120, 160), dma->color565(255, 220, 120));
      drawFirework(50, 52, frameNo + 40, dma->color565(140, 200, 255), dma->color565(190, 150, 255));
      dma->flipDMABuffer();
    }
    if (millis() - partyPhaseAt > 20000) {
      partyPhase = 0;
      partyPhaseAt = millis();
      if (LittleFS.exists("/cake.gif")) openGif("/cake.gif", true);
    }
  }
}

/**********************  11b. TEST MODE  ******************************/
/* /test cycles through every visual screen for ~4 s each:
 *   0  Clock – clear/sunny day
 *   1  Clock – rain
 *   2  Clock – snow
 *   3  Clock – fog
 *   4  Clock – thunderstorm
 *   5  Clock – cloudy
 *   6  Clock – clear night (stars + moon)
 *   7  Scroll message (/msg style)
 *   8  Party – procedural cake + cat (no GIF needed)
 *   9  Party – scroll banner + fireworks
 *  10  GIF  – cake.gif if present, else skip
 *  (restores original weather code and returns to clock when done) */
#define TEST_STEPS       11
#define TEST_STEP_MS   4000   // ms per step

void startTest(const String &chatId) {
  testChat       = chatId;
  testStep       = 0;
  testStepAt     = millis();
  testSavedWCode = wCode;
  mode           = MODE_TEST;
  logLine("Test mode start");
}

void runTest() {
  uint32_t ms = millis();

  // Advance step every TEST_STEP_MS
  if (ms - testStepAt > TEST_STEP_MS) {
    testStep++;
    testStepAt = ms;
    if (testStep >= TEST_STEPS) {
      // Done — restore everything
      wCode = testSavedWCode;
      closeGif();
      mode = MODE_CLOCK;
      logLine("Test mode done");
      if (testChat.length()) bot.sendMessage(testChat, "✅ Test complete — back to clock.");
      return;
    }
  }

  // Per-step setup (runs once on step change via testStepAt reset above,
  // but we key off testStep directly so rendering is correct every frame)
  switch (testStep) {
    // ---- Clock variants: just spoof wCode + hour ----
    case 0:  wCode = 0;  tmNow.tm_hour = 10; break;   // clear day
    case 1:  wCode = 61; tmNow.tm_hour = 14; break;   // rain
    case 2:  wCode = 73; tmNow.tm_hour = 9;  break;   // snow
    case 3:  wCode = 45; tmNow.tm_hour = 8;  break;   // fog
    case 4:  wCode = 95; tmNow.tm_hour = 16; break;   // thunderstorm
    case 5:  wCode = 2;  tmNow.tm_hour = 12; break;   // cloudy
    case 6:  wCode = 0;  tmNow.tm_hour = 22; break;   // clear night
    default: break;
  }

  if (testStep <= 6) {
    // Clock face with spoofed weather/time
    static uint32_t lastFace = 0;
    if (ms - lastFace > 66) { lastFace = ms; renderClock(); }
    return;
  }

  if (testStep == 7) {
    // Scroll message
    static uint32_t scrollStepAt = 0;
    if (scrollStepAt != testStepAt) {
      scrollStepAt = testStepAt;
      scrollText = "HELLO " + String(HER_NAME) + "! THIS IS A TEST MESSAGE";
      scrollX = PANEL_W;
    }
    if (renderScroll(C_MSG)) dma->flipDMABuffer();
    return;
  }

  if (testStep == 8) {
    // Procedural party – cake + cat
    static uint32_t lastTick = 0;
    if (ms - lastTick > 66) { lastTick = ms; frameNo++; }
    dma->fillScreen(0);
    drawCakeAndCat(frameNo);
    dma->flipDMABuffer();
    return;
  }

  if (testStep == 9) {
    // Party scroll + fireworks
    static uint32_t partyScrollStepAt = 0;
    if (partyScrollStepAt != testStepAt) {
      partyScrollStepAt = testStepAt;
      scrollText = "HAPPY BIRTHDAY " + String(HER_NAME) + "!";
      scrollX = PANEL_W;
    }
    static uint32_t lastTick = 0;
    if (ms - lastTick > 66) { lastTick = ms; frameNo++; }
    if (renderScroll(C_MSG)) {
      drawFirework(12, 10, frameNo,      dma->color565(255, 120, 160), dma->color565(255, 220, 120));
      drawFirework(50, 52, frameNo + 40, dma->color565(140, 200, 255), dma->color565(190, 150, 255));
      dma->flipDMABuffer();
    }
    return;
  }

  if (testStep == 10) {
    // GIF – cake.gif if present
    static uint32_t gifStepAt = 0;
    if (gifStepAt != testStepAt) {
      gifStepAt = testStepAt;
      closeGif();
      if (LittleFS.exists("/cake.gif")) openGif("/cake.gif", true);
      else { testStep++; testStepAt = ms; return; }  // skip if missing
    }
    if (gifOpen) {
      int res = gif.playFrame(true, NULL);
      blitGifCanvas();
      dma->flipDMABuffer();
      if (res == 0) gif.reset();
    }
    return;
  }
}
