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
</head><body><h1>&#9200; DigiFrame</h1>
<fieldset><legend>Send a message</legend>
<input id=m placeholder="Hello!" style="width:64%">
<button onclick="api('msg','t='+encodeURIComponent(m.value))">Send</button></fieldset>
<fieldset><legend>Brightness</legend>
<input type=range min=1 max=255 value=100 id=b style="width:100%"
 onchange="api('brightness','v='+b.value)"></fieldset>
<fieldset><legend>GIFs (c_* = character pack)</legend><ul id=l></ul>
<input type=file id=f accept=.gif><br><input id=n placeholder="name" style="width:100px">
<label><input type=checkbox id=p> character pack</label>
<button onclick=up()>Upload</button></fieldset>
<fieldset><legend>Random cameo every</legend>
<input id=iv type=number min=0 value=20 style="width:60px"> min (0 = off)
<button onclick="api('interval','m='+iv.value)">Set</button></fieldset>
<button onclick="api('celebrate')">&#127881; Celebration test</button>
<button onclick="api('stop')">&#9209; Back to clock</button>
<fieldset><legend>Special days</legend><ul id=ev></ul>
<input id=ed placeholder="MM-DD" style="width:70px">
<select id=et><option value=custom>custom</option><option value=birthday>birthday</option></select>
<input id=em placeholder="message" style="width:98%">
<button onclick=addEv()>Add / update</button>
<div class=st>type drives the visual: custom = fireworks, birthday = cake</div></fieldset>
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
<fieldset><legend>Time zone</legend>
<input id=tz type=number step=0.25 placeholder="UTC offset (hours)" style="width:55%">
<button onclick=saveTz()>Save</button>
<div class=st>e.g. 5.5 for IST, -8 for PST, 5.75 for Nepal &mdash; clock updates right away</div></fieldset>
<fieldset><legend>Home Assistant (MQTT)</legend>
<label><input type=checkbox id=mqe> enable</label><br>
<input id=mqh placeholder="broker host/IP" style="width:60%">
<input id=mqp type=number placeholder="1883" style="width:70px"><br>
<input id=mqu placeholder="username (optional)" style="width:45%">
<input id=mqw type=password placeholder="password" style="width:45%">
<button onclick=saveMqtt()>Save</button>
<div class=st>the clock announces itself to Home Assistant via MQTT discovery</div></fieldset>
<fieldset><legend>Firmware (OTA)</legend>
<input type=file id=fw accept=.bin><br>
<button onclick=ota()>&#9889; Update firmware</button>
<div class=st id=ost>upload DigiFrame.ino.bin (app image) &mdash; frame reboots when done</div></fieldset>
<fieldset><legend>Logs (live)</legend>
<pre id=log style="background:#0a0a12;padding:8px;border-radius:6px;max-height:220px;overflow:auto;font-size:11px;white-space:pre-wrap;margin:0"></pre>
<button onclick="loadLogs()">&#8635; Refresh</button></fieldset>
<script>
async function api(ep,body){await fetch('/api/'+ep,{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''});load();loadLogs();loadCfg();loadEv()}
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
 wst.textContent='WiFi: '+j.wifi;tst.textContent='';
 mqe.checked=!!j.mqttEn;mqh.placeholder=j.mqttHost||'broker host/IP';mqp.placeholder=j.mqttPort||1883;mqu.placeholder=j.mqttUser||'username (optional)';
 tz.placeholder='UTC'+(j.tz>=0?'+':'')+(j.tz/3600)+'h'}catch(e){}}
async function loadEv(){try{let r=await fetch('/api/events'),j=await r.json();
 ev.innerHTML=j.length?j.map(e=>`<li>${e.date} [${e.type}] ${e.message} <button onclick="delEv('${e.date}')">&#128465;</button></li>`).join(''):'<li class=st>none yet</li>'}catch(e){}}
