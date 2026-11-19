/*
 * stoplight_tx.ino  —  ESP32-S3 stoplight + BLE broadcaster
 *
 * Cycles the onboard RGB LED  YELLOW -> RED -> GREEN -> (repeat)
 * and broadcasts the current state over BLE advertising, e.g.
 *     "stoplight 1 - green"
 * Any number of receivers can listen; no pairing or connection needed.
 *
 * Board:  ESP32S3 Dev Module   (builds on Arduino-ESP32 core 3.x or 4.x)
 * Tools:  USB CDC On Boot = Enabled  (only if you're plugged into the "USB" port
 *         instead of the "UART/COM" port and want Serial output)
 */

#include <Arduino.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 4
  #include <BLE.h>                 // core 4.x: new BLE API
#else
  #include <BLEDevice.h>           // core 3.x: classic BLEDevice API
  #include <BLEAdvertising.h>
#endif

// ---------------- user settings ----------------
#define STOPLIGHT_ID   1        // change per stoplight (keep it ≤ 2 digits)
#define RGB_PIN        38       // board silkscreen "RGB@IO38" = 38; some DevKitC-1 v1.0 boards use 48
#define BRIGHTNESS     40       // 0–255; the WS2812 is blinding at 255

// Optional: also drive discrete LEDs (e.g. a stoplight PCB). Assumes active-high.
#define USE_DISCRETE_LEDS 0
#define PIN_RED     42
#define PIN_YELLOW  41
#define PIN_GREEN   40

// Durations in ms
#define T_GREEN   5000
#define T_YELLOW  2000
#define T_RED     5000

// 0xFFFF = Bluetooth SIG "no company / testing" ID. The receiver filters on it.
#define COMPANY_ID 0xFFFF
// ------------------------------------------------

enum LightState { YELLOW, RED, GREEN };

struct StateInfo {
  const char* name;
  uint8_t r, g, b;      // full-scale colour, scaled by BRIGHTNESS
  uint32_t durationMs;
};

const StateInfo STATES[] = {
  { "yellow", 255, 140,   0, T_YELLOW },
  { "red",    255,   0,   0, T_RED    },
  { "green",    0, 255,   0, T_GREEN  },
};

LightState state = YELLOW;   // starts on yellow, per spec

#if ESP_ARDUINO_VERSION_MAJOR >= 4
BLEAdvertising adv;
#else
BLEAdvertising* adv = nullptr;
#endif

void setLed(const StateInfo& s) {
  rgbLedWrite(RGB_PIN,
              (s.r * BRIGHTNESS) / 255,
              (s.g * BRIGHTNESS) / 255,
              (s.b * BRIGHTNESS) / 255);
#if USE_DISCRETE_LEDS
  digitalWrite(PIN_RED,    state == RED);
  digitalWrite(PIN_YELLOW, state == YELLOW);
  digitalWrite(PIN_GREEN,  state == GREEN);
#endif
}

// Payload budget is 31 bytes:
//   flags AD (3) + manufacturer AD header (2) + company ID (2) = 7
//   → 24 bytes left for text. "stoplight 99 - yellow" is 21. OK.
void broadcast(const char* msg) {
#if ESP_ARDUINO_VERSION_MAJOR >= 4
  BLEAdvertisementData data;
  data.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
  data.setManufacturerData(COMPANY_ID, (const uint8_t*)msg, strlen(msg));
  adv.stop();
  adv.setAdvertisementData(data);
  adv.start();                      // 0 = advertise until stopped
#else
  String mfg;
  mfg += (char)(COMPANY_ID & 0xFF); // company ID is little-endian on air
  mfg += (char)(COMPANY_ID >> 8);
  mfg += msg;
  BLEAdvertisementData data;
  data.setFlags(0x06);              // LE General Discoverable | BR/EDR not supported
  data.setManufacturerData(mfg);
  adv->stop();
  adv->setAdvertisementData(data);
  adv->start();
#endif
}

void setupBLE(const char* devName) {
#if ESP_ARDUINO_VERSION_MAJOR >= 4
  BTStatus st = BLE.begin(devName);
  if (!st) {
    Serial.printf("BLE init failed: %s\n", st.toString());
    while (true) delay(1000);
  }
  adv = BLE.getAdvertising();
  // Name doesn't fit next to the message (31-byte limit), so it goes in the
  // scan response that phones request: nRF Connect then shows "Stoplight-1".
  adv.setType(BLEAdvType::ScannableUndirected);
  adv.setScanResponse(true);
  BLEAdvertisementData scanResp;
  scanResp.setName(devName);
  adv.setScanResponseData(scanResp);
  adv.setInterval(20, 40);          // ms → receivers see changes within ~40 ms
#else
  BLEDevice::init(devName);
  adv = BLEDevice::getAdvertising();
  adv->setScanResponse(true);       // core 3.x puts the device name in the scan response
  adv->setMinInterval(0x20);        // units of 0.625 ms → 20 ms
  adv->setMaxInterval(0x40);        // 40 ms
#endif
}

void setup() {
  Serial.begin(115200);
  delay(300);

#if USE_DISCRETE_LEDS
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
#endif

  char devName[20];
  snprintf(devName, sizeof(devName), "Stoplight-%d", STOPLIGHT_ID);
  Serial.printf("\n%s booting...\n", devName);   // printed BEFORE BLE starts
  setupBLE(devName);
  Serial.printf("%s ready (core %s)\n", devName, ESP_ARDUINO_VERSION_STR);
}

void loop() {
  const StateInfo& s = STATES[state];

  char msg[24];
  snprintf(msg, sizeof(msg), "stoplight %d - %s", STOPLIGHT_ID, s.name);

  setLed(s);
  broadcast(msg);                   // repeats this packet until the next change
  Serial.println(msg);

  delay(s.durationMs);              // BLE runs in its own task, so delay() is fine

  state = (LightState)((state + 1) % 3);   // YELLOW -> RED -> GREEN -> YELLOW
}
