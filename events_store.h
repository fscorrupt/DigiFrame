/* DigiFrame — special days (/events.json) + persisted config (/config.json) */
#pragma once

/**********************  6. EVENTS (special days)  ********************/
struct SpecialDay { String date; String name; };   // date = "MM-DD"
#define MAX_EVENTS 12
SpecialDay events[MAX_EVENTS];
int numEvents = 0;

void saveEvents() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < numEvents; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["d"] = events[i].date;
    o["n"] = events[i].name;
  }
  File f = LittleFS.open("/events.json", "w");
  serializeJson(doc, f);
  f.close();
}
void loadEvents() {
  numEvents = 0;
  if (!LittleFS.exists("/events.json")) {
    // Seed the one that matters ;)
    events[0] = { "07-28", String(HER_NAME) + "'s Birthday" };
    numEvents = 1;
    saveEvents();
    return;
  }
  File f = LittleFS.open("/events.json", "r");
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    for (JsonObject o : doc.as<JsonArray>()) {
      if (numEvents >= MAX_EVENTS) break;
      events[numEvents++] = { o["d"].as<String>(), o["n"].as<String>() };
    }
  }
  f.close();
}
String todayMMDD() {
  char b[6];
  snprintf(b, sizeof(b), "%02d-%02d", tmNow.tm_mon + 1, tmNow.tm_mday);
  return String(b);
}
// true if today matches a stored special day
bool isSpecialToday() {
  String t = todayMMDD();
  for (int i = 0; i < numEvents; i++)
    if (events[i].date == t) return true;
  return false;
}
// days until next event (searches up to 366 days ahead); -1 if none
int daysToNextEvent(String &nameOut) {
  if (numEvents == 0) return -1;
  time_t now = time(nullptr);
  for (int d = 0; d < 366; d++) {
    time_t t = now + (time_t)d * 86400;
    struct tm tmp;
    localtime_r(&t, &tmp);
    char b[6];
    snprintf(b, sizeof(b), "%02d-%02d", tmp.tm_mon + 1, tmp.tm_mday);
    for (int i = 0; i < numEvents; i++)
      if (events[i].date == String(b)) { nameOut = events[i].name; return d; }
  }
  return -1;
}

/**********************  6b. PERSISTED CONFIG  ************************/
void saveConfig() {
  JsonDocument d;
  d["charMin"] = charEveryMs / 60000UL;
  d["bright"]  = userBrightness;
  d["ssid"]    = cfgWifiSsid;
  d["pass"]    = cfgWifiPass;
  d["tgToken"] = botToken;
  d["tgChat"]  = allowedChatId;
  d["lat"]     = cfgLat;
  d["lon"]     = cfgLon;
  File f = LittleFS.open("/config.json", "w");
  serializeJson(d, f);
  f.close();
}
void loadConfig() {
  if (!LittleFS.exists("/config.json")) return;
  File f = LittleFS.open("/config.json", "r");
  JsonDocument d;
  if (deserializeJson(d, f) == DeserializationError::Ok) {
    if (d["charMin"].is<int>()) charEveryMs = (uint32_t)d["charMin"].as<int>() * 60000UL;
    if (d["bright"].is<int>())  userBrightness = constrain(d["bright"].as<int>(), 1, 255);
    if (d["ssid"].is<const char*>())    cfgWifiSsid   = d["ssid"].as<String>();
    if (d["pass"].is<const char*>())    cfgWifiPass   = d["pass"].as<String>();
    if (d["tgToken"].is<const char*>()) botToken      = d["tgToken"].as<String>();
    if (d["tgChat"].is<const char*>())  allowedChatId = d["tgChat"].as<String>();
    if (d["lat"].is<const char*>()) strlcpy(cfgLat, d["lat"], sizeof(cfgLat));
    if (d["lon"].is<const char*>()) strlcpy(cfgLon, d["lon"], sizeof(cfgLon));
  }
  f.close();
}
