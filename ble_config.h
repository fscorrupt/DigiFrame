/* DigiFrame — BLE config service (NimBLE GATT) for the cloud dashboard */
#pragma once
#include <NimBLEDevice.h>

/**********************  12c. BLE CONFIG SERVICE  *********************/
/* Exposes the same actions as the local HTTP dashboard over Bluetooth so
 * the cloud site (Web Bluetooth) can pair with the frame and configure it —
 * including WiFi provisioning before the frame is on any network.
 *
 * THREADING: NimBLE callbacks run on the BLE host task (core 0). They must
 * NEVER touch LittleFS / the DMA panel / openGif directly — they only parse
 * input and postTgCmd() it, exactly like tgTask. loop() (core 1) drains the
 * queue and calls the ctl* functions. The read/notify characteristics are
 * the reverse: their values are refreshed from bleTick() (core 1), so the
 * BLE stack only ever serves already-prepared bytes.
 *
 * UUIDs are mirrored in BLE_PROTOCOL.md and the website's ble.js. */
#define BLE_SVC_UUID    "6b1d0001-2f4a-4d9b-9c3e-1a2b3c4d5e6f"
#define BLE_STATUS_UUID "6b1d0002-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // read/notify JSON
#define BLE_WIFI_UUID   "6b1d0003-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // write {ssid,pass}
#define BLE_CTRL_UUID   "6b1d0004-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // write {op,...}
#define BLE_GIFS_UUID   "6b1d0005-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // read/notify JSON array
#define BLE_UPLOAD_UUID "6b1d0006-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // write framed chunks
#define BLE_LOG_UUID    "6b1d0007-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // read/notify log tail
#define BLE_EVENTS_UUID "6b1d0008-2f4a-4d9b-9c3e-1a2b3c4d5e6f"  // read/notify special-days JSON

/* GATT characteristic values are capped at 512 bytes by the ATT spec, so
 * every read/notify payload below must stay under that. */
#define BLE_VAL_MAX     512
#define BLE_UP_MAX      (512 * 1024)   // largest GIF accepted over BLE

NimBLEServer         *bleServer     = nullptr;
NimBLECharacteristic *bleStatusChar = nullptr;
NimBLECharacteristic *bleGifsChar   = nullptr;
NimBLECharacteristic *bleLogChar    = nullptr;
NimBLECharacteristic *bleEventsChar = nullptr;
volatile bool         bleConnected  = false;

/* GIF upload staging buffer — owned by the BLE task until END hands it to
 * core 1 (which frees it after writing to LittleFS). */
static uint8_t *bleUpBuf  = nullptr;
static size_t   bleUpCap  = 0;
static size_t   bleUpLen  = 0;
static String   bleUpName = "";
static bool     bleUpPack = false;

static void bleUploadReset() {
  if (bleUpBuf) free(bleUpBuf);
  bleUpBuf = nullptr;
  bleUpCap = bleUpLen = 0;
  bleUpName = "";
  bleUpPack = false;
}

String bleDeviceName() {                       // stable per-board name for the QR/site
  static String name;
  if (name.length()) return name;
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char b[24];
  snprintf(b, sizeof(b), "DigiFrame-%02X%02X", mac[4], mac[5]);
  name = String(b);
  return name;
}

/* ---- server (connection) callbacks ---- */
class DfServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &) override {
    bleConnected = true;
    logLine("BLE client connected");
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int reason) override {
    bleConnected = false;
    bleUploadReset();
    logLine("BLE client disconnected (" + String(reason) + ")");
    NimBLEDevice::startAdvertising();          // ready for the next phone
  }
};

/* ---- WiFi provisioning: {"ssid":"..","pass":".."} ---- */
class DfWifiCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    JsonDocument d;
    if (deserializeJson(d, c->getValue().c_str())) return;
    postTgCmd(TGC_SET_WIFI, d["ssid"] | "", 0, d["pass"] | "");
  }
};

/* ---- commands: {"op":"...", ...} (mirrors the /api/* endpoints) ---- */
class DfCtrlCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    JsonDocument d;
    if (deserializeJson(d, c->getValue().c_str())) return;
    String op = d["op"] | "";
    if      (op == "msg")        postTgCmd(TGC_MSG, d["text"] | "");
    else if (op == "pin")        postTgCmd(TGC_PIN, d["text"] | "");
    else if (op == "brightness") postTgCmd(TGC_BRIGHTNESS, "", (uint8_t)constrain((int)(d["v"] | 0), 1, 255));
    else if (op == "play")       postTgCmd(TGC_PLAY_GIF, d["name"] | "");
    else if (op == "del")        postTgCmd(TGC_DEL_GIF, d["name"] | "");
    else if (op == "interval")   postTgCmd(TGC_INTERVAL, String((int)(d["min"] | 0)));
    else if (op == "celebrate")  postTgCmd(TGC_CELEBRATE, d["type"] | "", 0, d["message"] | "");
    else if (op == "stop")       postTgCmd(TGC_STOP);
    else if (op == "loc")        postTgCmd(TGC_SET_LOC, d["lat"] | "", 0, d["lon"] | "");
    else if (op == "tgconfig")   postTgCmd(TGC_SET_TG, d["token"] | "", 0, d["chat"] | "");
    else if (op == "tgtest")     postTgCmd(TGC_TGTEST);
    // typed special days + Home Assistant config: hand the whole JSON to core 1
    else if (op == "event_add")  postTgCmd(TGC_EVENT_ADD, c->getValue().c_str());
    else if (op == "event_del")  postTgCmd(TGC_EVENT_DEL, d["date"] | "");
    else if (op == "mqtt")       postTgCmd(TGC_SET_MQTT, c->getValue().c_str());
  }
};

