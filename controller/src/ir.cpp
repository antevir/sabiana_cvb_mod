#include <TLog.h>

#include "ir.hpp"

#define IR_SYNCH_US   5900
#define IR_ZERO_US    1200
#define IR_ONE_US     440
#define IR_TOLERANCE  200
#define IR_WITHIN(duration, time) \
   (((duration) < ((time) + IR_TOLERANCE)) && ((duration) >= ((time) - IR_TOLERANCE)))

#define IR_MODE_OFF    0x86
#define IR_MODE_AUTO   0x18
#define IR_MODE_FAN    0x58
#define IR_MODE_HEAT   0x78
#define IR_MODE_COOL   0x98

static hw_timer_t *pIrTxTimer = NULL;
static Ir *pInstance = NULL;

static uint16_t calcIrChecksum(const uint8_t *pData)
{
   // Calc 12 bit checksum
   int16_t chkSum = -0x200;
   for (int i = 0; i < 9; i++) {
      chkSum += pData[i];
   }
   chkSum += pData[9] & 0x0F;
   return chkSum;
}

static bool verifyIrChecksum(const uint8_t *pData)
{
   uint16_t calcChkSum = calcIrChecksum(pData);
   uint16_t rxChkSum = pData[9] >> 4;
   rxChkSum |= pData[10] << 4;
   return calcChkSum == rxChkSum;
}

static uint8_t toIrMode(op_mode_t mode)
{
   switch(mode)
   {
      case OP_MODE_OFF:  return IR_MODE_OFF;
      case OP_MODE_AUTO: return IR_MODE_AUTO;
      case OP_MODE_COOL: return IR_MODE_COOL;
      case OP_MODE_HEAT: return IR_MODE_HEAT;
      case OP_MODE_FAN:  return IR_MODE_FAN;
   }
   return IR_MODE_OFF;
}

static op_mode_t fromIrMode(uint8_t modeByte)
{
   switch(modeByte)
   {
      case IR_MODE_OFF:  return OP_MODE_OFF;
      case IR_MODE_AUTO: return OP_MODE_AUTO;
      case IR_MODE_COOL: return OP_MODE_COOL;
      case IR_MODE_HEAT: return OP_MODE_HEAT;
      case IR_MODE_FAN:  return OP_MODE_FAN;
   }
   return OP_MODE_OFF;
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
   uint32_t duration = time - rxLastTime;
   int pin = digitalRead(rxPin);
   
   if (pin == 1) {
      if (IR_WITHIN(duration, IR_SYNCH_US)) {
         rxBit = 0;
         rxByte = 0;
         memset(&rxIsrData[0], 0, sizeof(rxIsrData));
         rxGotSync = true;
      }
   } else if (rxGotSync) {
      bool gotBit = false;
      if (IR_WITHIN(duration, IR_ONE_US)) {
         rxIsrData[rxByte] |= (1 << rxBit);
         gotBit = true;
      } else if (IR_WITHIN(duration, IR_ZERO_US)) {
         gotBit = true;
      }
      
      if (gotBit) {
         if (++rxBit == 8) {
            rxBit = 0;
            if (++rxByte >= sizeof(rxIsrData)) {
               // Too many bits
               rxGotSync = false;
            }
         }
         if (rxByte == 10 && rxBit == 5) {
            memcpy(&rxData[0], &rxIsrData[0], 11);
            rxGotData = true;
            rxGotSync = false;
         }
      } else {
         // Corrupt bit
         Log.printf("Corrupt IR bit %d, byte %d\r\n", rxBit, rxByte);
         rxGotSync = false;
      }
   }
   rxLastTime = time;
}

IRAM_ATTR
void Ir::txTick(void)
{
   if (txCurBit == txBitCount) {
      return;
   }
   if (txPulseHighCounter + txPulseLowCounter == 0)
   {
      // Setup sync bit
      txPulseLowCounter = 49;
      txPulseHighCounter = 0;
      txSyncBit = 1;
      digitalWrite(txPin, LOW);
   }
   
   if (txPulseLowCounter > 0) {
      if ((--txPulseLowCounter == 0) && (txPulseHighCounter > 0)) {
         // set HIGH
         digitalWrite(txPin, HIGH);
      }
   } else if (txPulseHighCounter > 0) {
      if (--txPulseHighCounter == 0) {
         // set LOW
         digitalWrite(txPin, LOW);
      }
   }
   
   if (txPulseHighCounter + txPulseLowCounter == 0)
   {
      // Bit is done - setup next
      if (!txSyncBit) {
         txCurBit++;
      }
      txSyncBit = false;
      if (txCurBit < txBitCount) {
         uint8_t byte = txCurBit / 8;
         uint8_t bit = txCurBit % 8;
         bool bitValue = txData[byte] & (1 << bit);
         if (bitValue) {
            txPulseHighCounter = 4;
            txPulseLowCounter = 11;
         } else {
            txPulseHighCounter = 11;
            txPulseLowCounter = 4;       
         }
      } else {
         // All bits done
         txBitCount = 0;
         txCurBit = 0;
         digitalWrite(txPin, HIGH);
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
   
   // Mode: byte0
   op_mode_t mode = fromIrMode(rxData[0]);
   // Fan: bit9-10
   uint8_t fan = (rxData[1] >> 1) & 0x3;
   // Swing: bit 15
   bool swing = rxData[1] & 0x80;
   // Temp: bit 17-22
   uint8_t hexTemp = (rxData[2] >> 1) & 0x3F;
   // Temp is coded in 16 base, so 20 degrees = 0x20, 30 degrees = 0x30
   uint8_t temp = hexTemp & 0xF;
   temp += 10 * (hexTemp >> 4);
   Log.printf("mode: %d, fan: %d, swing: %d, temp: %d\r\n", mode, fan, swing, temp);
   this->irCmdCb(mode, fan, swing, temp);
   //txIrCommand(mode, fan, swing, temp);
}

void Ir::sendCommand(op_mode_t mode, uint8_t fan, bool swing, uint8_t temp)
{
   static const uint8_t initData[11] = {
      0x00, 0x60, 0x00, 0x06, 0x00, 0x22, 0xa0, 0x22, 0x06, 0x02, 0x00
      // 786240         06     00    22    a0      22  06c200
      // 20c lvl1 heat
   };
   if (txBitCount != 0) {
      return; // TX already in progress
   }
   uint8_t hexTemp = temp % 10;
   hexTemp += (temp / 10) << 4;
   memcpy((void *)&txData[0], &initData[0], sizeof(txData));
   txData[0] |= toIrMode(mode);
   txData[1] |= fan << 1;
   txData[1] |= swing ? 0x80 : 0;
   txData[2] |= hexTemp << 1;
   // Add 12 bit checksum
   int16_t chkSum = calcIrChecksum((uint8_t *)&txData[0]);
   txData[9] |= (chkSum & 0x0F) << 4;
   txData[10] = chkSum >> 4;
   
#if DUMP_TX_IR_DATA
   Log.printf("TXIR: ");
   for (int i = 0; i < 11; i++) {
      Log.printf("%02x", txData[i]);
   }
   Log.printf("\r\n");
#endif
   
   /*uint8_t test[11] = { 0x78, 0x62, 0x40, 0x06, 0x00, 0x22, 0xa0, 0x22, 0x06, 0xc2, 0x00};
   memcpy((void *)&txData[0], &test[0], sizeof(txData));*/
   txBitCount = 85;
}

