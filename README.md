# Weather Station IoT

A custom ESP32-based weather station built to measure real conditions at home instead of relying on generalized weather app data. The project uses a Heltec outdoor node for sensing and BLE transmission, plus an Elecrow CrowPanel 4.2 inch e-paper display for the indoor dashboard.

## Project Overview

This started as a soldering and electronics challenge and turned into a practical IoT build. My goal is to create a field-ready, solar-friendly weather station that can reliably measure temperature, humidity, wind speed, wind direction, and rainfall, then display those readings on a clean low-power screen indoors.

## Current Working Architecture

### Outdoor Sensor Node (Heltec WiFi LoRa 32 V2)
- Reads all sensors
- Builds BLE payload
- Transmits every 3 minutes (daytime)
- Deep sleeps hourly at night

**Sensors:**
- SHT30 temperature & humidity
- Anemometer (wind speed)
- Wind vane (direction via ADC)
- Tipping bucket rain gauge

**Power behavior:**
- Day: active, continuous counting + transmit every 3 min
- Night: send once → deep sleep 1 hour

**OLED:**
- Off by default
- Only on during transmit or daytime errors

---

### Indoor Display (CrowPanel 4.2” E-Paper)
- BLE scanner (no pairing required)
- Parses payload and renders dashboard

**Dashboard includes:**
- Temperature
- Humidity
- Wind speed
- Rainfall
- Wind direction compass + arrow
- 24-point temp trend
- Last update timer

---

## BLE Payload Format
