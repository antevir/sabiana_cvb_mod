#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>

#include "fan_cycle_config.hpp"
#include "settings.hpp"

class FanCycle {
   private:
      static constexpr int MODE_OFF = 0;
      static constexpr int MODE_FAN = 1;
      static constexpr int MODE_HEAT = 2;

      Settings *settings = nullptr;
      bool running = false;
      bool overrideOn = false;
      bool inActiveIdle = false;
      uint32_t cycleStartMs = 0;
      int outMode = MODE_FAN;
      int outFan = FanCycleConfig::FAN_MIN;

      void stopOverride() {
         running = false;
         overrideOn = false;
      }

      void startCycle(uint32_t nowMs, int fanLevel) {
         running = true;
         overrideOn = true;
         cycleStartMs = nowMs;
         outMode = MODE_FAN;
         outFan = fanLevel;
      }

      bool heatIsDrivingFan(int mode, float airTemp, int setTemp) const {
         if (mode == MODE_FAN) {
            return true;
         }
         if (mode != MODE_HEAT) {
            return false;
         }
         if (isnan(airTemp)) {
            return true;
         }
         return airTemp < (float)setTemp + 0.5f;
      }

   public:
      void init(Settings *settings) {
         this->settings = settings;
      }

      bool tick(bool activePeriod, int mode, int fanLevel, float airTemp, int setTemp) {
         (void)fanLevel;
         if (!settings) {
            return false;
         }

         const bool enabled = settings->getFanCycleEn() != 0;
         if (!enabled || !activePeriod) {
            const bool changed = overrideOn;
            stopOverride();
            inActiveIdle = false;
            return changed;
         }

         const int cycleFan = FanCycleConfig::clamp(settings->getFanCycleFan(),
                                                    FanCycleConfig::FAN_MIN,
                                                    FanCycleConfig::FAN_MAX);
         int periodMin = FanCycleConfig::snapPeriod(settings->getFanCyclePeriod());
         int durMin = FanCycleConfig::snapDuration(settings->getFanCycleDur());
         if (durMin > periodMin) {
            durMin = periodMin;
         }

         if (heatIsDrivingFan(mode, airTemp, setTemp)) {
            const bool changed = overrideOn;
            stopOverride();
            inActiveIdle = false;
            return changed;
         }

         const uint32_t nowMs = millis();
         const uint32_t periodMs = (uint32_t)periodMin * 60U * 1000U;
         const uint32_t durMs = (uint32_t)durMin * 60U * 1000U;

         if (!inActiveIdle) {
            inActiveIdle = true;
            startCycle(nowMs, cycleFan);
            return true;
         }

         if (running) {
            if (nowMs - cycleStartMs >= durMs) {
               stopOverride();
               return true;
            }
            if (!overrideOn || outFan != cycleFan) {
               overrideOn = true;
               outMode = MODE_FAN;
               outFan = cycleFan;
               return true;
            }
            return false;
         }

         if (nowMs - cycleStartMs >= periodMs) {
            startCycle(nowMs, cycleFan);
            return true;
         }
         return false;
      }

      bool overrideActive() const {
         return overrideOn;
      }

      int effectiveMode() const {
         return outMode;
      }

      int effectiveFan() const {
         return outFan;
      }
};
