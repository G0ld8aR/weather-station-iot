/*
  CrowPanel ESP32-S3 4.2" E-Paper (SSD1683) BLE Weather Receiver

  - Scans for your Heltec WeatherNode advertiser
  - Matches Manufacturer ID: 0x02E5
  - Parses payload: "WS:T=72.34,H=45.6"
  - Displays Temp (F) + Humidity (%) on the CrowPanel

  Libraries:
    - GxEPD2 (Library Manager)
  BLE:
    - built-in ESP32 BLE (BLEDevice.h)

  CrowPanel 4.2 pins:
    SCK=12, MOSI=11, CS=45, DC=46, RST=47, BUSY=48, PWR=7

  Required header tab:
    portrait_dither_400x300.h  (the generated 400x300 bitmap)
*/

#include <Arduino.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <pgmspace.h>

#include "portrait_dither_400x300.h"

// ---- CrowPanel 4.2 pins ----
static const int EPD_SCK  = 12;
static const int EPD_MOSI = 11;
static const int EPD_PWR  = 7;
static const int EPD_CS   = 45;
static const int EPD_DC   = 46;
static const int EPD_RST  = 47;
static const int EPD_BUSY = 48;

// Panel type (common CrowPanel 4.2 SSD1683)
GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> display(
  GxEPD2_420_GYE042A87(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ---- WeatherNode target ----
static const uint16_t TARGET_MFG_ID = 0x02E5;

// Scan cadence
static const uint32_t SCAN_EVERY_MS = 12000;
static uint32_t lastScanMs = 0;

// State
static bool found = false;
static float weatherTempF = NAN;
static float weatherHum   = NAN;

struct Seen { String name; int rssi; };
static Seen top[3] = {{"",-999},{"",-999},{"",-999}};

BLEScan* pBLEScan = nullptr;

// ---------- helpers ----------
static void considerTop(const String& name, int rssi) {
  for (int k = 0; k < 3; k++) {
    if (rssi > top[k].rssi) {
      for (int j = 2; j > k; j--) top[j] = top[j - 1];
      top[k] = {name, rssi};
      break;
    }
  }
}

static String clip(const String& s, int maxChars) {
  if ((int)s.length() <= maxChars) return s;
  return s.substring(0, maxChars - 1) + ".";
}

// Draw background portrait + top status panel frame
static void drawBackgroundWithPanel(int panelH) {
  // Fullscreen portrait
  display.drawBitmap(
    0, 0,
    portrait_dither_400x300,
    portrait_dither_400x300_w,
    portrait_dither_400x300_h,
    GxEPD_BLACK
  );

  // White status panel
  display.fillRect(0, 0, display.width(), panelH, GxEPD_WHITE);
  display.drawRect(0, 0, display.width(), panelH, GxEPD_BLACK);
}

static void drawStatusScreen(const char* title, const char* line1) {
  const int panelH = 78;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawBackgroundWithPanel(panelH);

    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 22);
    display.print(title);

    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 50);
    display.print(line1);
  } while (display.nextPage());
}

static void drawFoundScreen() {
  const int panelH = 140;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawBackgroundWithPanel(panelH);

    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 25);
    display.print("OUTDOOR WEATHER");

    display.setFont(&FreeMono9pt7b);

    display.setCursor(10, 60);
    display.print("Temp: ");
    display.print(weatherTempF, 1);
    display.print(" F");

    display.setCursor(10, 90);
    display.print("Humidity: ");
    display.print(weatherHum, 1);
    display.print(" %");

  } while (display.nextPage());
}

static void drawNotFoundScreen(int seenCount) {
  const int panelH = 128;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawBackgroundWithPanel(panelH);

    // This is the line that got cut off in your paste earlier
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 22);
    display.print("NOT FOUND");

    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 48);
    display.print("Seen: ");
    display.print(seenCount);

    display.setCursor(10, 78);
    display.print("Top1: ");
    display.print(clip(top[0].name, 20));
    display.print(" (");
    display.print(top[0].rssi);
    display.print(")");

    display.setCursor(10, 100);
    display.print("Top2: ");
    display.print(clip(top[1].name, 20));
    display.print(" (");
    display.print(top[1].rssi);
    display.print(")");

  } while (display.nextPage());
}

// ---------- BLE callbacks ----------
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    if (!d.haveManufacturerData()) return;

    String raw = d.getManufacturerData();

    // Look for ASCII WS marker anywhere
    if (raw.indexOf("WS:") < 0) return;

    Serial.print("RAW LEN: ");
    Serial.println(raw.length());

    Serial.print("RAW HEX: ");
    for (int i = 0; i < raw.length(); i++) {
      uint8_t b = raw[i];
      if (b < 16) Serial.print("0");
      Serial.print(b, HEX);
      Serial.print(" ");
    }
    Serial.println();

    Serial.print("RAW ASCII: ");
    Serial.println(raw);
    Serial.println("-----");
  }
};

 


// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);
  delay(50);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  display.init(115200);
  display.setRotation(0); // native landscape 400x300

  // Optional: one-time clear to reduce ghosting
  display.setFullWindow();
  display.firstPage();
  do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());

  drawStatusScreen("BLE Weather", "Ready");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(60);

  lastScanMs = 0;
}

void loop() {
  uint32_t now = millis();
  if (now - lastScanMs < SCAN_EVERY_MS) { delay(50); return; }
  lastScanMs = now;

  // reset scan state
  found = false;
  weatherTempF = NAN;
  weatherHum   = NAN;
  top[0] = {"",-999}; top[1] = {"",-999}; top[2] = {"",-999};

  drawStatusScreen("BLE Weather", "Scanning 5s...");

  BLEScanResults* results = pBLEScan->start(5, false);
  int n = results ? results->getCount() : 0;
  pBLEScan->clearResults();

  if (found) drawFoundScreen();
  else drawNotFoundScreen(n);
}
