#pragma once

#include <Arduino.h>
#include "common.h"

#define DUMP_RX_IR_DATA 1
#define DUMP_TX_IR_DATA 1

typedef void (*ir_command_t)(op_mode_t mode, uint8_t fan, bool swing, uint8_t temp);

class Ir {
   private:
      uint8_t txPin;
      uint8_t rxPin;
      ir_command_t irCmdCb;
      
      bool rxGotSync = false;
      uint32_t rxLastTime = 0;
      uint8_t rxIsrData[16];
      int rxBit = 0;
      int rxByte = 0;
      
      uint8_t rxData[11];
      volatile bool rxGotData = false;
      
      
      volatile uint8_t txBitCount = 0;
      volatile uint8_t txData[11];
      
      uint8_t txCurBit = 0;
      bool txSyncBit = false;
      uint8_t txPulseLowCounter = 0;
      uint8_t txPulseHighCounter = 0;
      
      static void timerIsr(void);
      static void gpioIsr(void);
      
      void txTick(void);
      void rxPinEdge(void);
      
   public:
      Ir(uint8_t txPin, uint8_t rxPin);
      void init(ir_command_t irCmdCb);
      void handle(void);
      void sendCommand(op_mode_t mode, uint8_t fan, bool swing, uint8_t temp);
};