/* DigiFrame — special days (/events.json) + persisted config (/config.json) */
#pragma once

/**********************  6. EVENTS (special days)  ********************/
/* A special day: a date, a TYPE that drives the celebration theme
   ("custom" -> fireworks, "birthday" -> cake + confetti), and a message. */
struct SpecialDay { String date; String type; String message; };   // date = "MM-DD"
#define MAX_EVENTS 12
SpecialDay events[MAX_EVENTS];
int numEvents = 0;

void saveEvents() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < numEvents; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["d"] = events[i].date;
    o["t"] = events[i].type;
    o["m"] = events[i].message;
  }
  File f = LittleFS.open("/events.json", "w");
  serializeJson(doc, f);
  f.close();
}
void loadEvents() {
  numEvents = 0;
  // No default events — this is a general-purpose clock. Users add their own.
  if (!LittleFS.exists("/events.json")) return;
  File f = LittleFS.open("/events.json", "r");
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    for (JsonObject o : doc.as<JsonArray>()) {
      if (numEvents >= MAX_EVENTS) break;
      events[numEvents++] = {
        o["d"].as<String>(),
        o["t"].is<const char*>() ? o["t"].as<String>() : String("custom"),
        // "m" is the message; fall back to the legacy "n" key if present
        o["m"].is<const char*>() ? o["m"].as<String>()
          : (o["n"].is<const char*>() ? o["n"].as<String>() : String(""))
      };
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
// today's special day (type + message), or nullptr
SpecialDay *todaysEvent() {
  String t = todayMMDD();
  for (int i = 0; i < numEvents; i++)
    if (events[i].date == t) return &events[i];
  return nullptr;
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
      if (events[i].date == String(b)) { nameOut = events[i].message; return d; }
  }
  return -1;
}

/**********************  6a. CALENDAR EVENTS (from HA)  ****************/
struct CalendarEvent { String date; String time; String message; };
#define MAX_CALENDAR 10
CalendarEvent calEvents[MAX_CALENDAR];
int numCalEvents = 0;

void saveCalendar() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < numCalEvents; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["d"] = calEvents[i].date;
    if (calEvents[i].time.length() > 0) o["t"] = calEvents[i].time;
    o["m"] = calEvents[i].message;
  }
  File f = LittleFS.open("/calendar.json", "w");
  serializeJson(doc, f);
  f.close();
}

void loadCalendar() {
  numCalEvents = 0;
  if (!LittleFS.exists("/calendar.json")) return;
  File f = LittleFS.open("/calendar.json", "r");
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    for (JsonObject o : doc.as<JsonArray>()) {
      if (numCalEvents >= MAX_CALENDAR) break;
      calEvents[numCalEvents++] = {
        o["d"].as<String>(),
        o["t"].is<const char*>() ? o["t"].as<String>() : String(""),
        o["m"].as<String>()
      };
    }
  }
  f.close();
}

void setCalendarFromJson(const String &jsonStr) {
  JsonDocument doc;
  if (deserializeJson(doc, jsonStr) == DeserializationError::Ok) {
    numCalEvents = 0;
    for (JsonObject o : doc.as<JsonArray>()) {
      if (numCalEvents >= MAX_CALENDAR) break;
      calEvents[numCalEvents++] = { 
        o["d"].as<String>(), 
        o["t"].is<const char*>() ? o["t"].as<String>() : String(""), 
        o["m"].as<String>() 
      };
    }
    saveCalendar();
  }
}

// Gets the next calendar event starting from today
bool getNextCalendarEvent(String &nameOut) {
  if (numCalEvents == 0) return false;
  time_t now = time(nullptr);
  for (int d = 0; d < 30; d++) {  // Look up to 30 days ahead
    time_t t = now + (time_t)d * 86400;
    struct tm tmp;
    localtime_r(&t, &tmp);
    char b[12];
    snprintf(b, sizeof(b), "%04d-%02d-%02d", tmp.tm_year + 1900, tmp.tm_mon + 1, tmp.tm_mday);
    for (int i = 0; i < numCalEvents; i++) {
      if (calEvents[i].date == String(b)) {
        nameOut = calEvents[i].message;
        if (d == 0) nameOut += " (Today)";
        else if (d == 1) nameOut += " (Tmrw)";
        else { nameOut += " (+"; nameOut += d; nameOut += "d)"; }
        return true;
      }
    }
  }
  return false;
}

