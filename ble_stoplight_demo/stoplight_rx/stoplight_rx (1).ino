/*
 * stoplight_rx.ino  —  ESP32 BLE receiver for stoplight broadcasts
 *
 * Passively scans for boards whose BLE name is "stoplight <id> - <color>"
 * (sent by stoplight_tx), prints each state change, and mirrors the colour
 * on the onboard RGB LED. Handles multiple stoplights (IDs 0–15) at once.
 *
 * Works on any ESP32 with BLE (S3, C3, classic ESP32...).
 * Arduino-ESP32 core 3.3.x, also builds on 4.x.
 */

#include <Arduino.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 4
  #include <BLE.h>
#else
  #include <BLEDevice.h>
  #include <BLEScan.h>
  #include <BLEAdvertisedDevice.h>
#endif

// ---------------- user settings ----------------
#define HAS_RGB_LED  1        // set 0 if your receiver board has no WS2812
#define RGB_PIN      38       // board silkscreen "RGB@IO38" = 38; some DevKitC-1 v1.0 boards use 48
#define BRIGHTNESS   40
#define MAX_LIGHTS   16
// ------------------------------------------------

enum LightState { UNKNOWN, YELLOW, RED, GREEN };
LightState lastState[MAX_LIGHTS];   // de-dupes: we hear each state ~25x/second

// Diagnostics, printed every 5 s by loop()
volatile uint32_t advSeen = 0;        // every BLE advert heard (phones, laptops...)
volatile uint32_t stoplightSeen = 0;  // adverts named "stoplight ..."

// ======= put your own logic here (e.g. tell the car to stop on red) =======
void onStoplightChange(int id, LightState s, int rssi) {
#if HAS_RGB_LED
  switch (s) {
    case GREEN:  rgbLedWrite(RGB_PIN, 0, BRIGHTNESS, 0); break;
    case YELLOW: rgbLedWrite(RGB_PIN, BRIGHTNESS, BRIGHTNESS * 55 / 100, 0); break;
    case RED:    rgbLedWrite(RGB_PIN, BRIGHTNESS, 0, 0); break;
    default: break;
  }
#endif
}
// ==========================================================================

LightState parseColour(const char* c) {
  if (!strcmp(c, "green"))  return GREEN;
  if (!strcmp(c, "yellow")) return YELLOW;
  if (!strcmp(c, "red"))    return RED;
  return UNKNOWN;
}

// Called with the advertised name of every device that has one.
void handleName(const String& name, const String& addr, int rssi) {
  if (!name.startsWith("stoplight ")) return;
  stoplightSeen++;

  int id = -1;
  char colour[12] = {0};
  if (sscanf(name.c_str(), "stoplight %d - %11s", &id, colour) != 2) return;
  if (id < 0 || id >= MAX_LIGHTS) return;

  LightState s = parseColour(colour);
  if (s == UNKNOWN || s == lastState[id]) return;   // only act on changes
  lastState[id] = s;

  Serial.printf("[RX] %s   (from %s, RSSI %d dBm)\n", name.c_str(), addr.c_str(), rssi);
  onStoplightChange(id, s, rssi);
}

#if ESP_ARDUINO_VERSION_MAJOR >= 4
// ---------------- core 4.x ----------------
BLEScan scan;

void onAdv(const BLEAdvertisedDevice& dev) {
  advSeen++;
  if (!dev.haveName()) return;
  handleName(dev.getName(), dev.getAddress().toString(), dev.getRSSI());
}

void setupBLE() {
  BTStatus st = BLE.begin("Stoplight-RX");
  if (!st) {
    Serial.printf("BLE init failed: %s\n", st.toString());
    while (true) delay(1000);
  }
  scan = BLE.getScan();
  scan.setActiveScan(false);        // passive: the name is in the main advert packet
  scan.setInterval(100);
  scan.setWindow(99);               // ≈ continuous listening
  scan.setFilterDuplicates(false);  // we de-dupe ourselves
  scan.onResult(onAdv);
}

void scanOnce() {
  scan.startBlocking(1000);         // callbacks fire during the scan
  scan.clearResults();              // don't let every nearby phone pile up in RAM
}

#else
// ---------------- core 3.x ----------------
BLEScan* scan = nullptr;

class StoplightCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    advSeen++;
    if (!dev.haveName()) return;
    handleName(dev.getName(), dev.getAddress().toString(), dev.getRSSI());
  }
};

void setupBLE() {
  BLEDevice::init("Stoplight-RX");
  scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new StoplightCallbacks(), true); // true = report repeats
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(99);
}

void scanOnce() {
  scan->start(1, false);            // 1 s blocking scan
  scan->clearResults();
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
#if HAS_RGB_LED
  rgbLedWrite(RGB_PIN, 0, 0, 0);    // LED off until a stoplight message arrives
#endif
  Serial.printf("\nRX booting (core %s)...\n", ESP_ARDUINO_VERSION_STR);  // printed BEFORE BLE starts
  for (int i = 0; i < MAX_LIGHTS; i++) lastState[i] = UNKNOWN;
  setupBLE();
  Serial.println("Listening for stoplights...");
}

void loop() {
  scanOnce();

  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 5000) {
    lastReport = millis();
    // adverts > 0 but stoplight = 0  -> receiver works; the stoplight isn't broadcasting
    // adverts = 0                     -> the receiver's BLE scan isn't running
    Serial.printf("[RX] alive: %lu BLE adverts heard, %lu from stoplights\n",
                  (unsigned long)advSeen, (unsigned long)stoplightSeen);
  }
}
