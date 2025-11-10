#include <TLog.h>

#include "ir.hpp"

#define IR_LEADER_MARK_MIN_US       4000
#define IR_MARK_LONG_THRESHOLD_US   800
#define IR_SPACE_LONG_THRESHOLD_US  800

#define IR_MODE_OFF    0x86
#define IR_MODE_AUTO   0x18
#define IR_MODE_FAN    0x58
#define IR_MODE_HEAT   0x78
#define IR_MODE_COOL   0x98

static hw_timer_t *pIrTxTimer = NULL;
static Ir *pInstance = NULL;

static uint16_t calcIrChecksum(const uint8_t *pData)
{
   int16_t chkSum = -0x200;
   for (int i = 0; i < 9; i++) {
      chkSum += pData[i];
   }
   chkSum += pData[9] & 0x0F;
   return chkSum & 0xFF;
}

static bool verifyIrChecksum(const uint8_t *pData)
{
   uint8_t calcChkSum = calcIrChecksum(pData);
   uint8_t rxChkSum = (pData[9] >> 4) | (pData[10] << 4);
   return calcChkSum == rxChkSum;
}

static uint8_t toIrModeCode(op_mode_t mode)
{
   // Return mode code (upper 4 bits) that matches decoder expectations
   // Enum: OFF=0, FAN=1, HEAT=2, COOL=3, AUTO=4
   // IR codes: auto=0, fan=2, heat=3, cool=4
   switch(mode)
   {
      case OP_MODE_AUTO: return 0;  // Mode code 0 = auto
      case OP_MODE_FAN:  return 2;  // Mode code 2 = fan
      case OP_MODE_HEAT: return 3;  // Mode code 3 = heat
      case OP_MODE_COOL: return 4;  // Mode code 4 = cool
      case OP_MODE_OFF:
      default: return 0;  // Default to auto for OFF
   }
}

static op_mode_t fromIrMode(uint8_t modeCode)
{
   // Mode is in upper 4 bits of byte 0
   switch(modeCode)
   {
      case 0: return OP_MODE_AUTO;
      case 2: return OP_MODE_FAN;
      case 3: return OP_MODE_HEAT;
      case 4: return OP_MODE_COOL;
      default: return OP_MODE_OFF;
   }
}

IRAM_ATTR
void Ir::timerIsr(void)
{
   if (pInstance) {
      pInstance->txTick();
   }
}

IRAM_ATTR
void Ir::gpioIsr()
{
   if (pInstance) {
      pInstance->rxPinEdge();
   }
}

IRAM_ATTR
void Ir::rxPinEdge(void)
{
   uint32_t time = esp_timer_get_time();
   uint32_t duration = time - rxState.lastTime;
   int pin = digitalRead(rxPin);

   if (pin == 1) {
      // HIGH transition: mark ended
      if (duration >= IR_LEADER_MARK_MIN_US) {
         // Sync detected - reset and start receiving
         memset(&rxState, 0, sizeof(rxState));
         rxState.gotSync = true;
         rxState.lastTime = time;
         return;
      }

      if (rxState.gotSync) {
         rxState.markDuration = duration;
         rxState.waitingForSpace = true;
      }
   } else {
      // LOW transition: space ended
      if (rxState.gotSync && rxState.waitingForSpace) {
         rxState.spaceDuration = duration;

         // Decode bit: 0 = short mark + long space, 1 = long mark + short space
         bool markLong = rxState.markDuration > IR_MARK_LONG_THRESHOLD_US;
         bool spaceLong = rxState.spaceDuration > IR_SPACE_LONG_THRESHOLD_US;

         bool bitValue = false;
         if (!markLong && spaceLong) {
            bitValue = false;  // Bit 0
         } else if (markLong && !spaceLong) {
            bitValue = true;   // Bit 1
         } else {
            // Invalid combination - reset
            rxState.gotSync = false;
            rxState.lastTime = time;
            return;
         }

         if (bitValue) {
            rxState.isrData[rxState.byte] |= (1 << rxState.bit);
         }

         // Check if we've received 84 bits (10 bytes + 4 bits)
         if (rxState.byte == 10 && rxState.bit == 3) {
            rxRawByte8 = rxState.isrData[8];
            for (int i = 0; i < 10; i++) {
               rxData[i] = rxState.isrData[i];
            }
            rxData[10] = rxState.isrData[10] & 0x0F;
            rxGotData = true;
            rxState.gotSync = false;
            rxState.lastTime = time;
            return;
         }

         rxState.bit++;
         if (rxState.bit >= 8) {
            rxState.bit = 0;
            rxState.byte++;
            if (rxState.byte >= sizeof(rxState.isrData)) {
               rxState.gotSync = false;
               rxState.lastTime = time;
               return;
            }
         }

         // Reset for next bit
         rxState.waitingForSpace = false;
         rxState.markDuration = 0;
         rxState.spaceDuration = 0;
      }
   }
   rxState.lastTime = time;
}

