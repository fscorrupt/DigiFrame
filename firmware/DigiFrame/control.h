/* DigiFrame — shared control layer (one implementation per action) */
#pragma once

/**********************  10b. CONTROL LAYER  **************************/
/* Every config / command action lives here exactly once, so the two
 * front-ends stay in lock-step and can't drift:
 *   - the local HTTP dashboard (web_portal.h) runs on core 1 and calls
 *     these ctl* functions directly;
 *   - the Telegram bot (telegram.h) and the Home Assistant MQTT client
 *     (mqtt_ha.h) run on core 0 and must marshal to core 1 via
 *     postTgCmd() — loop() then calls the ctl*.
 * ALL of these run on core 1: they touch LittleFS / the DMA panel /
 * openGif / saveConfig, none of which are safe from a core-0 task. */

void ctlSendMsg(const String &text, bool pin) {
  scrollText = text;
  scrollText.toUpperCase();
  scrollX = PANEL_W;
  closeGif();
  mode      = MODE_MSG;
  msgEndsAt = pin ? 0 : (millis() + MSG_MINUTES * 60000UL);
}

void ctlSetBrightness(int v) {
  userBrightness = constrain(v, 1, 255);
  dma->setBrightness8(userBrightness);
  saveConfig();
}

bool ctlPlayGif(const String &name) {          // name = "foo.gif" or "/foo.gif"
  String p = name.startsWith("/") ? name : "/" + name;
  if (openGif(p, true)) { mode = MODE_GIF; return true; }
  return false;
}

bool ctlDelGif(const String &name) {
  String p = name.startsWith("/") ? name : "/" + name;
  return LittleFS.remove(p);
}

void ctlSetInterval(int minutes) {
  charEveryMs = (minutes <= 0) ? 0 : (uint32_t)minutes * 60000UL;
  saveConfig();
}

void ctlCelebrate() {                          // celebrate today's special day, else a generic one
  SpecialDay *e = todaysEvent();
  if (e) startCelebration(e->type, e->message);
  else   startCelebration("custom", "");       // startCelebration supplies a default banner
}

/* ---- special days (date + type + message) ---- */
bool ctlAddEvent(const String &date, const String &type, const String &message) {
  String d = date; d.trim();
  if (d.length() != 5 || d[2] != '-') return false;       // "MM-DD"
  String t = (type == "birthday") ? "birthday" : "custom";
  for (int i = 0; i < numEvents; i++)                     // update an existing date in place
    if (events[i].date == d) { events[i] = { d, t, message }; saveEvents(); logLine("event updated " + d); return true; }
  if (numEvents >= MAX_EVENTS) return false;
  events[numEvents++] = { d, t, message };
  saveEvents();
  logLine("event added " + d + " (" + t + ")");
  return true;
}
bool ctlDelEvent(const String &date) {
  String d = date; d.trim();
  for (int k = 0; k < numEvents; k++)
    if (events[k].date == d) {
      for (int j = k; j < numEvents - 1; j++) events[j] = events[j + 1];
      numEvents--;
      saveEvents();
      logLine("event deleted " + d);
      return true;
    }
  return false;
}
String ctlListEventsJson() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < numEvents; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["date"]    = events[i].date;
    o["type"]    = events[i].type;
    o["message"] = events[i].message;
  }
  String out; serializeJson(doc, out); return out;
}
/* BLE passes the whole event as JSON {date,type,message} */
bool ctlAddEventJson(const String &json) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;
  return ctlAddEvent(d["date"] | "", d["type"] | "custom", d["message"] | "");
}

/* ---- Home Assistant / MQTT config ---- */
void ctlSetMqtt(bool en, const String &host, int port, const String &user, const String &pass) {
  mqttEnable = en;
  mqttHost = host; mqttHost.trim();
  if (port > 0) mqttPort = port;
  mqttUser = user;
  if (pass.length()) mqttPass = pass;          // keep existing if the field is left blank
  saveConfig();
  mqttConfigDirty = true;                       // mqttTask reconnects on core 0
  logLine("MQTT config updated (enable=" + String(en ? "yes" : "no") + ", host=" + mqttHost + ")");
}
void ctlSetMqttJson(const String &json) {
  JsonDocument d;
  if (deserializeJson(d, json)) return;
  ctlSetMqtt(d["enable"] | false, d["host"] | "", d["port"] | 1883, d["user"] | "", d["pass"] | "");
}

void ctlStop() {
  if (mode == MODE_TEST) wCode = testSavedWCode;  // restore spoofed weather
  closeGif();
  mode = MODE_CLOCK;
}

bool ctlSetLoc(const String &lat, const String &lon) {
  String la = lat; la.trim();
  String lo = lon; lo.trim();
  float flat = la.toFloat(), flon = lo.toFloat();
  if (!la.length() || !lo.length() || flat < -90 || flat > 90 || flon < -180 || flon > 180)
    return false;
  strlcpy(cfgLat, la.c_str(), sizeof(cfgLat));
  strlcpy(cfgLon, lo.c_str(), sizeof(cfgLon));
  saveConfig();
  weatherNow = true;                           // weatherTask refetches right away
  logLine("location updated -> " + la + "," + lo);
  return true;
}

