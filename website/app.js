/* DigiFrame — cloud dashboard UI, wired to the frame over Web Bluetooth. */
(function () {
  "use strict";

  const $ = (id) => document.getElementById(id);
  const ble = new DigiFrameBLE();
  let brightTimer = null;   // debounce the slider
  let mqttInit = false;     // prefill the MQTT enable toggle only once

  // ---------- URL hint (#d=DigiFrame-XXXX from the panel QR) ----------
  const hash = new URLSearchParams(location.hash.slice(1));
  const deviceHint = hash.get("d");
  const apName = hash.get("ap") || "DigiFrame";
  const apPass = hash.get("app") || "digiframe123";

  // ---------- capability gate ----------
  if (!DigiFrameBLE.supported) {
    $("gate").classList.add("hidden");
    $("fallback").classList.remove("hidden");
    if (deviceHint) $("fbName").textContent = deviceHint;
    $("fbAp").textContent = apName;
    $("fbApPass").textContent = apPass;
  }

  // ---------- connection ----------
  function setConn(state, text) {
    const el = $("conn");
    el.textContent = text;
    el.classList.toggle("on", state === "on");
  }

  ble.on.disconnect = () => {
    setConn("off", "offline");
    $("dash").classList.add("hidden");
    $("gate").classList.remove("hidden");
    $("gateStatus").textContent = "Disconnected.";
  };
  ble.on.status = renderStatus;
  ble.on.gifs = renderGifs;
  ble.on.events = renderEvents;
  ble.on.log = (text) => { const l = $("log"); l.textContent = text; l.scrollTop = l.scrollHeight; };

  $("connectBtn").addEventListener("click", async () => {
    $("gateStatus").textContent = "Requesting device…";
    try {
      const name = await ble.connect();
      setConn("on", name);
      $("gate").classList.add("hidden");
      $("dash").classList.remove("hidden");
      $("gateStatus").textContent = "";
    } catch (e) {
      $("gateStatus").textContent = e && e.name === "NotFoundError"
        ? "No frame selected."
        : "Connection failed: " + (e && e.message ? e.message : e);
    }
  });
  $("disconnectBtn").addEventListener("click", () => ble.disconnect());

  // ---------- status + gif rendering ----------
  let editing = {};   // don't clobber a field the user is typing in
  ["ssid", "pass", "tgToken", "tgChat", "lat", "lon", "interval"].forEach((id) => {
    const el = $(id);
    el.addEventListener("focus", () => { editing[id] = true; });
    el.addEventListener("blur", () => { editing[id] = false; });
  });

  function ph(id, val) { if (val !== undefined && val !== null && val !== "") $(id).placeholder = String(val); }

  function renderStatus(s) {
    if (!s) return;
    ph("ssid", s.ssid ? "SSID: " + s.ssid : "network name (SSID)");
    ph("tgChat", s.chat ? "chat id: " + s.chat : "allowed chat id");
    ph("tgToken", s.token ? "token: " + s.token : "bot token (from @BotFather)");
    ph("lat", "lat: " + s.lat);
    ph("lon", "lon: " + s.lon);
    if (!editing.interval && document.activeElement !== $("interval")) $("interval").value = s.interval;
    if (document.activeElement !== $("bright")) {
      $("bright").value = s.bright;
      $("brightVal").textContent = "level " + s.bright;
    }
    $("wifiStatus").textContent = "WiFi: " + (s.wifi || "—");
    $("wifiStatus").className = "st " + (String(s.wifi || "").startsWith("connected") ? "ok" : "");
    if (!mqttInit) { $("mqEn").checked = !!s.mqttEn; mqttInit = true; }
    ph("mqHost", s.mqttHost || "broker host/IP");
    ph("mqPort", s.mqttPort || 1883);
    ph("mqUser", s.mqttUser || "username (optional)");
    $("statusLine").textContent =
      `mode ${s.mode} · ${s.heap} KB free · ${s.ip || "no ip"} · cameo ${s.interval}m`;
  }

  function renderEvents(list) {
    const ul = $("evList");
    ul.innerHTML = "";
    if (!list.length) { ul.innerHTML = '<li class="st">no special days yet</li>'; return; }
    list.forEach((e) => {
      const li = document.createElement("li");
      const span = document.createElement("span");
      span.className = "name";
      span.textContent = `${e.date} [${e.type}] ${e.message}`;
      const del = document.createElement("button");
      del.className = "ghost";
      del.textContent = "🗑";
      del.onclick = () => ble.delEvent(e.date);
      li.append(span, del);
      ul.appendChild(li);
    });
  }

  function renderGifs(list) {
    const ul = $("gifList");
    ul.innerHTML = "";
    if (!list.length) { ul.innerHTML = '<li class="st">no GIFs yet — upload one below</li>'; return; }
    list.forEach((full) => {
      const name = full.replace(/^\//, "");
      const li = document.createElement("li");
      const span = document.createElement("span");
      span.className = "name";
      span.textContent = name;
      const play = document.createElement("button");
      play.textContent = "▶";
      play.onclick = () => ble.play(name);
      const del = document.createElement("button");
      del.className = "ghost";
      del.textContent = "🗑";
      del.onclick = () => ble.del(name);
      li.append(span, play, del);
      ul.appendChild(li);
    });
  }

  // ---------- actions ----------
  const guard = (fn) => async (...a) => {
    if (!ble.connected) return;
    try { await fn(...a); } catch (e) { console.warn(e); }
  };

  $("msgSend").onclick = guard(() => ble.msg($("msg").value));
  $("msgPin").onclick  = guard(() => ble.pin($("msg").value));
  $("btnCelebrate").onclick = guard(() => ble.celebrate());
  $("btnStop").onclick  = guard(() => ble.stop());
  $("intervalSet").onclick = guard(() => ble.interval(parseInt($("interval").value, 10) || 0));
  $("locSave").onclick  = guard(() => ble.loc($("lat").value.trim(), $("lon").value.trim()));
  $("wifiSave").onclick = guard(() => {
    ble.setWifi($("ssid").value.trim(), $("pass").value);
    $("wifiStatus").textContent = "WiFi: connecting…";
  });
  $("tgSave").onclick = guard(() => {
    ble.tgconfig($("tgToken").value.trim(), $("tgChat").value.trim());
    $("tgStatus").textContent = "Saved.";
  });
  $("tgTest").onclick = guard(() => { ble.tgtest(); $("tgStatus").textContent = "Test sent — check the logs."; });

  $("evAdd").onclick = guard(() => {
    const d = $("evDate").value.trim();
    if (!d) return;
    ble.addEvent(d, $("evType").value, $("evMsg").value.trim());
    $("evMsg").value = "";
  });
  $("mqSave").onclick = guard(() => ble.setMqtt({
    enable: $("mqEn").checked,
    host: $("mqHost").value.trim(),
    port: parseInt($("mqPort").value, 10) || 1883,
    user: $("mqUser").value.trim(),
    pass: $("mqPass").value,
  }));

  $("bright").addEventListener("input", () => {
    const v = parseInt($("bright").value, 10);
    $("brightVal").textContent = "level " + v;
    clearTimeout(brightTimer);
    brightTimer = setTimeout(() => guard(() => ble.brightness(v))(), 120);
  });

  // ---------- GIF upload ----------
  $("gifUpload").onclick = guard(async () => {
    const f = $("gifFile").files[0];
    if (!f) { $("uploadStatus").textContent = "pick a .gif first"; return; }
    const name = ($("gifName").value.trim() || f.name).replace(/\.gif$/i, "");
    const pack = $("gifPack").checked;
    const buf = await f.arrayBuffer();
    $("uploadStatus").className = "st";
    await ble.uploadGif(name, pack, buf, (sent, total) => {
      $("uploadStatus").textContent = `uploading ${Math.round((100 * sent) / total)}% (${Math.round(sent / 1024)} KB)`;
    });
    $("uploadStatus").textContent = "uploaded ✓";
    $("uploadStatus").classList.add("ok");
    $("gifFile").value = "";
    $("gifName").value = "";
  });

  // ---------- PWA service worker ----------
  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("sw.js").catch(() => {});
  }
})();