IRAM_ATTR
void Ir::txTick(void)
{
   if (txBitCount == 0) {
      return;  // TX not active
   }

   // Decrement pulse counters
   if (txState.pulseLowCounter > 0) {
      if (--txState.pulseLowCounter == 0) {
         // Mark (LOW) finished - switch to space (HIGH)
         digitalWrite(txPin, HIGH);
      }
   } else if (txState.pulseHighCounter > 0) {
      if (--txState.pulseHighCounter == 0) {
         // Space (HIGH) finished - pulse pair complete
      }
   }

   // Setup next pulse when current one is complete
   if (txState.pulseHighCounter + txState.pulseLowCounter == 0)
   {
      bool syncJustCompleted = txState.syncBit;

      if (txState.syncBit) {
         // Sync pulse just completed - clear flag and set up first data bit
         txState.syncBit = false;
      }

      if (syncJustCompleted) {
         // Sync just completed - set up first data bit (txState.curBit is 0)
         uint8_t byte = txState.curBit / 8;
         uint8_t bit = txState.curBit % 8;
         bool bitValue = txData[byte] & (1 << bit);
         if (bitValue) {
            // Bit 1: long mark + short space
            txState.pulseLowCounter = 11;
            txState.pulseHighCounter = 4;
         } else {
            // Bit 0: short mark + long space
            txState.pulseLowCounter = 4;
            txState.pulseHighCounter = 11;
         }
         digitalWrite(txPin, LOW);
         txState.curBit++;
      } else if (txState.curBit == 0 && txBitCount > 0 && !txState.syncBit) {
         // First call: setup sync pulse (before any data bits)
         // Sync: LOW (mark) for ~5341us, then HIGH (space) for ~1126us
         txState.pulseLowCounter = 49;   // 49 * 109us = 5341us (sync mark)
         txState.pulseHighCounter = 10;  // 10 * 109us = 1090us ≈ 1126us (sync space)
         txState.syncBit = true;
         digitalWrite(txPin, LOW);
      } else if (txState.curBit < txBitCount) {
         // Setup data bit
         uint8_t byte = txState.curBit / 8;
         uint8_t bit = txState.curBit % 8;
         bool bitValue = txData[byte] & (1 << bit);
         if (bitValue) {
            // Bit 1: long mark + short space
            txState.pulseLowCounter = 11;
            txState.pulseHighCounter = 4;
         } else {
            // Bit 0: short mark + long space
            txState.pulseLowCounter = 4;
            txState.pulseHighCounter = 11;
         }
         digitalWrite(txPin, LOW);
         txState.curBit++;
      } else {
         // All bits done (sync + 84 data bits) - return to idle (HIGH)
         txBitCount = 0;
         memset(&txState, 0, sizeof(txState));
         digitalWrite(txPin, HIGH);
         return;
      }
   }
}

Ir::Ir(uint8_t txPin, uint8_t rxPin) :
   txPin(txPin),
   rxPin(rxPin)
{
}

void Ir::init(ir_command_t irCmdCb)
{
   this->irCmdCb = irCmdCb;
   pInstance = this;

   pinMode(rxPin, INPUT_PULLUP);
   pinMode(txPin, OUTPUT);
   digitalWrite(txPin, HIGH);

   pIrTxTimer = timerBegin(0, 80, true);  // timer 0, 80 prescaler → 1 tick = 1µs
   timerAttachInterrupt(pIrTxTimer, &timerIsr, true);
   timerAlarmWrite(pIrTxTimer, 109, true);  // trigger each 109µs
   timerAlarmEnable(pIrTxTimer);

   attachInterrupt(digitalPinToInterrupt(rxPin), &Ir::gpioIsr, CHANGE);
}

