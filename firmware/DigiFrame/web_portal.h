/* DigiFrame — local web dashboard + captive portal pages */
#pragma once

/**********************  12b. LOCAL WEB DASHBOARD  ********************/
/* Served by the ESP32 itself: http://digiframe.local on your home WiFi,
 * or http://192.168.4.1 in setup-hotspot mode (captive portal).
 * GIF upload, brightness, messages, character pack, WiFi
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
</fieldset>
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
</fieldset>
<fieldset><legend>Weather location</legend>
<input id=la placeholder="latitude" style="width:28%">
<input id=lo placeholder="longitude" style="width:28%">
<div class=st>decimal degrees, e.g. 12.97 / 77.59 &mdash; weather refreshes right away</div></fieldset>
<fieldset><legend>Time zone</legend>
<input id=tz type=number step=0.25 placeholder="UTC offset (hours)" style="width:55%">
<div class=st>e.g. 5.5 for IST, -8 for PST, 5.75 for Nepal &mdash; clock updates right away</div></fieldset>
<fieldset><legend>Display &amp; Time Format</legend>
<select id=lang><option value=0>English</option><option value=1>Deutsch</option></select>
<select id=tfm><option value=0>12-hour</option><option value=1>24-hour</option></select>
<select id=rot><option value=0>0 deg</option><option value=90>90 deg</option><option value=180>180 deg</option><option value=270>270 deg</option></select>
<br><label style="margin-top:8px;display:block"><input type=checkbox id=swc> Swap Green/Blue colors (fixes pink sun, reboots frame)</label>
</fieldset>
<fieldset><legend>Night Mode (Dim &amp; Minimal)</legend>
<div style="margin-bottom:8px">
  <select id=nmo><option value=0>Auto (Schedule)</option><option value=1>Force ON</option><option value=2>Force OFF</option></select>
</div>
<div id=nmSched>
  Start: <input id=ns type=number min=0 max=23 placeholder="23" style="width:40px">h &nbsp; 
  End: <input id=ne type=number min=0 max=23 placeholder="7" style="width:40px">h<br>
  <div style="font-size:12px;margin-top:6px;display:flex;gap:6px">
    <label><input type=checkbox id=nd0>Su</label>
    <label><input type=checkbox id=nd1>Mo</label>
    <label><input type=checkbox id=nd2>Tu</label>
    <label><input type=checkbox id=nd3>We</label>
    <label><input type=checkbox id=nd4>Th</label>
    <label><input type=checkbox id=nd5>Fr</label>
    <label><input type=checkbox id=nd6>Sa</label>
  </div>
</div>
</fieldset>
<fieldset><legend>Home Assistant (MQTT)</legend>
<label><input type=checkbox id=mqe> enable</label><br>
<input id=mqh placeholder="broker host/IP" style="width:60%">
<input id=mqp type=number placeholder="1883" style="width:70px"><br>
<input id=mqu placeholder="username (optional)" style="width:45%">
<input id=mqw type=password placeholder="password" style="width:45%">
<div class=st>the clock announces itself to Home Assistant via MQTT discovery</div></fieldset>
<button onclick="saveAll()" style="width:100%;font-size:16px;padding:12px;margin-top:8px;">&#128190; Save All Settings</button>
<div id=savSt class=st style="text-align:center;font-size:14px;min-height:20px;"></div>
<fieldset><legend>Firmware (OTA)</legend>
<input type=file id=fw accept=.bin><br>
<button onclick=ota()>&#9889; Update firmware</button>
<div class=st id=ost>upload DigiFrame.ino.bin (app image) &mdash; frame reboots when done</div></fieldset>
<fieldset><legend>Logs (live)</legend>
<pre id=log style="background:#0a0a12;padding:8px;border-radius:6px;max-height:220px;overflow:auto;font-size:11px;white-space:pre-wrap;margin:0"></pre>
<button onclick="loadLogs()">&#8635; Refresh</button></fieldset>
<script>
async function api(ep,body){await fetch('/api/'+ep,{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''});await load();await loadLogs();await loadCfg();await loadEv()}
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
 if(j.lat) la.value=j.lat; if(j.lon) lo.value=j.lon;
 tfm.value=j['24h']?'1':'0'; rot.value=j.rot; lang.value=j.lang;
 mqe.checked=!!j.mqttEn; if(j.mqttHost) mqh.value=j.mqttHost; if(j.mqttPort) mqp.value=j.mqttPort; if(j.mqttUser) mqu.value=j.mqttUser;
 tz.value=j.tz/3600; swc.checked=!!j.swap;
 if(j.ns!==undefined)ns.value=j.ns; if(j.ne!==undefined)ne.value=j.ne;
 if(j.nd!==undefined)for(let i=0;i<7;i++)document.getElementById('nd'+i).checked=(j.nd&(1<<i));
 if(j.no!==undefined)nmo.value=j.no;
}catch(e){}}
async function loadEv(){try{let r=await fetch('/api/events'),j=await r.json();
 ev.innerHTML=j.length?j.map(e=>`<li>${e.date} [${e.type}] ${e.message} <button onclick="delEv('${e.date}')">&#128465;</button></li>`).join(''):'<li class=st>none yet</li>'}catch(e){}}
async function addEv(){if(!ed.value)return;await fetch('/api/events',{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'d='+encodeURIComponent(ed.value)+'&t='+et.value+'&m='+encodeURIComponent(em.value)});em.value='';loadEv()}
async function delEv(d){await fetch('/api/eventdel',{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'d='+encodeURIComponent(d)});loadEv()}
async function saveAll() {
  let savSt = document.getElementById('savSt');
  savSt.textContent = "Saving...";
  savSt.style.color = "yellow";
  try {
    let p = new URLSearchParams();
    p.append("lang", document.getElementById('lang').value);
    p.append("rot", document.getElementById('rot').value);
    p.append("tfm", document.getElementById('tfm').value);
    p.append("iv", document.getElementById('iv').value);
    let ws = document.getElementById('ws').value;
    if(ws) { p.append("ws", ws); p.append("wp", document.getElementById('wp').value); }
    let la = document.getElementById('la').value, lo = document.getElementById('lo').value;
    if(la && lo) { p.append("la", la); p.append("lo", lo); }
    let tz = document.getElementById('tz').value;
    if(tz !== "") p.append("tz", Math.round(parseFloat(tz)*3600));
    p.append("mqe", document.getElementById('mqe').checked ? '1' : '0'); 
    let mqh = document.getElementById('mqh').value;
    p.append("mqh", mqh); // ALWAYS send mqh so backend triggers ctlSetMqtt
    p.append("mqp", document.getElementById('mqp').value || 1883);
    p.append("mqu", document.getElementById('mqu').value);
    p.append("mqw", document.getElementById('mqw').value);
    p.append("swc", document.getElementById('swc').checked ? '1' : '0');
    p.append("ns", document.getElementById('ns').value || 0);
    p.append("ne", document.getElementById('ne').value || 7);
    p.append("no", document.getElementById('nmo').value || 0);
    let mask = 0; for(let i=0;i<7;i++)if(document.getElementById('nd'+i).checked) mask|=(1<<i);
    p.append("nd", mask);
    await fetch('/api/saveall', { method:'POST', body: p });
    savSt.textContent = "Saved successfully!"; savSt.style.color = "#0f0";
    setTimeout(()=>savSt.textContent="", 3000);
    loadCfg();
  } catch(e) { savSt.textContent = "Error saving!"; savSt.style.color = "red"; }
}
async function pollLogs(){await loadLogs();setTimeout(pollLogs,2000)}
async function init(){await load();await loadCfg();await loadEv();pollLogs()}init();</script></body></html>)HTML";

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
  web.on("/", HTTP_GET, []() {
    web.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    web.sendHeader("Pragma", "no-cache");
    web.sendHeader("Expires", "0");
    web.send_P(200, "text/html", DASH_HTML);
  });
  web.on("/api/logs", HTTP_GET, []() {
    web.send(200, "text/plain", ctlLogsText());
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
  /* ---- Home Assistant Calendar API ---- */
  web.on("/api/calendar", HTTP_POST, []() {
    String payload = web.arg("plain");
    if (!payload.length()) payload = web.arg("json"); // fallback
    if (payload.length()) {
       setCalendarFromJson(payload);
       web.send(200, "text/plain", "ok");
    } else {
       web.send(400, "text/plain", "no payload");
    }
  });
  /* ---- Home Assistant / MQTT config; mqttTask reconnects (core 0) ---- */
  web.on("/api/mqtt", HTTP_POST, []() {
    ctlSetMqtt(web.arg("en") == "1", web.arg("h"), web.arg("p").toInt(),
               web.arg("u"), web.arg("w"));
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/timefmt", HTTP_POST, []() {
    ctlSetTimeFmt(web.arg("v") == "1");
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/rotation", HTTP_POST, []() {
    ctlSetRotation(web.arg("v").toInt());
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/lang", HTTP_POST, []() {
    ctlSetLang(web.arg("v").toInt());
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
    String cfg = ctlStatusJson();
    // Inject extra config into the JSON
    cfg.replace("}", ",\"swap\":" + String(swapColors ? "true" : "false") + 
                     ",\"ns\":" + String(cfgNightStart) + 
                     ",\"ne\":" + String(cfgNightEnd) + 
                     ",\"nd\":" + String(cfgNightDays) + 
                     ",\"no\":" + String(cfgNightOverride) + "}");
    web.send(200, "application/json", cfg);
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
  /* ---- save all config (global save button) ---- */
  web.on("/api/saveall", HTTP_POST, []() {
    if (web.hasArg("lang")) ctlSetLang(web.arg("lang").toInt());
    if (web.hasArg("rot")) ctlSetRotation(web.arg("rot").toInt());
    if (web.hasArg("tfm")) ctlSetTimeFmt(web.arg("tfm").toInt());
    if (web.hasArg("iv")) ctlSetInterval(web.arg("iv").toInt());
    if (web.hasArg("ws")) ctlSetWifi(web.arg("ws"), web.arg("wp"));
    if (web.hasArg("la")) ctlSetLoc(web.arg("la"), web.arg("lo"));
    if (web.hasArg("tz")) ctlSetTz(web.arg("tz").toInt());
    if (web.hasArg("mqh")) ctlSetMqtt(web.arg("mqe")=="1", web.arg("mqh"), web.arg("mqp").toInt(), web.arg("mqu"), web.arg("mqw"));
    bool doReboot = false;
    if (web.hasArg("swc")) {
       bool newSwc = web.arg("swc") == "1";
       if (swapColors != newSwc) {
           swapColors = newSwc;
           doReboot = true;
       }
    }
    if (web.hasArg("ns")) cfgNightStart = web.arg("ns").toInt();
    if (web.hasArg("ne")) cfgNightEnd = web.arg("ne").toInt();
    if (web.hasArg("nd")) cfgNightDays = web.arg("nd").toInt();
    if (web.hasArg("no")) cfgNightOverride = web.arg("no").toInt();
    saveConfig();
    
    web.send(200, "text/plain", "ok");
    if (doReboot) { delay(500); ESP.restart(); }
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
