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

      // RX ISR state - all variables used in rxPinEdge() ISR
      struct RxIsrState {
         bool gotSync = false;
         uint32_t lastTime = 0;
         uint32_t markDuration = 0;  // Duration of mark (LOW pulse)
         uint32_t spaceDuration = 0; // Duration of space (HIGH pulse)
         bool waitingForSpace = false; // Waiting for space after mark
         uint8_t isrData[16] = {0};
         int bit = 0;
         int byte = 0;
      } rxState;

      // RX data shared between ISR and main thread
      uint8_t rxData[11];
      uint8_t rxRawByte8 = 0;  // Raw byte 8 for swing detection
      volatile bool rxGotData = false;

      // TX ISR state - all variables used in txTick() ISR
      struct TxIsrState {
         uint8_t curBit = 0;
         bool syncBit = false;
         uint8_t pulseLowCounter = 0;
         uint8_t pulseHighCounter = 0;
      } txState;

      // TX data shared between ISR and main thread
      volatile uint8_t txBitCount = 0;
      volatile uint8_t txData[11];

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