void Ir::handle(void)
{
   if (!rxGotData) {
      return;
   }
   rxGotData = false;

#if DUMP_RX_IR_DATA
   Log.printf("IR: ");
   for (int i = 0; i < 11; i++) {
      Log.printf("%02x", rxData[i]);
   }
   Log.printf("\r\n");
#endif

   if (!verifyIrChecksum(rxData)) {
      Log.printf("Invalid IR chksum\r\n");
      return;
   }

   // Extract fields
   uint8_t modeCode = rxData[0] >> 4;
   uint8_t powerBit = (rxData[0] >> 3) & 0x01;  // Bit 3 of byte 0 = power (1=ON, 0=OFF)
   op_mode_t mode = fromIrMode(modeCode);

   // If power bit is 0, override mode to OFF
   if (powerBit == 0) {
      mode = OP_MODE_OFF;
   }

   uint8_t fan = rxData[1] & 0x0F;
   uint8_t temp = (rxData[2] >> 4) * 10 + (rxData[2] & 0x0F);
   uint8_t swingBits = (rxRawByte8 >> 1) & 0x3;
   bool swing = (swingBits == 0);

   this->irCmdCb(mode, fan, swing, temp);
}

void Ir::sendCommand(op_mode_t mode, uint8_t fan, bool swing, uint8_t temp)
{
   // Base data from working CSV - only bytes we DON'T set in sendCommand
   // We set: bytes 0 (mode), 1 (fan), 2 (temp), 8 (swing) - these are 0 in initData
   // We preserve: bytes 3-7, byte 9 lower nibble (0x1), byte 10 (checksum will be recalculated)
   static const uint8_t initData[11] = {
      0x00, 0x00, 0x00, 0xC0, 0x00, 0x88, 0x0A, 0x84, 0x00, 0x01, 0x00
   };
   if (txBitCount != 0) {
      return; // TX already in progress
   }
   // Encode temperature: upper nibble = tens, lower nibble = ones
   uint8_t hexTemp = ((temp / 10) << 4) | (temp % 10);
   memcpy((void *)&txData[0], &initData[0], sizeof(txData));
   // Set mode code in upper 4 bits of byte 0
   // Set power bit (bit 3) - 1=ON, 0=OFF
   uint8_t byte0 = (toIrModeCode(mode) << 4);
   if (mode != OP_MODE_OFF) {
      byte0 |= 0x08;  // Set bit 3 for ON
   }
   txData[0] = byte0;
   // Set fan code in lower 4 bits of byte 1
   txData[1] = fan & 0x0F;
   // Set temperature in byte 2
   txData[2] = hexTemp;
   // Set swing in byte 8, bits [2:1]: 0 = enabled, 3 = disabled
   if (swing) {
      // Swing enabled: bits [2:1] = 0, set bit 7
      txData[8] = 0x80;  // 0b10000000
   } else {
      // Swing disabled: set bits [2:1] = 3, set bits 7, 6
      txData[8] = 0xC6;  // 0b11000110
   }
   // Calculate and set checksum
   // Checksum calculation includes bytes 0-8 and lower 4 bits of byte 9
   int16_t chkSum = calcIrChecksum((uint8_t *)&txData[0]);
   // Set checksum: upper 4 bits of byte 9 = lower 4 bits of checksum
   //                lower 4 bits of byte 10 = upper 4 bits of checksum
   txData[9] = (txData[9] & 0x0F) | ((chkSum & 0x0F) << 4);
   txData[10] = (chkSum >> 4) & 0x0F;

#if DUMP_TX_IR_DATA
   Log.printf("TXIR: ");
   for (int i = 0; i < 11; i++) {
      Log.printf("%02x", txData[i]);
   }
   Log.printf("\r\n");
#endif

   txBitCount = 85;  // 85 total: sync + 84 data bits
   memset(&txState, 0, sizeof(txState));
}
