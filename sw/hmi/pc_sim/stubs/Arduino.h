#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "WiFi.h"

inline bool getLocalTime(struct tm *info)
{
   memset(info, 0, sizeof(*info));
   info->tm_hour = 18;
   info->tm_min = 42;
   return true;
}
