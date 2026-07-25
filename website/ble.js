/* DigiFrame — Web Bluetooth transport.
 * UUIDs and payloads mirror ble_config.h / BLE_PROTOCOL.md. */
(function (global) {
  "use strict";

  const SVC    = "6b1d0001-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const STATUS = "6b1d0002-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const WIFI   = "6b1d0003-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const CTRL   = "6b1d0004-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const GIFS   = "6b1d0005-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const UPLOAD = "6b1d0006-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const LOG    = "6b1d0007-2f4a-4d9b-9c3e-1a2b3c4d5e6f";
  const EVENTS = "6b1d0008-2f4a-4d9b-9c3e-1a2b3c4d5e6f";

  const CHUNK = 180;                       // GIF upload DATA payload bytes
  const enc = new TextEncoder();
  const dec = new TextDecoder();

  class DigiFrameBLE {
    constructor() {
      this.device = null;
      this.chars = {};
      this.on = { status: null, gifs: null, log: null, events: null, disconnect: null };
    }

    get connected() {
      return !!(this.device && this.device.gatt && this.device.gatt.connected);
    }

    static get supported() {
      return typeof navigator !== "undefined" && !!navigator.bluetooth;
    }

    async connect() {
      this.device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [SVC] }, { namePrefix: "DigiFrame" }],
        optionalServices: [SVC],
      });
      this.device.addEventListener("gattserverdisconnected", () => {
        this.chars = {};
        if (this.on.disconnect) this.on.disconnect();
      });

      const server = await this.device.gatt.connect();
      const svc = await server.getPrimaryService(SVC);
      const uuids = { STATUS, WIFI, CTRL, GIFS, UPLOAD, LOG, EVENTS };
      for (const [k, u] of Object.entries(uuids)) {
        this.chars[k] = await svc.getCharacteristic(u);
      }

      await this._subscribe("STATUS", (v) => this.on.status && this.on.status(this._json(v)));
      await this._subscribe("GIFS",   (v) => this.on.gifs   && this.on.gifs(this._json(v) || []));
      await this._subscribe("LOG",    (v) => this.on.log    && this.on.log(dec.decode(v)));
      await this._subscribe("EVENTS", (v) => this.on.events && this.on.events(this._json(v) || []));

      // prime with current values
      await this._readInto("STATUS", (v) => this.on.status && this.on.status(this._json(v)));
      await this._readInto("GIFS",   (v) => this.on.gifs   && this.on.gifs(this._json(v) || []));
      await this._readInto("LOG",    (v) => this.on.log    && this.on.log(dec.decode(v)));
      await this._readInto("EVENTS", (v) => this.on.events && this.on.events(this._json(v) || []));
      return this.device.name || "DigiFrame";
    }

    disconnect() {
      if (this.device && this.device.gatt.connected) this.device.gatt.disconnect();
    }

    // ---- commands ----
    control(obj) { return this._writeJson("CTRL", obj); }
    setWifi(ssid, pass) { return this._writeJson("WIFI", { ssid, pass }); }

    msg(text)          { return this.control({ op: "msg", text }); }
    pin(text)          { return this.control({ op: "pin", text }); }
    brightness(v)      { return this.control({ op: "brightness", v }); }
    play(name)         { return this.control({ op: "play", name }); }
    del(name)          { return this.control({ op: "del", name }); }
    interval(min)      { return this.control({ op: "interval", min }); }
    celebrate(type, message) { return this.control({ op: "celebrate", type: type || "", message: message || "" }); }
    stop()             { return this.control({ op: "stop" }); }
    loc(lat, lon)      { return this.control({ op: "loc", lat, lon }); }
    tgconfig(token, chat) { return this.control({ op: "tgconfig", token, chat }); }
    tgtest()           { return this.control({ op: "tgtest" }); }
    addEvent(date, type, message) { return this.control({ op: "event_add", date, type, message }); }
    delEvent(date)     { return this.control({ op: "event_del", date }); }
    setMqtt(cfg)       { return this.control(Object.assign({ op: "mqtt" }, cfg)); }

    // ---- GIF upload (framed: START / DATA* / END) ----
    async uploadGif(name, pack, arrayBuffer, onProgress) {
      const c = this.chars.UPLOAD;
      const bytes = new Uint8Array(arrayBuffer);
      const start = new Uint8Array([0x01, ...enc.encode(JSON.stringify({ name, pack: pack ? 1 : 0 }))]);
      await c.writeValueWithResponse(start);
      for (let i = 0; i < bytes.length; i += CHUNK) {
        const slice = bytes.subarray(i, Math.min(i + CHUNK, bytes.length));
        const frame = new Uint8Array(slice.length + 1);
        frame[0] = 0x02;
        frame.set(slice, 1);
        await c.writeValueWithResponse(frame);
        if (onProgress) onProgress(Math.min(i + CHUNK, bytes.length), bytes.length);
      }
      await c.writeValueWithResponse(new Uint8Array([0x03]));
    }

    // ---- internals ----
    async _subscribe(key, cb) {
      const c = this.chars[key];
      c.addEventListener("characteristicvaluechanged", (e) => cb(e.target.value));
      await c.startNotifications();
    }
    async _readInto(key, cb) {
      try { cb(await this.chars[key].readValue()); } catch (e) { /* value not ready */ }
    }
    _writeJson(key, obj) {
      return this.chars[key].writeValueWithResponse(enc.encode(JSON.stringify(obj)));
    }
    _json(dataview) {
      try { return JSON.parse(dec.decode(dataview)); } catch (e) { return null; }
    }
  }

  global.DigiFrameBLE = DigiFrameBLE;
})(window);
