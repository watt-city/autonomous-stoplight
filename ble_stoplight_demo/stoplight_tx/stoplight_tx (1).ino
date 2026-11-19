/*
 * stoplight_tx.ino  —  ESP32-S3 stoplight + BLE broadcaster
 *
 * Cycles the onboard RGB LED  YELLOW -> RED -> GREEN -> (repeat)
 * and broadcasts the current state as the board's BLE name, e.g.
 *     "stoplight 1 - green"
 * so nRF Connect (or any receiver) shows the live state with no pairing.
 *
 * Uses the same advertising calls as the team's working ESP32S3_Car sketch:
 * the library builds the packet; we only change the name and restart it.
 *
 * Board:  ESP32S3 Dev Module   (Arduino-ESP32 core 3.3.x, also builds on 4.x)
 * Tools:  cable in UART port -> USB CDC On Boot = Disabled
 *         cable in USB port  -> USB CDC On Boot = Enabled
 */

#include <Arduino.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 4
  #include <BLE.h>
#else
  #include <BLEDevice.h>
  #include <BLEAdvertising.h>
#endif

// ---------------- user settings ----------------
#define STOPLIGHT_ID   1        // change per stoplight (keep it ≤ 2 digits)
#define RGB_PIN        38       // board silkscreen "RGB@IO38" = 38; some DevKitC-1 v1.0 boards use 48
#define BRIGHTNESS     40       // 0–255; the WS2812 is blinding at 255

// Optional: also drive discrete LEDs (e.g. the stoplight PCB). Assumes active-high.
#define USE_DISCRETE_LEDS 0
#define PIN_RED     42
#define PIN_YELLOW  41
#define PIN_GREEN   40

// Durations in ms
#define T_GREEN   5000
#define T_YELLOW  2000
#define T_RED     5000
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

// Packet: flags (3 B) + TX power (3 B) + name (2 + 20 B for "stoplight 1 - yellow")
// = 28 of the 31 bytes allowed, so the name stays in the main broadcast packet.
void broadcast(const char* msg) {
#if ESP_ARDUINO_VERSION_MAJOR >= 4
  adv.stop();
  adv.setName(msg);
  adv.start();
#else
  BLEDevice::stopAdvertising();
  adv->setName(msg);
  BLEDevice::startAdvertising();
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
  adv.setScanResponse(false);       // keep the name in the main packet so passive scanners see it
  adv.setInterval(20, 40);          // ms
#else
  #if !defined(CONFIG_NIMBLE_ENABLED) && !defined(CONFIG_BT_NIMBLE_ENABLED)
    #error "This sketch expects the NimBLE stack (ESP32-S3 on core 3.3.x uses it)."
  #endif
  BLEDevice::init(devName);
  adv = BLEDevice::getAdvertising();
  adv->setScanResponse(true);       // same as the working car sketch
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
  broadcast(msg);                   // board's BLE name is now e.g. "stoplight 1 - red"
  Serial.println(msg);

  delay(s.durationMs);              // BLE runs in its own task, so delay() is fine

  state = (LightState)((state + 1) % 3);   // YELLOW -> RED -> GREEN -> YELLOW
}
