/* DigiFrame — Telegram bot commands + menus */
#pragma once

/**********************  12. TELEGRAM  ********************************/
/* GIF uploads happen on the local dashboard (http://digiframe.local) —
 * the bot only controls playback. The main actions live on a persistent
 * reply keyboard (buttons under the text box); picking a GIF or a
 * brightness level uses inline keyboards answered as callback queries.
 * Runs on core 0 (tgTask): render/mode changes always go through
 * postTgCmd() and are applied by loop() on core 1. */

const char REPLY_KB[] =
  "[[\"\xF0\x9F\x8E\x89 Party\",\"\xE2\x8F\xB9 Stop\"],"
   "[\"\xE2\x96\xB6 Play GIF\",\"\xF0\x9F\x92\xA1 Brightness\"],"
   "[\"\xF0\x9F\x93\x8A Status\",\"\xF0\x9F\x93\x83 GIF List\"],"
   "[\"\xF0\x9F\x93\x85 Events\",\"\xF0\x9F\xA7\xAA Test\"]]";

void sendMainMenu(const String &chat) {
  bot.sendMessageWithReplyKeyboard(chat,
    "\xF0\x9F\x92\x97 DigiFrame — tap a button below, or type:\n"
    "/msg <text> — scroll for 10 min\n"
    "/pin <text> — scroll until \xE2\x8F\xB9 Stop\n"
    "/play <name>   /del <name>\n"
    "/brightness <1-255>\n"
    "/event add MM-DD Name   /event del MM-DD\n"
    "/char every <min> (0=off)   /char del <name>\n"
    "GIF upload: http://digiframe.local",
    "", REPLY_KB, true);   // resize keyboard to fit
}

/* Inline keyboard of stored GIFs — tapping one plays it. */
void sendPlayKeyboard(const String &chat) {
  String kb = "[";
  bool any = false;
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    String nm = f.name();
    if (nm.endsWith(".gif")) {
      if (any) kb += ",";
      String base = nm.substring(0, nm.length() - 4);
      kb += "[{\"text\":\"\xE2\x96\xB6 " + base + "\",\"callback_data\":\"play:/" + nm + "\"}]";
      any = true;
    }
    f = root.openNextFile();
  }
  kb += "]";
  if (!any) {
    bot.sendMessage(chat, "No GIFs stored yet — upload some at http://digiframe.local");
    return;
  }
  bot.sendMessageWithInlineKeyboard(chat, "Pick a GIF to play (\xE2\x8F\xB9 Stop to end):", "", kb);
}

/* Inline brightness presets. */
void sendBrightnessKeyboard(const String &chat) {
  bot.sendMessageWithInlineKeyboard(chat, "Pick a brightness:", "",
    "[[{\"text\":\"\xF0\x9F\x8C\x91 10\",\"callback_data\":\"bright:10\"},"
      "{\"text\":\"\xF0\x9F\x8C\x98 40\",\"callback_data\":\"bright:40\"},"
      "{\"text\":\"\xF0\x9F\x8C\x97 100\",\"callback_data\":\"bright:100\"}],"
     "[{\"text\":\"\xF0\x9F\x8C\x96 160\",\"callback_data\":\"bright:160\"},"
      "{\"text\":\"\xF0\x9F\x8C\x95 255\",\"callback_data\":\"bright:255\"}]]");
}

