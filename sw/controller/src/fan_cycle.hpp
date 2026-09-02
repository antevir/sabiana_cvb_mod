#pragma once

#include <stdint.h>

class FanCycle {
   private:
      bool enabled = false;
      bool activePeriod = false;
      bool eligible = false;
      bool running = false;
      uint8_t fanLevel = 1;
      uint16_t periodMinutes = 60;
      uint16_t durationMinutes = 5;
      uint32_t cycleStartMs = 0;

      void reset() {
         eligible = false;
         running = false;
      }

   public:
      void configure(bool enabled, bool activePeriod, uint8_t fanLevel,
                     uint16_t periodMinutes, uint16_t durationMinutes) {
         this->enabled = enabled;
         this->activePeriod = activePeriod;
         this->fanLevel = fanLevel < 1 ? 1 : (fanLevel > 3 ? 3 : fanLevel);
         this->periodMinutes = periodMinutes == 0 ? 1 : periodMinutes;
         this->durationMinutes = durationMinutes == 0 ? 1 : durationMinutes;
         if (this->durationMinutes > this->periodMinutes) {
            this->durationMinutes = this->periodMinutes;
         }
         if (!enabled || !activePeriod) {
            reset();
         }
      }

      bool tick(bool heatingActive, bool fanAlreadyRunning, uint32_t nowMs) {
         if (!enabled || !activePeriod || heatingActive || fanAlreadyRunning) {
            reset();
            return false;
         }

         const uint32_t periodMs = (uint32_t)periodMinutes * 60U * 1000U;
         const uint32_t durationMs = (uint32_t)durationMinutes * 60U * 1000U;

         if (!eligible) {
            eligible = true;
            running = true;
            cycleStartMs = nowMs;
            return true;
         }

         const uint32_t elapsedMs = nowMs - cycleStartMs;
         if (running) {
            if (elapsedMs >= durationMs) {
               running = false;
            }
         } else if (elapsedMs >= periodMs) {
            running = true;
            cycleStartMs = nowMs;
         }
         return running;
      }

      void stop() {
         reset();
      }

      void disable() {
         enabled = false;
         reset();
      }

      uint8_t getFanLevel() const {
         return fanLevel;
      }

      bool isRunning() const {
         return running;
      }
};
