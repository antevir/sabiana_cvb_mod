#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

static void connectWifi()
{
   WiFi.persistent(true);
   WiFi.setSleep(false);
   WiFi.begin();

   uint32_t start = millis();
   while (WiFi.status() != WL_CONNECTED && (millis() - start) < 8000) {
      delay(100);
   }
   if (WiFi.status() == WL_CONNECTED) {
      return;
   }

   if (!LittleFS.begin(false)) {
      return;
   }
   File f = LittleFS.open("/settings.txt", "r");
   if (!f) {
      return;
   }
   String ssid = f.readStringUntil('\n');
   String psk = f.readStringUntil('\n');
   f.close();
   ssid.trim();
   psk.trim();
   if (ssid.length() > 0) {
      WiFi.begin(ssid.c_str(), psk.c_str());
   }
}

void setup()
{
   setCpuFrequencyMhz(240);
   disableCore0WDT();
   disableCore1WDT();
   connectWifi();

   ArduinoOTA.setHostname("FanHmi");
   ArduinoOTA.setTimeout(30000);
   ArduinoOTA.begin();
   MDNS.begin("FanHmi");
}

void loop()
{
   ArduinoOTA.handle();
   delay(1);
}
