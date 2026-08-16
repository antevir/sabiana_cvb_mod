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
   int getFanCycleEn() { return fanCycleEn; }
   void setFanCycleEn(int v) { fanCycleEn = v; }
   int getFanCycleFan() { return fanCycleFan; }
   void setFanCycleFan(int v) { fanCycleFan = v; }
   int getFanCyclePeriod() { return fanCyclePeriod; }
   void setFanCyclePeriod(int v) { fanCyclePeriod = v; }
   int getFanCycleDur() { return fanCycleDur; }
   void setFanCycleDur(int v) { fanCycleDur = v; }

private:
   int setTemp = 21;
   int mode = 2;      /* heat */
   int fanLevel = 1;  /* low */
   int fanCycleEn = 1;
   int fanCycleFan = 1;
   int fanCyclePeriod = 60;
   int fanCycleDur = 5;
};
