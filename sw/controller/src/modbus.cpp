#include <TLog.h>

#include "modbus.hpp"

#define MODBUS_FRAME_TMO_MS 500

#define MODBUS_ADDR_BYTE  0
#define MODBUS_FUN_BYTE  1
#define MODE_BYTE 21
#define AIR_TEMP_BYTE 22
#define SET_TEMP_BYTE 28
#define STATE_BYTE 110

#define STATE_FAN_AUTO_BIT  0
#define STATE_OFF           1  // Inverted if STATE_FAN_AUTO_BIT is set
#define STATE_FAN_LVL1_BIT  2
#define STATE_FAN_LVL2_BIT  3
#define STATE_FAN_LVL3_BIT  4
#define STATE_PROBING_MASK  0xE0

enum {
   MODBUS_MODE_OFF = 0x00,
   MODBUS_MODE_AUTO = 0x41,
   MODBUS_MODE_FAN = 0x03,
   MODBUS_MODE_HEAT = 0x51,
   MODBUS_MODE_COOL = 0x41,  // Same as AUTO, distinguished by STATE byte
   MODBUS_MODE_SWING_BIT = 0x80,  // Bit 7 indicates swing enabled
};

static uint16_t calcModbusCrc(uint8_t *pData, int len)
{
   uint16_t crc = 0xFFFF;
   
   for (int pos = 0; pos < len; pos++) {
      crc ^= (uint16_t)pData[pos];
      
      for (int i = 0; i < 8; i++) {
         if ((crc & 0x0001) != 0) {
            crc >>= 1;
            crc ^= 0xA001;
         } else {
            crc >>= 1;
         }
      }
   }
   // Note: Returned value has low and high bytes swapped
   return crc;  
}

static uint8_t getModeByte(op_mode_t mode)
{
   switch (mode)
   {
      case OP_MODE_OFF:
         return MODBUS_MODE_OFF;
      case OP_MODE_AUTO:
         return MODBUS_MODE_AUTO;
      case OP_MODE_FAN:
         return MODBUS_MODE_FAN;
      case OP_MODE_COOL:
         return MODBUS_MODE_COOL;
      case OP_MODE_HEAT:
         return MODBUS_MODE_HEAT;
   }
   return OP_MODE_OFF;
}

static uint8_t getStateByte(op_mode_t mode, fan_level_t level)
{
   if (mode == OP_MODE_OFF) {
      return 1 << STATE_OFF;
   }
   switch (level)
   {
      case FAN_LEVEL_AUTO:
         return 1 << STATE_FAN_AUTO_BIT;
      case FAN_LEVEL_1:
         return 1 << STATE_FAN_LVL1_BIT;
      case FAN_LEVEL_2:
         return 1 << STATE_FAN_LVL2_BIT;
      case FAN_LEVEL_3:
         return 1 << STATE_FAN_LVL3_BIT;
   }
   return 1 << STATE_OFF; 
}

void Modbus::handleFrame(uint8_t *pData, size_t length)
{
   if (length <= 2) {
      return;
   }
   uint16_t calcCrc = calcModbusCrc(pData, length - 2);
   uint16_t rxCrc = pData[length - 2] | (pData[length - 1] << 8);
   if (rxCrc != calcCrc) {
      // Invalid CRC - ignore frame
      Log.println("Invalid CRC");
      return;
   }
   if (pData[MODBUS_ADDR_BYTE] != 0 || pData[MODBUS_FUN_BYTE] != 0x45) {
      // Invalid address or function code
      Log.println("Invalid data");
      return;
   }
#if DUMP_MODBUS_FRAMES
   Log.printf("MODBUS: ");
   for (int i = 0; i < length; i++) {
      Log.printf("%02x", pData[i]);
   }
   Log.printf("\r\n");
#endif
   uint8_t modeByte = pData[MODE_BYTE];
   bool swing = (modeByte & MODBUS_MODE_SWING_BIT) != 0;
   uint8_t baseMode = modeByte & ~MODBUS_MODE_SWING_BIT;  // Strip swing bit
   uint16_t airTemp = pData[AIR_TEMP_BYTE] + (pData[AIR_TEMP_BYTE + 1] << 8);
   uint16_t setTemp = pData[SET_TEMP_BYTE] + (pData[SET_TEMP_BYTE + 1] << 8);
   uint8_t stateByte = pData[STATE_BYTE];
#if PRINT_STATE
   Log.printf("mode: %02x (base:%02x swing:%d), air: %d.%d, setTemp: %d, state: %02x\r\n",
              modeByte, baseMode, swing, gAirTemp / 10, gAirTemp % 10, gSetTemp / 10, stateByte);
#endif
   uint8_t fanLevel = FAN_LEVEL_OFF;
   if (stateByte & (1 << STATE_FAN_AUTO_BIT)) {
      fanLevel = (stateByte & (1 << STATE_OFF)) ? FAN_LEVEL_AUTO : FAN_LEVEL_OFF;
   } else if (stateByte & (1 << STATE_OFF)) {
      fanLevel = FAN_LEVEL_OFF;
   } else if (stateByte & (1 << STATE_FAN_LVL1_BIT)) {
      fanLevel = FAN_LEVEL_1;
   } else if (stateByte & (1 << STATE_FAN_LVL2_BIT)) {
      fanLevel = FAN_LEVEL_2;
   } else if (stateByte & (1 << STATE_FAN_LVL3_BIT)) {
      fanLevel = FAN_LEVEL_3;
   } else {
      Log.printf("Fan mode unknown!\r\n");
   }
   bool probing = false;
   if ((stateByte & STATE_PROBING_MASK) == STATE_PROBING_MASK) {
      probing = true;
   }
#if PRINT_STATE
   Log.printf("fan lvl: %d, probing: %d\r\n", fanLevel, probing);
#endif
   rxLastFrameTime = millis();
}