/* ---- chunked GIF upload: 1-byte opcode + payload ----
 *   0x01 START  payload = JSON {"name":"x","pack":0}
 *   0x02 DATA   payload = raw GIF bytes (use write-with-response for flow control)
 *   0x03 END    payload = none  -> commit to LittleFS on core 1 */
class DfUploadCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() < 1) return;
    const uint8_t *p = v.data();
    uint8_t  op = p[0];
    size_t   n  = v.length() - 1;              // payload length after opcode

    if (op == 0x01) {                          // START
      bleUploadReset();
      JsonDocument d;
      if (n && deserializeJson(d, (const char *)(p + 1), n) == DeserializationError::Ok) {
        bleUpName = String((const char *)(d["name"] | "gif"));
        bleUpPack = ((int)(d["pack"] | 0)) != 0;
      } else { bleUpName = "gif"; bleUpPack = false; }
      bleUpBuf = (uint8_t *)ps_malloc(BLE_UP_MAX);
      bleUpCap = bleUpBuf ? BLE_UP_MAX : 0;
      bleUpLen = 0;
      logLine("BLE upload start: " + bleUpName + (bleUpBuf ? "" : " (NO PSRAM BUFFER)"));
    } else if (op == 0x02) {                   // DATA
      if (bleUpBuf && bleUpLen + n <= bleUpCap) {
        memcpy(bleUpBuf + bleUpLen, p + 1, n);
        bleUpLen += n;
      }
    } else if (op == 0x03) {                   // END -> hand buffer to core 1
      if (bleUpBuf && bleUpLen) {
        postTgCmd(TGC_GIF_COMMIT, bleUpName, bleUpPack ? 1 : 0, "", bleUpBuf, bleUpLen);
        bleUpBuf = nullptr;                     // ownership transferred; core 1 frees
        bleUploadReset();                       // clears name/len (buf already null)
      } else bleUploadReset();
    }
  }
};

void bleInit() {
  NimBLEDevice::init(bleDeviceName().c_str());
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new DfServerCB());

  NimBLEService *svc = bleServer->createService(BLE_SVC_UUID);
  bleStatusChar = svc->createCharacteristic(BLE_STATUS_UUID,
                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, BLE_VAL_MAX);
  bleGifsChar   = svc->createCharacteristic(BLE_GIFS_UUID,
                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, BLE_VAL_MAX);
  bleLogChar    = svc->createCharacteristic(BLE_LOG_UUID,
                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, BLE_VAL_MAX);
  bleEventsChar = svc->createCharacteristic(BLE_EVENTS_UUID,
                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, BLE_VAL_MAX);
  svc->createCharacteristic(BLE_WIFI_UUID, NIMBLE_PROPERTY::WRITE, 256)
     ->setCallbacks(new DfWifiCB());
  svc->createCharacteristic(BLE_CTRL_UUID, NIMBLE_PROPERTY::WRITE, BLE_VAL_MAX)
     ->setCallbacks(new DfCtrlCB());
  svc->createCharacteristic(BLE_UPLOAD_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, BLE_VAL_MAX)
     ->setCallbacks(new DfUploadCB());
  svc->start();

  bleStatusChar->setValue(ctlStatusJson().c_str());
  bleGifsChar->setValue(ctlListGifsJson().c_str());
  bleEventsChar->setValue(ctlListEventsJson().c_str());

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SVC_UUID);
  adv->setName(bleDeviceName().c_str());
  adv->enableScanResponse(true);
  adv->start();
  logLine("BLE up: '" + bleDeviceName() + "' (service " + String(BLE_SVC_UUID).substring(0, 8) + ")");
}

/* Called every loop() iteration on core 1; refreshes the read/notify
 * characteristics for a connected client (safe LittleFS access here). */
void bleTick() {
  if (!bleServer || !bleConnected) return;
  static uint32_t last = 0;
  uint32_t ms = millis();
  if (ms - last < 1000) return;
  last = ms;

  bleStatusChar->setValue(ctlStatusJson().c_str());
  bleStatusChar->notify();
  bleGifsChar->setValue(ctlListGifsJson().c_str());
  bleGifsChar->notify();
  bleEventsChar->setValue(ctlListEventsJson().c_str());
  bleEventsChar->notify();

  static uint32_t lastLogSeq = 0;
  if (logSeq != lastLogSeq) {
    lastLogSeq = logSeq;
    String t = ctlLogsText();
    if (t.length() > BLE_VAL_MAX - 12) t = t.substring(t.length() - (BLE_VAL_MAX - 12));
    bleLogChar->setValue(t.c_str());
    bleLogChar->notify();
  }
}
