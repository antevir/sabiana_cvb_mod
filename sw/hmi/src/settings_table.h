#pragma once

#define SETTINGS_TABLE(X) \
/*   TYPE,    DEFAULT   FIELD NAME      GET/SET NAME */ \
   X(String,  "",       ssid,           Ssid) \
   X(String,  "",       psk,            Psk) \
   X(int,     0,        mode,           Mode) \
   X(int,     0,        fanLevel,       FanLevel) \
   X(int,     21,       setTemp,        SetTemp) \
   X(int,     6,        tpMonStart,     TpMonStart) \
   X(int,     18,       tpMonEnd,       TpMonEnd) \
   X(int,     6,        tpTueStart,     TpTueStart) \
   X(int,     18,       tpTueEnd,       TpTueEnd) \
   X(int,     6,        tpWedStart,     TpWedStart) \
   X(int,     18,       tpWedEnd,       TpWedEnd) \
   X(int,     6,        tpThuStart,     TpThuStart) \
   X(int,     18,       tpThuEnd,       TpThuEnd) \
   X(int,     6,        tpFriStart,     TpFriStart) \
   X(int,     18,       tpFriEnd,       TpFriEnd) \
   X(int,     6,        tpSatStart,     TpSatStart) \
   X(int,     18,       tpSatEnd,       TpSatEnd) \
   X(int,     6,        tpSunStart,     TpSunStart) \
   X(int,     18,       tpSunEnd,       TpSunEnd) \
   X(int,     21,       tpActiveTemp,   TpActiveTemp) \
   X(int,     1,        tpActiveFan,    TpActiveFan) \
   X(int,     2,        tpActiveMode,   TpActiveMode) \
   X(int,     18,       tpInactiveTemp, TpInactiveTemp) \
   X(int,     0,        tpInactiveFan,  TpInactiveFan) \
   X(int,     0,        tpInactiveMode, TpInactiveMode) \
   X(int,     0,        fanCycleEn,     FanCycleEn) \
   X(int,     1,        fanCycleFan,    FanCycleFan) \
   X(int,     60,       fanCyclePeriod, FanCyclePeriod) \
   X(int,     5,        fanCycleDur,    FanCycleDur)
