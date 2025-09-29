# ESP32 High-Speed File Download to SPIFFS

## Project Description
This project demonstrates how to download a file from the internet over **HTTPS** using an ESP32 and save it to its internal **SPI Flash File System (SPIFFS)**. The code is optimized for **high-speed downloads**, capable of achieving **1–2 Mbit/s** under good network conditions.

It also provides download progress feedback and optional RAM caching for smaller files to improve write efficiency.

---

## Features
- Connects automatically to a specified WiFi network.
- Downloads files over HTTPS securely.
- Supports multiple URLs as fallback options.
- Saves files to SPIFFS with optional RAM caching.
- Reports real-time download progress and speed.
- Works for small (<2 MB) and large files efficiently.

---

## How It Works

1. **WiFi Connection:**  
   The ESP32 connects to a WiFi network using the provided SSID and password. Connection status is printed on the Serial Monitor.

2. **SPIFFS Initialization:**  
   SPIFFS is mounted to store downloaded files. If mounting fails, the program halts.

3. **Download Process:**  
   - The code loops through the list of URLs until a file is successfully downloaded.
   - Data is downloaded in **chunks** (default 32 KB buffer) to optimize memory and speed.
   - For files smaller than 2 MB, the data is temporarily stored in RAM to allow a single SPIFFS write at the end.
   - For larger files, the data is written directly to SPIFFS in chunks to prevent memory overflow.

4. **Progress Reporting:**  
   Every second, the program prints:
   - Total bytes downloaded
   - Download speed in KB/s
   - Final download statistics after completion

5. **Verification:**  
   After download, the program reads the SPIFFS file to verify the file size.

---

## Hardware Required
- ESP32 development board (any model with SPIFFS support)
- USB cable for programming and serial output
- Active WiFi network

---

## Software Required
- Arduino IDE (or PlatformIO)
- ESP32 Board Support installed
- Libraries included by default:
  - `WiFi.h`
  - `HTTPClient.h`
  - `WiFiClientSecure.h`
  - `FS.h`
  - `SPIFFS.h`

---

## Setup Instructions
1. Open the project in Arduino IDE.
2. Update WiFi credentials:
   ```cpp
   const char* WIFI_SSID = "Your_SSID";
   const char* WIFI_PASSWORD = "Your_PASSWORD";