bool ctlSetWifi(const String &ssid, const String &pass) {
  String s = ssid; s.trim();
  if (!s.length()) return false;
  cfgWifiSsid = s;
  cfgWifiPass = pass;
  saveConfig();
  wifiRetryNow = true;                         // wifiManagerTick() reconnects on core 1
  logLine("WiFi creds updated -> '" + cfgWifiSsid + "'");
  return true;
}

void ctlSetTg(const String &token, const String &chat) {
  String tk = token; tk.trim();
  String ch = chat;  ch.trim();
  if (tk.length()) { botToken = tk; tgTokenDirty = true; }  // tgTask applies the token
  if (ch.length()) allowedChatId = ch;
  saveConfig();
  logLine("Telegram config updated (chat_id=" + allowedChatId + ")");
}

void ctlSetTz(int seconds) {
  tzOffsetSec = constrain(seconds, -12 * 3600, 14 * 3600);
  configTime(tzOffsetSec, 0, "pool.ntp.org", "time.google.com");  // re-apply offset live
  saveConfig();
  logLine("timezone set: UTC" + String(tzOffsetSec >= 0 ? "+" : "") + String(tzOffsetSec / 3600.0, 2) + "h");
}

void ctlTgTest() {
  if (WiFi.status() != WL_CONNECTED) { logLine("tgtest: no WiFi"); return; }
  logLine("tgtest: sending to " + allowedChatId + " ...");
  bool ok = bot.sendMessage(allowedChatId, "DigiFrame test message", "");
  logLine("tgtest: sendMessage returned " +
          String(ok ? "true (check your phone)" : "FALSE — token/chat_id bad"));
}

/* Write an uploaded GIF (buffered by the BLE task) to LittleFS. Runs on
 * core 1 via TGC_GIF_COMMIT; the caller frees the buffer afterwards. */
bool ctlCommitGif(const String &name, bool pack, const uint8_t *buf, size_t len) {
  if (!buf || !len) return false;
  String nm = name; nm.replace(" ", "_");
  if (!nm.endsWith(".gif")) nm += ".gif";
  if (pack && !nm.startsWith("c_")) nm = "c_" + nm;
  if (!nm.startsWith("/")) nm = "/" + nm;
  File f = LittleFS.open(nm, "w");
  if (!f) { logLine("BLE GIF upload FAILED to open " + nm); return false; }
  size_t w = f.write(buf, len);
  f.close();
  logLine("BLE GIF upload: " + nm + " (" + String(len / 1024) + " KB) " +
          (w == len ? "ok" : "SHORT WRITE"));
  return w == len;
}

/* ---- read-only views shared by HTTP + BLE (must run on core 1) ---- */

String ctlListGifsJson() {
  String out = "[";
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  bool first = true;
  while (f) {
    String nm = f.name();
    if (nm.endsWith(".gif")) {
      if (!first) out += ",";
      out += "\"" + nm + "\"";
      first = false;
    }
    f = root.openNextFile();
  }
  out += "]";
  return out;
}

String ctlStatusJson() {
  String tk = botToken;                        // token is masked for display
  if (tk.length() > 10) tk = tk.substring(0, 6) + "..." + tk.substring(tk.length() - 4);
  JsonDocument d;
  d["ssid"]     = cfgWifiSsid;
  d["chat"]     = allowedChatId;
  d["token"]    = tk;
  d["lat"]      = cfgLat;
  d["lon"]      = cfgLon;
  d["tz"]       = tzOffsetSec;
  d["bright"]   = userBrightness;
  d["interval"] = charEveryMs / 60000UL;
  d["mode"]     = (int)mode;
  d["heap"]     = ESP.getFreeHeap() / 1024;
  d["ip"]       = WiFi.localIP().toString();
  d["wifi"]     = (WiFi.status() == WL_CONNECTED)
                    ? "connected, IP " + WiFi.localIP().toString()
                    : String(portalActive ? "hotspot mode — enter your WiFi above"
                                          : "disconnected");
  d["mqttEn"]   = mqttEnable;
  d["mqttHost"] = mqttHost;
  d["mqttPort"] = mqttPort;
  d["mqttUser"] = mqttUser;
  String out;
  serializeJson(d, out);
  return out;
}

String ctlLogsText() {
  String out;
  if (logMutex) xSemaphoreTake(logMutex, portMAX_DELAY);
  for (int i = 0; i < LOG_LINES; i++) {
    String &l = logBuf[(logHead + i) % LOG_LINES];
    if (l.length()) out += l + "\n";
  }
  if (logMutex) xSemaphoreGive(logMutex);
  if (!out.length()) out = "(no logs yet)";
  return out;
}
