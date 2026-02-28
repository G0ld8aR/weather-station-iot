#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_SHT31.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

// ---------- OLED (Heltec V2) ----------
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,
  /* clock=*/ 15,
  /* data=*/ 4,
  /* reset=*/ 16
);

// ---------- SHT31 on accessible pins ----------
static const int SHT_SDA = 33;
static const int SHT_SCL = 32;

Adafruit_SHT31 sht31;
bool shtOk = false;

BLEAdvertising* pAdvertising = nullptr;
static const uint16_t MFG_ID = 0x02E5;

void oled3(const String& a, const String& b, const String& c) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, a.c_str());
  u8g2.drawStr(0, 28, b.c_str());
  u8g2.drawStr(0, 44, c.c_str());
  u8g2.sendBuffer();
}

void bleUpdateAdvert(float tF, float rh) {
  if (!pAdvertising) return;

  BLEAdvertisementData adv;
  adv.setName("WeatherNode");

  // Now advertising Fahrenheit
  String payload = "WS:T=" + String(tF, 2) + ",H=" + String(rh, 1);

  // Manufacturer data begins with 2-byte ID (little endian)
  String mfg;
  mfg += char(MFG_ID & 0xFF);
  mfg += char((MFG_ID >> 8) & 0xFF);
  mfg += payload;

  adv.setManufacturerData(mfg);

  pAdvertising->stop();
  pAdvertising->setAdvertisementData(adv);
  pAdvertising->start();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  u8g2.begin();
  oled3("Booting", "OLED OK", "Init SHT31...");

  Wire.begin(SHT_SDA, SHT_SCL);
  shtOk = sht31.begin(0x44);

  BLEDevice::init("WeatherNode");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);
  pAdvertising->start();

  if (!shtOk) {
    oled3("SHT31 NOT FOUND", "SDA 33  SCL 32", "VIN 3V3 GND GND");
    Serial.println("SHT31 NOT FOUND at 0x44");
  } else {
    oled3("SHT31 FOUND", "BLE advertising", "Waiting reads...");
    Serial.println("SHT31 FOUND at 0x44");
  }
}

void loop() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < 2000) return;
  lastMs = now;

  if (!shtOk) {
    oled3("SHT31 NOT FOUND", "Check wiring", "SDA33 SCL32");
    return;
  }

  float tC = sht31.readTemperature();
  float h  = sht31.readHumidity();

  if (isnan(tC) || isnan(h)) {
    oled3("SHT31 READ ERROR", "Check wiring", "Will retry...");
    Serial.println("SHT31 read error");
    return;
  }

  // Convert Celsius to Fahrenheit
  float tF = (tC * 9.0 / 5.0) + 32.0;

  bleUpdateAdvert(tF, h);

  oled3("SHT31 OK + BLE",
        "Temp F: " + String(tF, 2),
        "RH: " + String(h, 1) + "%");

  Serial.print("Advertising WS:T=");
  Serial.print(tF, 2);
  Serial.print(",H=");
  Serial.println(h, 1);
}
