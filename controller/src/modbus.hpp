#pragma once

#include <Arduino.h>
#include "common.h"

#define DUMP_MODBUS_FRAMES 1

class Modbus {
   private:
      HardwareSerial &serial;
   
      uint8_t rxFrame[256];
      size_t rxFramePos = 0;
      unsigned long rxLastFrameTime = 0;
      
      void handleFrame(uint8_t *pData, size_t length);
   public:
      Modbus(HardwareSerial &serial) :
         serial(serial)
      {
      }
      void handle(void);
      void sendFrame(op_mode_t mode, fan_level_t level, uint8_t setTemp, float airTemp);
};