/*
 * ESP32 Signal Analyzer
 * 6 analog + 8 digital, FFT, file storage on W25Q32
 */

#include <WiFi.h>
#include "config.h"
#include "flash.h"
#include "adc.h"
#include "fft.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n\n=== ESP32 Analyzer ===");

  // Wi-Fi AP поднимаем РАНЬШЕ остального (стабильнее на многих платах)
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);

  // channel 1, max 4 clients, SSID hidden=false
  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);
  Serial.printf("softAP(%s): %s\n", AP_SSID, ok ? "OK" : "FAIL");
  delay(200);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());

  flashReady = initFlash();
  Serial.println(flashReady ? "W25Q32 OK" : "W25Q32 FAIL");

  initADC();

  setupWeb();
  Serial.println("http://192.168.4.1");
  Serial.println("Wi-Fi: ESP32-Analyzer / 12345678");
}

void loop() {
  server.handleClient();
}
