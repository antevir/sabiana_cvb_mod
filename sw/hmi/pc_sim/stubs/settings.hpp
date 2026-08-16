#pragma once

#include "Arduino.h"

class Settings {
public:
   int getSetTemp() { return setTemp; }
   void setSetTemp(int v) { setTemp = v; }
   int getMode() { return mode; }
   void setMode(int v) { mode = v; }
   int getFanLevel() { return fanLevel; }
   void setFanLevel(int v) { fanLevel = v; }

private:
   int setTemp = 21;
   int mode = 2;      /* heat */
   int fanLevel = 1;  /* low */
};
