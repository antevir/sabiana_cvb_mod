#pragma once

#include <WiFi.h>

class WifiLink {
   public:
      static void start(const String &ssid, const String &psk) {
         credSsid() = ssid;
         credPsk() = psk;
         WiFi.mode(WIFI_STA);
         WiFi.persistent(false);
         WiFi.setAutoReconnect(true);
         WiFi.setSleep(WIFI_PS_NONE);
         connectNow();
      }

      static void reconnect(const String &ssid, const String &psk) {
         credSsid() = ssid;
         credPsk() = psk;
         haveIp() = false;
         WiFi.setSleep(WIFI_PS_NONE);
         WiFi.disconnect(false, false);
         connectNow();
      }

      static void loop() {
         if (WiFi.status() == WL_CONNECTED) {
            if (!haveIp()) {
               haveIp() = true;
               pendingNotify() = true;
               WiFi.setSleep(WIFI_PS_MIN_MODEM);
            }
            return;
         }

         if (haveIp()) {
            haveIp() = false;
            WiFi.setSleep(WIFI_PS_NONE);
         }
         if (credSsid().length() == 0) {
            return;
         }
         uint32_t now = millis();
         if (lastBegin() != 0 && now - lastBegin() < 20000) {
            return;
         }
         WiFi.setSleep(WIFI_PS_NONE);
         WiFi.disconnect(false, false);
         connectNow();
      }

      static bool takeConnected() {
         if (!pendingNotify()) {
            return false;
         }
         pendingNotify() = false;
         return true;
      }

   private:
      static String &credSsid() { static String value; return value; }
      static String &credPsk() { static String value; return value; }
      static bool &haveIp() { static bool value = false; return value; }
      static bool &pendingNotify() { static bool value = false; return value; }
      static uint32_t &lastBegin() { static uint32_t value = 0; return value; }

      static void connectNow() {
         lastBegin() = millis();
         if (credSsid().length() == 0) {
            return;
         }
         WiFi.begin(credSsid().c_str(), credPsk().c_str());
      }
};
