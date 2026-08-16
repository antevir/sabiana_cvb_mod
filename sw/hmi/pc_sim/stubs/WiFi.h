#pragma once

#define WL_CONNECTED    3
#define WL_DISCONNECTED 6

class WiFiClass {
public:
   int status() const { return WL_CONNECTED; }
};

inline WiFiClass WiFi;