/**********************  6b. PERSISTED CONFIG  ************************/
void saveConfig() {
  JsonDocument d;
  d["charMin"] = charEveryMs / 60000UL;
  d["bright"]  = userBrightness;
  d["cord"]    = colorOrder;
  d["ns"]      = cfgNightStart;
  d["ne"]      = cfgNightEnd;
  d["nd"]      = cfgNightDays;
  d["no"]      = cfgNightOverride;
  d["ssid"]    = cfgWifiSsid;
  d["pass"]    = cfgWifiPass;
  d["lat"]     = cfgLat;
  d["lon"]     = cfgLon;
  d["tz"]      = tzOffsetSec;
  d["24h"]     = use24h;
  d["rot"]     = displayRotation;
  d["lang"]    = cfgLang;
  d["mqttEn"]   = mqttEnable;
  d["mqttHost"] = mqttHost;
  d["mqttPort"] = mqttPort;
  d["mqttUser"] = mqttUser;
  d["mqttPass"] = mqttPass;
  d["cH"] = theme.hourHex;
  d["cM"] = theme.minHex;
  d["cC"] = theme.colonHex;
  d["cS"] = theme.secHex;
  d["cD"] = theme.dateHex;
  d["cT"] = theme.tempHex;
  d["cCtm"] = theme.calTimeHex;
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
    if (d["cord"].is<int>())       colorOrder = constrain(d["cord"].as<int>(), 0, 5);
    // Backward compat: old config used a bool "swap" or "swc" for G↔B swap
    else if (d["swap"].is<bool>() && d["swap"].as<bool>()) colorOrder = 1;
    else if (d["swc"].is<bool>()  && d["swc"].as<bool>())  colorOrder = 1;
    if (d["ns"].is<int>())      cfgNightStart = d["ns"].as<int>();
    if (d["ne"].is<int>())      cfgNightEnd = d["ne"].as<int>();
    if (d["nd"].is<int>())      cfgNightDays = d["nd"].as<int>();
    if (d["no"].is<int>())      cfgNightOverride = d["no"].as<int>();
    if (d["ssid"].is<const char*>())    cfgWifiSsid   = d["ssid"].as<String>();
    if (d["pass"].is<const char*>())    cfgWifiPass   = d["pass"].as<String>();
    if (d["lat"].is<const char*>()) strlcpy(cfgLat, d["lat"], sizeof(cfgLat));
    if (d["lon"].is<const char*>()) strlcpy(cfgLon, d["lon"], sizeof(cfgLon));
    if (d["tz"].is<int>())               tzOffsetSec = d["tz"].as<int>();
    if (d["24h"].is<bool>())             use24h = d["24h"].as<bool>();
    if (d["rot"].is<int>())              displayRotation = d["rot"].as<int>();
    if (d["lang"].is<int>())             cfgLang = d["lang"].as<int>();
    if (d["mqttEn"].is<bool>())          mqttEnable = d["mqttEn"].as<bool>();
    if (d["mqttHost"].is<const char*>()) mqttHost   = d["mqttHost"].as<String>();
    if (d["mqttPort"].is<int>())         mqttPort   = d["mqttPort"].as<int>();
    if (d["mqttUser"].is<const char*>()) mqttUser   = d["mqttUser"].as<String>();
    if (d["mqttPass"].is<const char*>()) mqttPass   = d["mqttPass"].as<String>();
    
    if (d["cH"].is<const char*>()) theme.hourHex = d["cH"].as<String>();
    if (d["cM"].is<const char*>()) theme.minHex = d["cM"].as<String>();
    if (d["cC"].is<const char*>()) theme.colonHex = d["cC"].as<String>();
    if (d["cS"].is<const char*>()) theme.secHex = d["cS"].as<String>();
    if (d["cD"].is<const char*>()) theme.dateHex = d["cD"].as<String>();
    if (d["cT"].is<const char*>()) theme.tempHex = d["cT"].as<String>();
    if (d["cCtm"].is<const char*>()) theme.calTimeHex = d["cCtm"].as<String>();
    
    applyThemeColors();
  }
  f.close();
}
