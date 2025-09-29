/***************************************************************
 * Project: High-Speed File Download to SPIFFS on ESP32
 * Author: Ramireddy Allugunti
 * Objective: Download a file via HTTPS and save it to SPIFFS
 *            with higher speed (1–2 Mbit/s possible)
 ***************************************************************/

// ---------------- Header Files ----------------
#include <WiFi.h>              // Core WiFi support
#include <HTTPClient.h>        // High-level HTTP(S) client
#include <WiFiClientSecure.h>  // Secure HTTPS client
#include <FS.h>                // File system base class
#include <SPIFFS.h>            // SPI Flash File System (internal flash storage)

// ---------------- WiFi Configuration ----------------
const char* WIFI_SSID     = "Airtel_Sri Srinivasa pg 1st flr";  // WiFi SSID
const char* WIFI_PASSWORD = "AVR994529";                        // WiFi Password

// ---------------- Download Sources ----------------
// You can list multiple URLs here (failover support)
String downloadURLS[] = {
  "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
};
const int NUM_URLS = sizeof(downloadURLS) / sizeof(downloadURLS[0]); // Number of URLs

// ---------------- Target File ----------------
const char* TARGET_FILE = "/download.bin";   // Destination file inside SPIFFS

// ---------------- Setup Function ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected Successfully!");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  // Initialize SPIFFS (flash file system)
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS!");
    while (1); // Halt if SPIFFS fails
  }

  // Try downloading from multiple URLs (first successful one is used)
  bool downloadOK = false;
  for (int i = 0; i < NUM_URLS; i++) {
    Serial.println("Trying URL: " + downloadURLS[i]);
    if (downloadToSPIFFS(downloadURLS[i], TARGET_FILE)) {
      downloadOK = true;
      break; // Stop once a successful download occurs
    }
  }

  if (!downloadOK) {
    Serial.println("All download attempts failed.");
  }
}

void loop() {
  // Nothing in loop – one-time download in setup()
}

// ---------------- Download Function ----------------
// Downloads file from HTTPS and saves to SPIFFS
bool downloadToSPIFFS(String fileURL, const char* destPath) {
  HTTPClient http;
  WiFiClientSecure* secureClient = new WiFiClientSecure; // Create secure client
  secureClient->setInsecure(); // Accept all certificates (no validation)

  // Connect to HTTPS server
  Serial.println("Connecting to " + fileURL);
  if (!http.begin(*secureClient, fileURL)) {
    Serial.println("HTTPS connection failed!");
    return false;
  }

  // Add optional headers (some servers require these)
  http.addHeader("User-Agent", "ESP32-Downloader");
  http.addHeader("Accept-Encoding", "identity");

  // Send GET request
  int responseCode = http.GET();
  if (responseCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed, response = %d\n", responseCode);
    http.end();
    return false;
  }

  // Get file size from headers (if provided)
  int fileSize = http.getSize();
  Serial.printf("Content-Length: %d bytes\n", fileSize);

  // Allocate buffer for streaming chunks
  #define DOWNLOAD_BUFFER_SIZE 32768  // 32 KB buffer for high-speed transfer
  uint8_t* chunkBuffer = (uint8_t*)malloc(DOWNLOAD_BUFFER_SIZE);
  if (!chunkBuffer) {
    Serial.println("Buffer allocation failed!");
    http.end();
    return false;
  }

  // Optional RAM caching if file is small enough (< 2MB)
  uint8_t* ramCache = nullptr;
  if (fileSize > 0 && fileSize < (2 * 1024 * 1024)) {
    ramCache = (uint8_t*)malloc(fileSize);
  }

  // Download tracking variables
  int totalBytes = 0;
  unsigned long startTime = millis();
  unsigned long lastReport = startTime;
  WiFiClient* netStream = http.getStreamPtr(); // Stream pointer for raw download

  Serial.println("Downloading Started...");

  // ---------------- Download Loop ----------------
  while (http.connected() && (fileSize > 0 || fileSize == -1)) {
    size_t availBytes = netStream->available();
    if (availBytes) {
      // Cap available bytes to buffer size
      if (availBytes > DOWNLOAD_BUFFER_SIZE) availBytes = DOWNLOAD_BUFFER_SIZE;

      // Read data from network into buffer
      int bytesRead = netStream->readBytes((char*)chunkBuffer, availBytes);
      if (bytesRead <= 0) break; // No more data

      if (ramCache) {
        // Store into RAM (for later single SPIFFS write)
        memcpy(ramCache + totalBytes, chunkBuffer, bytesRead);
      } else {
        // Directly append to SPIFFS file
        File outFile = SPIFFS.open(destPath, FILE_APPEND);
        if (!outFile) {
          Serial.println("Failed to open SPIFFS file for writing!");
          free(chunkBuffer);
          http.end();
          return false;
        }
        outFile.write(chunkBuffer, bytesRead);
        outFile.close();
      }

      totalBytes += bytesRead;
      if (fileSize > 0) fileSize -= bytesRead;

      // Progress report every second
      unsigned long now = millis();
      if (now - lastReport > 1000) {
        float elapsedSec = (now - startTime) / 1000.0;
        float speedKBps = (totalBytes / 1024.0) / elapsedSec;
        Serial.printf("Downloaded: %d bytes | Speed: %.1f KB/s\n", totalBytes, speedKBps);
        lastReport = now;
      }
    }
    yield(); // Allow background WiFi/RTOS tasks
  }

  // ---------------- Final File Write ----------------
  if (ramCache) {
    // Write entire file from RAM to SPIFFS
    File outFile = SPIFFS.open(destPath, FILE_WRITE);
    if (!outFile) {
      Serial.println("Final SPIFFS write failed!");
      free(ramCache);
      free(chunkBuffer);
      http.end();
      return false;
    }
    outFile.write(ramCache, totalBytes);
    outFile.close();
    free(ramCache);
  }

  free(chunkBuffer);
  http.end();

  // ---------------- Download Summary ----------------
  unsigned long endTime = millis();
  float totalTimeSec = (endTime - startTime) / 1000.0;
  float avgSpeedKBps = (totalBytes / 1024.0) / totalTimeSec;
  Serial.printf("\nDownload complete!\nTotal Bytes: %d\nTime: %.2f s\nAvg Speed: %.1f KB/s\n",
                totalBytes, totalTimeSec, avgSpeedKBps);

  // Verify file size in SPIFFS
  File verifyFile = SPIFFS.open(destPath, FILE_READ);
  Serial.printf("SPIFFS file size: %d bytes\n", verifyFile.size());
  verifyFile.close();

  return (totalBytes > 0);
}
