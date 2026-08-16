#pragma once

#include <stdio.h>

struct FanCycleConfig {
   static constexpr int FAN_MIN = 1;
   static constexpr int FAN_MAX = 3;
   static constexpr int PERIOD_COUNT = 4;
   static constexpr int DUR_COUNT = 4;

   static const int *periodValues() {
      static const int values[] = { 15, 30, 60, 120 };
      return values;
   }

   static const int *durationValues() {
      static const int values[] = { 5, 10, 15, 30 };
      return values;
   }

   static int clamp(int value, int lo, int hi) {
      if (value < lo) return lo;
      if (value > hi) return hi;
      return value;
   }

   static int nearestIndex(const int *values, int count, int value) {
      int best = 0;
      int bestDiff = values[0] - value;
      if (bestDiff < 0) bestDiff = -bestDiff;
      for (int i = 1; i < count; i++) {
         int diff = values[i] - value;
         if (diff < 0) diff = -diff;
         if (diff < bestDiff) {
            best = i;
            bestDiff = diff;
         }
      }
      return best;
   }

   static int periodIndex(int minutes) {
      return nearestIndex(periodValues(), PERIOD_COUNT, minutes);
   }

   static int durationIndex(int minutes) {
      return nearestIndex(durationValues(), DUR_COUNT, minutes);
   }

   static int snapPeriod(int minutes) {
      return periodValues()[periodIndex(minutes)];
   }

   static int snapDuration(int minutes) {
      return durationValues()[durationIndex(minutes)];
   }

   static int maxDurationIndexForPeriod(int periodMinutes) {
      const int *values = durationValues();
      int idx = DUR_COUNT - 1;
      while (idx > 0 && values[idx] > periodMinutes) {
         idx--;
      }
      return idx;
   }

   static void formatMinutes(int minutes, char *buf, int bufSize) {
      if (minutes >= 60 && (minutes % 60) == 0) {
         snprintf(buf, bufSize, "%dh", minutes / 60);
      } else {
         snprintf(buf, bufSize, "%dm", minutes);
      }
   }
};