async function addEv(){if(!ed.value)return;await fetch('/api/events',{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'d='+encodeURIComponent(ed.value)+'&t='+et.value+'&m='+encodeURIComponent(em.value)});em.value='';loadEv()}
async function delEv(d){await fetch('/api/eventdel',{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'d='+encodeURIComponent(d)});loadEv()}
async function saveMqtt(){await api('mqtt','en='+(mqe.checked?'1':'0')+'&h='+encodeURIComponent(mqh.value)+'&p='+(mqp.value||1883)+'&u='+encodeURIComponent(mqu.value)+'&w='+encodeURIComponent(mqw.value))}
async function saveTz(){let h=parseFloat(tz.value);if(isNaN(h))return;await api('tz','s='+Math.round(h*3600))}
load();loadLogs();loadCfg();loadEv();setInterval(loadLogs,2000)</script></body></html>)HTML";

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
  if (mqttTaskHandle)    vTaskResume(mqttTaskHandle);
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
    if (mqttTaskHandle)    vTaskSuspend(mqttTaskHandle);
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
    web.send(200, "text/plain", ctlLogsText());
  });
  web.on("/api/tgtest", HTTP_POST, []() {
    ctlTgTest();
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/list", HTTP_GET, []() {
    web.send(200, "application/json", ctlListGifsJson());
  });
  web.on("/api/msg", HTTP_POST, []() {
    ctlSendMsg(web.arg("t"), false);
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/brightness", HTTP_POST, []() {
    ctlSetBrightness(web.arg("v").toInt());
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/play", HTTP_POST, []() {
    ctlPlayGif(web.arg("g"));
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/del", HTTP_POST, []() {
    ctlDelGif(web.arg("g"));
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/interval", HTTP_POST, []() {
    ctlSetInterval(web.arg("m").toInt());
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/celebrate", HTTP_POST, []() {
    ctlCelebrate();
    web.send(200, "text/plain", "ok");
  });
  /* ---- special days: list / add-update / delete ---- */
  web.on("/api/events", HTTP_GET, []() {
    web.send(200, "application/json", ctlListEventsJson());
  });
  web.on("/api/events", HTTP_POST, []() {
    bool ok = ctlAddEvent(web.arg("d"), web.arg("t"), web.arg("m"));
    web.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "bad date or list full");
  });
  web.on("/api/eventdel", HTTP_POST, []() {
    ctlDelEvent(web.arg("d"));
    web.send(200, "text/plain", "ok");
  });
  /* ---- Home Assistant / MQTT config; mqttTask reconnects (core 0) ---- */
  web.on("/api/mqtt", HTTP_POST, []() {
    ctlSetMqtt(web.arg("en") == "1", web.arg("h"), web.arg("p").toInt(),
               web.arg("u"), web.arg("w"));
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/stop", HTTP_POST, []() {
    ctlStop();
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
    web.send(200, "application/json", ctlStatusJson());
  });
  /* ---- save WiFi creds; wifi_manager picks up wifiRetryNow in loop() ---- */
  web.on("/api/wifi", HTTP_POST, []() {
    if (!ctlSetWifi(web.arg("s"), web.arg("p"))) { web.send(400, "text/plain", "SSID required"); return; }
    web.send(200, "text/plain", "ok — connecting to " + cfgWifiSsid);
  });
  /* ---- save weather location; weatherTask refetches right away ---- */
  web.on("/api/loc", HTTP_POST, []() {
    if (!ctlSetLoc(web.arg("la"), web.arg("lo"))) { web.send(400, "text/plain", "bad lat/lon"); return; }
    web.send(200, "text/plain", "ok");
  });
  /* ---- save timezone (UTC offset in seconds); re-applies immediately ---- */
  web.on("/api/tz", HTTP_POST, []() {
    ctlSetTz(web.arg("s").toInt());
    web.send(200, "text/plain", "ok");
  });
  /* ---- save Telegram config; tgTask applies the token (core 0) ---- */
  web.on("/api/tgconfig", HTTP_POST, []() {
    ctlSetTg(web.arg("t"), web.arg("c"));
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
