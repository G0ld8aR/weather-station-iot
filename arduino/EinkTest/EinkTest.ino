/*
  Heltec WiFi LoRa 32 V2
  BLE Advertising Loop Test + OLED via U8g2 (direct I2C)

  OLED pins (typical Heltec V2):
  SDA = GPIO 4
  SCL = GPIO 15
  RST = GPIO 16
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

static BLEAdvertising* pAdvertising = nullptr;
static const char* DEVICE_NAME = "HELTEC-LOOP";

// U8g2 OLED (SSD1306 128x64) over HW I2C, reset pin GPIO16
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 16);

uint32_t counter = 0;
uint32_t lastTickMs = 0;

void drawOLED(uint32_t n) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 14, "BLE Loop TX");
  u8g2.drawStr(0, 34, "Count:");
  u8g2.setCursor(60, 34);
  u8g2.print(n);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // OLED init
  Wire.begin(4, 15);   // SDA, SCL
  u8g2.begin();
  drawOLED(0);

  // BLE init
  BLEDevice::init(DEVICE_NAME);
  pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.setManufacturerData(String("HELTEC_LOOP:0"));

  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  pAdvertising->start();

  Serial.println("BLE Advertising started.");
}

void loop() {
  uint32_t now = millis();

  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    counter++;

    String payload = "HELTEC_LOOP:" + String(counter);

    BLEAdvertisementData advData;
    advData.setName(DEVICE_NAME);
    advData.setManufacturerData(payload);
    pAdvertising->setAdvertisementData(advData);

    drawOLED(counter);
    Serial.println(payload);
  }
}
