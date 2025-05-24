#pragma once

#define SETTINGS_TABLE(X) \
/*   TYPE,    DEFAULT   FIELD NAME  GET/SET NAME */ \
   X(String,  "",       ssid,       Ssid) \
   X(String,  "",       psk,        Psk) \
   X(int,     0,        mode,       Mode) \
   X(int,     0,        fanLevel,   FanLevel) \
   X(int,     21,       setTemp,    SetTemp)
