/* DigiFrame — local web dashboard + captive portal pages */
#pragma once

/**********************  12b. LOCAL WEB DASHBOARD  ********************/
/* Served by the ESP32 itself: http://digiframe.local on your home WiFi,
 * or http://192.168.4.1 in setup-hotspot mode (captive portal).
 * GIF upload, brightness, messages, character pack, WiFi + Telegram
 * config, live logs. No cloud, no open ports. */
const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name=viewport content="width=device-width,initial-scale=1"><title>DigiFrame</title>
<style>body{font-family:system-ui;background:#141420;color:#eee;max-width:420px;margin:auto;padding:16px}
h1{color:#ffb3de;font-size:22px}fieldset{border:1px solid #333;border-radius:10px;margin:12px 0;padding:12px}
legend{color:#aab}button,input{border-radius:8px;border:1px solid #444;background:#222;color:#eee;padding:8px;margin:3px 2px}
button{cursor:pointer;background:#ff5078;border:0}li{margin:6px 0;list-style:none}ul{padding:0;margin:6px 0}
.st{font-size:12px;color:#aab;margin-top:4px}</style>
</head><body><h1>&#128151; DigiFrame</h1>
<fieldset><legend>Send a message</legend>
<input id=m placeholder="I love you!" style="width:64%">
<button onclick="api('msg','t='+encodeURIComponent(m.value))">Send</button></fieldset>
<fieldset><legend>Brightness</legend>
<input type=range min=5 max=255 value=100 id=b style="width:100%"
 onchange="api('brightness','v='+b.value)"></fieldset>
<fieldset><legend>GIFs (c_* = character pack)</legend><ul id=l></ul>
<input type=file id=f accept=.gif><br><input id=n placeholder="name" style="width:100px">
<label><input type=checkbox id=p> character pack</label>
<button onclick=up()>Upload</button></fieldset>
<fieldset><legend>Random cameo every</legend>
<input id=iv type=number min=0 value=20 style="width:60px"> min (0 = off)
<button onclick="api('interval','m='+iv.value)">Set</button></fieldset>
<button onclick="api('party')">&#127881; Party test</button>
<button onclick="api('stop')">&#9209; Back to clock</button>
<fieldset><legend>WiFi</legend>
<input id=ws placeholder="network name (SSID)" style="width:94%"><br>
<input id=wp type=password placeholder="password" style="width:60%">
<button onclick="api('wifi','s='+encodeURIComponent(ws.value)+'&p='+encodeURIComponent(wp.value))">Save &amp; connect</button>
<div class=st id=wst></div></fieldset>
<fieldset><legend>Telegram</legend>
<input id=tt placeholder="bot token (from @BotFather)" style="width:94%"><br>
<input id=tc placeholder="allowed chat id" style="width:60%">
<button onclick="api('tgconfig','t='+encodeURIComponent(tt.value)+'&c='+encodeURIComponent(tc.value))">Save</button>
<div class=st id=tst></div>
<button onclick="api('tgtest')">Test Telegram send</button></fieldset>
<fieldset><legend>Weather location</legend>
<input id=la placeholder="latitude" style="width:28%">
<input id=lo placeholder="longitude" style="width:28%">
<button onclick="api('loc','la='+encodeURIComponent(la.value)+'&lo='+encodeURIComponent(lo.value))">Save</button>
<div class=st>decimal degrees, e.g. 12.97 / 77.59 &mdash; weather refreshes right away</div></fieldset>
<fieldset><legend>Firmware (OTA)</legend>
<input type=file id=fw accept=.bin><br>
<button onclick=ota()>&#9889; Update firmware</button>
<div class=st id=ost>upload DigiFrame.ino.bin (app image) &mdash; frame reboots when done</div></fieldset>
<fieldset><legend>Logs (live)</legend>
<pre id=log style="background:#0a0a12;padding:8px;border-radius:6px;max-height:220px;overflow:auto;font-size:11px;white-space:pre-wrap;margin:0"></pre>
<button onclick="loadLogs()">&#8635; Refresh</button></fieldset>
<script>
async function api(ep,body){await fetch('/api/'+ep,{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''});load();loadLogs();loadCfg()}
async function load(){try{let r=await fetch('/api/list'),j=await r.json();
 l.innerHTML=j.map(g=>`<li>${g} <button onclick="api('play','g=${g}')">&#9654;</button>
 <button onclick="api('del','g=${g}')">&#128465;</button></li>`).join('')}catch(e){}}
async function up(){if(!f.files[0])return;let fd=new FormData();fd.append('file',f.files[0]);
 await fetch('/api/upload?name='+encodeURIComponent(n.value)+'&pack='+(p.checked?'1':'0'),
 {method:'POST',body:fd});load()}
async function loadLogs(){try{let r=await fetch('/api/logs');log.textContent=await r.text();
 log.scrollTop=log.scrollHeight}catch(e){}}
function ota(){if(!fw.files[0]){ost.textContent='pick a .bin first';return}
 if(!confirm('Flash '+fw.files[0].name+' and reboot?'))return;
 let x=new XMLHttpRequest();x.open('POST','/api/ota');
 x.upload.onprogress=e=>{if(e.lengthComputable)ost.textContent='uploading '+Math.round(100*e.loaded/e.total)+'% ...'};
 x.onload=()=>{ost.textContent=x.responseText};
 x.onerror=()=>{ost.textContent='upload failed (connection lost)'};
 let fd=new FormData();fd.append('file',fw.files[0]);x.send(fd);
 ost.textContent='uploading...'}
async function loadCfg(){try{let r=await fetch('/api/config'),j=await r.json();
 ws.placeholder=j.ssid?('SSID: '+j.ssid):'network name (SSID)';
 tc.placeholder=j.chat?('chat id: '+j.chat):'allowed chat id';
 tt.placeholder=j.token?('token: '+j.token):'bot token (from @BotFather)';
 la.placeholder='lat: '+j.lat;lo.placeholder='lon: '+j.lon;
 wst.textContent='WiFi: '+j.wifi;tst.textContent=''}catch(e){}}
load();loadLogs();loadCfg();setInterval(loadLogs,2000)</script></body></html>)HTML";

void handleUpload() {
  HTTPUpload &up = web.upload();
  if (up.status == UPLOAD_FILE_START) {
    String nm = web.arg("name");
    if (!nm.length()) nm = up.filename;
    nm.replace(" ", "_");
    if (!nm.endsWith(".gif")) nm += ".gif";
    if (web.arg("pack") == "1" && !nm.startsWith("c_")) nm = "c_" + nm;
    webUpload = LittleFS.open("/" + nm, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (webUpload) webUpload.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (webUpload) webUpload.close();
  }
}

/* ---- OTA firmware update (dashboard "Firmware" section) ----
 * Streams an uploaded app image (DigiFrame.ino.bin) into the spare OTA
 * slot (the fatflash scheme has app0/app1) via Update, then reboots.
 * The first chunk must carry the esp_app_desc_t magic at offset 0x20 —
 * that's what distinguishes an app image from the bootloader/merged
 * images, so uploading the wrong .bin can't soft-brick the frame.
 * The whole upload is parsed inside one web.handleClient() call, so the
 * render loop never interleaves with flash writes. */
bool     otaBegun     = false;
String   otaError     = "";
uint32_t otaLastShown = 0;

void otaScreen(const String &line) {
  for (int b = 0; b < 2; b++) {          // paint both DMA buffers
    dma->fillScreen(0);
    dma->setTextSize(1);
    dma->setTextColor(C_MSG);
    dma->setCursor(2, 20);
    dma->print("UPDATING");
    dma->setTextColor(C_TEMP);
    dma->setCursor(2, 34);
    dma->print(line);
    dma->flipDMABuffer();
  }
}

void otaFail(const String &why) {
  otaError = why;
  Update.abort();
  otaBegun = false;
  if (tgTaskHandle)      vTaskResume(tgTaskHandle);
  if (weatherTaskHandle) vTaskResume(weatherTaskHandle);
  mode = MODE_CLOCK;
  logLine("OTA FAILED: " + why);
}

void handleOtaUpload() {
  HTTPUpload &up = web.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaError     = "";
    otaLastShown = 0;
    logLine("OTA start: " + up.filename);
    if (tgTaskHandle)      vTaskSuspend(tgTaskHandle);      // nothing else may
    if (weatherTaskHandle) vTaskSuspend(weatherTaskHandle); // touch heap/flash now
    closeGif();
    otaScreen("0 KB");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { otaFail(Update.errorString()); return; }
    otaBegun = true;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!otaBegun) return;               // already failed — drain the rest silently
    if (up.totalSize == 0) {             // first chunk: verify app-image descriptor
      uint32_t magic = 0;
      if (up.currentSize >= 0x24) memcpy(&magic, up.buf + 0x20, 4);
      if (magic != 0xABCD5432UL) {
        otaFail("not an app image - upload DigiFrame.ino.bin");
        return;
      }
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      otaFail(Update.errorString());
      return;
    }
    uint32_t done = up.totalSize + up.currentSize;
    if (done - otaLastShown > 131072) {  // progress every ~128 KB
      otaLastShown = done;
      otaScreen(String(done / 1024) + " KB");
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!otaBegun) return;
    otaBegun = false;
    if (Update.end(true)) {              // validates image + sets boot partition
      otaScreen("done!");
      logLine("OTA OK (" + String(up.totalSize / 1024) + " KB) - rebooting");
    } else otaFail(Update.errorString());
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (otaBegun) otaFail("upload aborted");
  }
}

void setupWeb() {
  web.on("/", HTTP_GET, []() { web.send_P(200, "text/html", DASH_HTML); });
  web.on("/api/logs", HTTP_GET, []() {
    String out;
    for (int i = 0; i < LOG_LINES; i++) {
      String &l = logBuf[(logHead + i) % LOG_LINES];
      if (l.length()) out += l + "\n";
    }
    if (!out.length()) out = "(no logs yet)";
    web.send(200, "text/plain", out);
  });
  web.on("/api/tgtest", HTTP_POST, []() {
    if (WiFi.status() != WL_CONNECTED) { logLine("tgtest: no WiFi"); web.send(200, "text/plain", "ok"); return; }
    logLine("tgtest: sending to " + allowedChatId + " ...");
    bool ok = bot.sendMessage(allowedChatId, "DigiFrame test message", "");
    logLine("tgtest: sendMessage returned " + String(ok ? "true (check your phone)" : "FALSE — token/chat_id bad"));
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/list", HTTP_GET, []() {
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
    web.send(200, "application/json", out);
  });
  web.on("/api/msg", HTTP_POST, []() {
    scrollText = web.arg("t");
    scrollText.toUpperCase();
    scrollX = PANEL_W;
    closeGif();
    mode = MODE_MSG;
    msgEndsAt = millis() + MSG_MINUTES * 60000UL;
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/brightness", HTTP_POST, []() {
    userBrightness = constrain(web.arg("v").toInt(), 1, 255);
    dma->setBrightness8(userBrightness);
    saveConfig();
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/play", HTTP_POST, []() {
    if (openGif("/" + web.arg("g"), true)) mode = MODE_GIF;
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/del", HTTP_POST, []() {
    LittleFS.remove("/" + web.arg("g"));
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/interval", HTTP_POST, []() {
    int mn = web.arg("m").toInt();
    charEveryMs = (mn <= 0) ? 0 : (uint32_t)mn * 60000UL;
    saveConfig();
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/party", HTTP_POST, []() {
    startParty(HER_NAME);
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/stop", HTTP_POST, []() {
    closeGif();
    mode = MODE_CLOCK;
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/upload", HTTP_POST,
         []() { web.send(200, "text/plain", "ok"); }, handleUpload);

  /* ---- OTA: upload DigiFrame.ino.bin, flash, reboot ---- */
  web.on("/api/ota", HTTP_POST, []() {
    bool ok = (otaError.length() == 0) && Update.isFinished();
    if (!ok && otaError.length() == 0) otaError = "no file received";
    web.sendHeader("Connection", "close");
    web.send(ok ? 200 : 500, "text/plain",
             ok ? "OK - flashed, rebooting..." : ("FAILED: " + otaError));
    if (ok) { delay(500); ESP.restart(); }
  }, handleOtaUpload);

  /* ---- config: current values (token masked) to prefill the form ---- */
  web.on("/api/config", HTTP_GET, []() {
    String tk = botToken;
    if (tk.length() > 10) tk = tk.substring(0, 6) + "..." + tk.substring(tk.length() - 4);
    JsonDocument d;
    d["ssid"]  = cfgWifiSsid;
    d["chat"]  = allowedChatId;
    d["token"] = tk;
    d["lat"]   = cfgLat;
    d["lon"]   = cfgLon;
    d["wifi"]  = (WiFi.status() == WL_CONNECTED)
                   ? "connected, IP " + WiFi.localIP().toString()
                   : String(portalActive ? "hotspot mode — enter your WiFi above" : "disconnected");
    String out;
    serializeJson(d, out);
    web.send(200, "application/json", out);
  });
  /* ---- save WiFi creds; wifi_manager picks up wifiRetryNow in loop() ---- */
  web.on("/api/wifi", HTTP_POST, []() {
    String s = web.arg("s");
    s.trim();
    if (!s.length()) { web.send(400, "text/plain", "SSID required"); return; }
    cfgWifiSsid = s;
    cfgWifiPass = web.arg("p");
    saveConfig();
    wifiRetryNow = true;
    logLine("WiFi creds updated -> '" + cfgWifiSsid + "'");
    web.send(200, "text/plain", "ok — connecting to " + cfgWifiSsid);
  });
  /* ---- save weather location; weatherTask refetches right away ---- */
  web.on("/api/loc", HTTP_POST, []() {
    String la = web.arg("la"); la.trim();
    String lo = web.arg("lo"); lo.trim();
    float flat = la.toFloat(), flon = lo.toFloat();
    if (!la.length() || !lo.length() || flat < -90 || flat > 90 || flon < -180 || flon > 180) {
      web.send(400, "text/plain", "bad lat/lon"); return;
    }
    strlcpy(cfgLat, la.c_str(), sizeof(cfgLat));
    strlcpy(cfgLon, lo.c_str(), sizeof(cfgLon));
    saveConfig();
    weatherNow = true;
    logLine("location updated -> " + la + "," + lo);
    web.send(200, "text/plain", "ok");
  });
  /* ---- save Telegram config; tgTask applies the token (core 0) ---- */
  web.on("/api/tgconfig", HTTP_POST, []() {
    String tk = web.arg("t"); tk.trim();
    String ch = web.arg("c"); ch.trim();
    if (tk.length()) { botToken = tk; tgTokenDirty = true; }
    if (ch.length()) allowedChatId = ch;
    saveConfig();
    logLine("Telegram config updated (chat_id=" + allowedChatId + ")");
    web.send(200, "text/plain", "ok");
  });
  /* ---- captive portal: any unknown URL (incl. OS connectivity probes
     like /generate_204, /hotspot-detect.html) redirects to the page ---- */
  web.onNotFound([]() {
    if (portalActive) {
      web.sendHeader("Location", "http://192.168.4.1/", true);
      web.send(302, "text/plain", "");
    } else {
      web.send(404, "text/plain", "not found");
    }
  });
  web.begin();
}