void handleTelegram() {
  int n = bot.getUpdates(bot.last_message_received + 1);
  if (n < 0) {
    logLine("TG poll error (getUpdates<0) — check token/TLS");
    return;
  }
  if (n > 0) logLine("TG got " + String(n) + " update(s)");
  while (n) {
    for (int i = 0; i < n; i++) {
      telegramMessage &m = bot.messages[i];
      logLine("TG from chat_id=" + m.chat_id +
              (m.text.length() ? (" text='" + m.text + "'") : String(" (non-text)")));
      if (m.chat_id != allowedChatId) {
        logLine("  -> IGNORED: chat_id != allowed (" + allowedChatId + ")");
        bot.sendMessage(m.chat_id, "Sorry, this frame belongs to someone special.");
        continue;
      }

      /* ---- inline-button taps (callback queries) ---- */
      if (m.type == "callback_query") {
        String d = m.text;
        if (d.startsWith("play:")) {
          postTgCmd(TGC_PLAY_GIF, d.substring(5));
          bot.answerCallbackQuery(m.query_id, "Playing!");
        } else if (d.startsWith("bright:")) {
          uint8_t v = constrain(d.substring(7).toInt(), 1, 255);
          userBrightness = v;
          postTgCmd(TGC_BRIGHTNESS, "", v);
          saveConfig();
          bot.answerCallbackQuery(m.query_id, "Brightness " + String(v));
        } else {
          bot.answerCallbackQuery(m.query_id);
        }
        continue;
      }

      String t = m.text;
      t.trim();

      /* ---- reply-keyboard buttons map onto the classic commands ---- */
      if      (t == "\xF0\x9F\x8E\x89 Party")      t = "/party";
      else if (t == "\xE2\x8F\xB9 Stop")           t = "/stop";
      else if (t == "\xF0\x9F\x93\x8A Status")     t = "/status";
      else if (t == "\xF0\x9F\x93\x83 GIF List")   t = "/list";
      else if (t == "\xF0\x9F\x93\x85 Events")     t = "/events";
      else if (t == "\xF0\x9F\xA7\xAA Test")       t = "/test";
      else if (t == "\xE2\x96\xB6 Play GIF")       { sendPlayKeyboard(m.chat_id);       continue; }
      else if (t == "\xF0\x9F\x92\xA1 Brightness") { sendBrightnessKeyboard(m.chat_id); continue; }

      /* ---- commands ---- */
      if (t.startsWith("/msg ") || t.startsWith("/pin ")) {
        String txt = t.substring(5); txt.toUpperCase();
        postTgCmd(t.startsWith("/msg ") ? TGC_MSG : TGC_PIN, txt);
        bot.sendMessage(m.chat_id, "On the wall!");
      }
      else if (t.startsWith("/play ")) {
        String p = "/" + t.substring(6) + ".gif";
        postTgCmd(TGC_PLAY_GIF, p);
        bot.sendMessage(m.chat_id, "Playing " + p);
      }
      else if (t == "/list") {
        String out = "Stored GIFs:\n";
        File root = LittleFS.open("/");
        File f = root.openNextFile();
        while (f) {
          String nm = f.name();
          if (nm.endsWith(".gif"))
            out += "  " + nm + "  (" + String(f.size() / 1024) + " KB)\n";
          f = root.openNextFile();
        }
        out += "\nFree: " + String((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024) + " KB";
        out += "\nUpload at http://digiframe.local";
        bot.sendMessage(m.chat_id, out);
      }
      else if (t.startsWith("/del ")) {
        String p = "/" + t.substring(5) + ".gif";
        bot.sendMessage(m.chat_id, LittleFS.remove(p) ? "Deleted." : "Not found.");
      }
      else if (t.startsWith("/char every ")) {
        int mn = t.substring(12).toInt();
        charEveryMs = (mn <= 0) ? 0 : (uint32_t)mn * 60000UL;
        saveConfig();
        bot.sendMessage(m.chat_id, mn <= 0 ? "Character cameos off."
                        : "A random character will visit every " + String(mn) + " min.");
      }
      else if (t == "/char list") {
        String out = "Character pack:\n";
        File root = LittleFS.open("/");
        File f = root.openNextFile();
        int cnt = 0;
        while (f) {
          String nm = f.name();
          if (nm.startsWith("c_") && nm.endsWith(".gif")) { out += "  " + nm + "\n"; cnt++; }
          f = root.openNextFile();
        }
        if (!cnt) out += "  (empty — upload with \"character pack\" checked on http://digiframe.local)";
        bot.sendMessage(m.chat_id, out);
      }
      else if (t.startsWith("/char del ")) {
        String p = "/c_" + t.substring(10) + ".gif";
        bot.sendMessage(m.chat_id, LittleFS.remove(p) ? "Removed from pack." : "Not found.");
      }
      else if (t.startsWith("/brightness ")) {
        userBrightness = constrain(t.substring(12).toInt(), 1, 255);
        postTgCmd(TGC_BRIGHTNESS, "", (uint8_t)userBrightness);
        saveConfig();
        bot.sendMessage(m.chat_id, "Brightness set.");
      }
      else if (t.startsWith("/event add ")) {
        // /event add 07-28 Her Birthday
        String rest = t.substring(11);
        int sp = rest.indexOf(' ');
        if (sp == 5 && numEvents < MAX_EVENTS) {
          events[numEvents++] = { rest.substring(0, 5), rest.substring(6) };
          saveEvents();
          bot.sendMessage(m.chat_id, "Event saved!");
        } else bot.sendMessage(m.chat_id, "Format: /event add MM-DD Name");
      }
      else if (t.startsWith("/event del ")) {
        String d = t.substring(11);
        bool found = false;
        for (int k = 0; k < numEvents; k++)
          if (events[k].date == d) {
            for (int j = k; j < numEvents - 1; j++) events[j] = events[j + 1];
            numEvents--; found = true; break;
          }
        if (found) saveEvents();
        bot.sendMessage(m.chat_id, found ? "Removed." : "No event on that date.");
      }
      else if (t == "/events") {
        String out = "Special days:\n";
        for (int k = 0; k < numEvents; k++)
          out += "  " + events[k].date + "  " + events[k].name + "\n";
        out += "\nAdd: /event add MM-DD Name\nRemove: /event del MM-DD";
        bot.sendMessage(m.chat_id, out);
      }
      else if (t == "/party") {
        String who = HER_NAME;
        for (int k = 0; k < numEvents; k++)
          if (events[k].date == todayMMDD()) who = events[k].name;
        postTgCmd(TGC_PARTY, who);
        bot.sendMessage(m.chat_id, "PARTY MODE! \xF0\x9F\x8E\x89 (\xE2\x8F\xB9 Stop to end)");
      }
      else if (t == "/test") {
        postTgCmd(TGC_TEST, m.chat_id);
        bot.sendMessage(m.chat_id,
          "\xF0\x9F\xA7\xAA Test mode: cycling through all screens (~4 s each).\n"
          "Clock \xC3\x97 7 weather types \xE2\x86\x92 scroll \xE2\x86\x92 party cake \xE2\x86\x92 party banner \xE2\x86\x92 GIF\n"
          "Tap \xE2\x8F\xB9 Stop to cancel early.");
      }
      else if (t == "/stop") {
        postTgCmd(TGC_STOP);
        bot.sendMessage(m.chat_id, "Back to clock.");
      }
      else if (t == "/status") {
        String evName; int d = daysToNextEvent(evName);
        char tb[32];
        snprintf(tb, sizeof(tb), "%02d:%02d:%02d", tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
        String out = "Time: " + String(tb) +
                     "\nMode: " + String((int)mode) +
                     "\nTemp: " + String(wTemp, 1) + "C (code " + String(wCode) + ")" +
                     "\nHeap: " + String(ESP.getFreeHeap() / 1024) + " KB" +
                     "\nIP: " + WiFi.localIP().toString() +
                     "\nNext event: " + (d >= 0 ? evName + " in " + String(d) + "d" : "none");
        bot.sendMessage(m.chat_id, out);
      }
      else if (t == "/help" || t == "/start" || t == "/menu") {
        sendMainMenu(m.chat_id);
      }
    }
    n = bot.getUpdates(bot.last_message_received + 1);
  }
}