void Modbus::handle(void)
{
   if (serial.available()) {
      uint8_t byte = (uint8_t)serial.read();
      rxFrame[rxFramePos++] = byte;
      rxLastFrameTime = millis();
      if (rxFramePos == sizeof(rxFrame)) {
         Log.println("Frame buffer full - flushing");
         rxFramePos = 0;
      }
   } else if (rxFramePos > 0) {
      if (millis() - rxLastFrameTime > MODBUS_FRAME_TMO_MS) {
         handleFrame(rxFrame, rxFramePos);
         rxFramePos = 0;
      }
   }
}

void Modbus::sendFrame(op_mode_t mode, fan_level_t level, uint8_t setTemp, float airTemp, bool swing)
{
   // Frame template from actual captured data (1511_30_lvl1_heat_no.txt)
   // Bytes that get modified: 21 (mode), 22-23 (air temp), 28-29 (set temp), 110 (state)
   static const uint8_t sampleFrameData[] = {
      0x00,0x45,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xBE,0x00,0x2C,0x01,0x96,0x00,0x2C,0x01,
      0x28,0x00,0x54,0x01,0xDC,0x00,0x32,0x00,0x0A,0x00,0x3C,0x00,0xB4,0x00,0x07,0x00,0x2C,
      0x01,0x14,0x00,0x0F,0x00,0x0A,0x00,0x32,0x00,0x64,0x00,0x0A,0x00,0x64,0x00,0x23,0x00,
      0x23,0x00,0x1E,0x00,0x96,0x00,0x3C,0x00,0x03,0x00,0x02,0x00,0x64,0x00,0x2C,0x01,0x64,
      0x00,0x2C,0x01,0x00,0x00,0x0A,0x00,0x64,0x00,0x32,0x00,0x32,0x00,0x00,0x00,0x0F,0x00,
      0x07,0x00,0x78,0x00,0x0C,0x00,0x96,0x00,0x00,0x00
   };
   static uint8_t txFrame[sizeof(sampleFrameData) + 2];
   memcpy(&txFrame[0], &sampleFrameData[0], sizeof(sampleFrameData));
   uint16_t airTemp16 = (uint16_t)(airTemp * 10.0f);
   uint16_t setTemp16 = (uint16_t)setTemp * 10;

   // Set mode byte with swing bit if enabled
   uint8_t modeByte = getModeByte(mode);
   if (swing) {
      modeByte |= MODBUS_MODE_SWING_BIT;
   }

   txFrame[MODE_BYTE] = modeByte;
   txFrame[STATE_BYTE] = getStateByte(mode, level);
   txFrame[AIR_TEMP_BYTE] = airTemp16 & 0xFF;
   txFrame[AIR_TEMP_BYTE + 1] = (airTemp16 >> 8) & 0xFF;
   txFrame[SET_TEMP_BYTE] = setTemp16 & 0xFF;
   txFrame[SET_TEMP_BYTE + 1] = (setTemp16 >> 8) & 0xFF;
   uint16_t crc = calcModbusCrc(&txFrame[0], sizeof(sampleFrameData));
   txFrame[sizeof(sampleFrameData)] = crc & 0xFF;
   txFrame[sizeof(sampleFrameData) + 1] = (crc >> 8) & 0xFF;
   serial.write(&txFrame[0], sizeof(txFrame));
}