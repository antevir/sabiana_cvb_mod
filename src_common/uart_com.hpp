#include <TLog.h>
#include "Arduino.h"

#define PKT_TYPE_STARTUP    1
#define PKT_TYPE_WIFI_CFG   2
#define PKT_TYPE_HMI_STATE  3
#define PKT_TYPE_FAN_STATE  4
#define PKT_TYPE_IR_CTRL    5

#define CTRL_DATA_TMO       10000

typedef struct {
   void (*startup)(void);
   void (*ctrlData)(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp);
   void (*ctrlDataTmo)(void);
   void (*wifiCfg)(String ssid, String psk);
} uart_com_callbacks_t;

class UartCom {
   private:
      static const uint8_t START_IDX = 0;
      static const uint8_t TYPE_IDX = 1;
      static const uint8_t LENGTH_IDX = 2;
      static const uint8_t PAYLOAD_IDX = 3;
      
      static const uint8_t START_BYTE = 0xC0;
      static const size_t MAX_PAYLOAD_LEN = 128;
      
      uint8_t rxData[4 + MAX_PAYLOAD_LEN]; // START_BYTE, TYPE, LENGTH, PAYLOAD, CHKSUM
      size_t rxLen = 0;
      HardwareSerial &serial;
      const uart_com_callbacks_t *pCallbacks;
      unsigned long lastCtrlDataTime = 0;
      
      uint8_t calcSum(uint8_t *data, size_t len) {
         uint8_t sum = 0;
         for (int i = 0; i < len; i++) {
            sum += data[i];
         }
         return sum;
      }
      
      void parseStartup(uint8_t *payload, size_t len) {
         if (len != 0) {
            Log.printf("Invalid startup\n");
            return;
         }
         if (pCallbacks->startup) {
            pCallbacks->startup();
         }
      }
      
      void parseWifiCfg(uint8_t *payload, size_t len) {
         if (strnlen((char *)payload, len) == len) {
            Log.printf("Invalid wifi cfg\n");
            return;
         }
         String ssid((char *)&payload[0]);
         String psk((char *)&payload[ssid.length() + 1], len - ssid.length() - 1);
         if (pCallbacks->wifiCfg) {
            pCallbacks->wifiCfg(ssid, psk);
         }
      }
      
      void parseCtrlData(uint8_t type, uint8_t *payload, size_t len) {
         if (len != 5) {
            Log.printf("Invalid state length\n");
            return;
         }
         uint16_t temp16 = payload[0] | (payload[1] << 8);
         float temp = (float)temp16 / 10.0f;
         Log.printf("UART ctrl data(%0.1f, %d)\n", temp, payload[2]);
         lastCtrlDataTime = millis();
         if (pCallbacks->ctrlData) {
            pCallbacks->ctrlData(type, temp, payload[2], payload[3], payload[4]);
         }
      }
      
      void parsePacket(uint8_t *data, size_t len) {
         switch (data[TYPE_IDX]) {
            case PKT_TYPE_STARTUP:
               parseStartup(&data[PAYLOAD_IDX], data[LENGTH_IDX]);
               break;
            case PKT_TYPE_WIFI_CFG:
               parseWifiCfg(&data[PAYLOAD_IDX], data[LENGTH_IDX]);
               break;
            case PKT_TYPE_FAN_STATE:
            case PKT_TYPE_HMI_STATE:
            case PKT_TYPE_IR_CTRL:
               parseCtrlData(data[TYPE_IDX], &data[PAYLOAD_IDX], data[LENGTH_IDX]);
               break;
            default:
               Log.printf("Received unknown packet type: %x\n", data[TYPE_IDX]);
               break;
         }
      }
      
      void handleByte(uint8_t byte) {
         if (rxLen == START_IDX) {
            if (byte != START_BYTE) {
               return;
            }
         } else if (rxLen == LENGTH_IDX) {
            if (byte > MAX_PAYLOAD_LEN) {
               rxLen = 0;
               return;
            }
         } else if (rxLen >= sizeof(rxData) - 1) {
            rxLen = 0;
            return;
         }
         rxData[rxLen++] = byte;
         if ((rxLen >= 4) && (rxLen == 4 + rxData[LENGTH_IDX])) {
            // This should be the last byte (checksum) of the packet
            uint8_t chkSum = calcSum(&rxData[0], rxLen - 1);
            if (chkSum == rxData[rxLen - 1]) {
               parsePacket(rxData, rxLen);
            } else {
               Log.printf("Received corrupt packet\n");
            }
            rxLen = 0;
         }
      }

   public:
      UartCom(HardwareSerial &serial, const uart_com_callbacks_t *pCallbacks) : 
         serial(serial)
      {
         this->pCallbacks = pCallbacks;
      }
      
      void init()
      {
         sendPacket(PKT_TYPE_STARTUP, NULL, 0);
      }
      
      void handleRx() {
         while (serial.available()) {
            uint8_t byte = (uint8_t)serial.read();
            handleByte(byte);
         }
         if ((lastCtrlDataTime != 0) &&
             (millis() - lastCtrlDataTime > CTRL_DATA_TMO))
         {
            lastCtrlDataTime = 0;
            if (pCallbacks->ctrlDataTmo) {
               pCallbacks->ctrlDataTmo();
            }
         }
      }
      
      void sendPacket(uint8_t type, uint8_t *payload, size_t payloadLen) {
         serial.write(START_BYTE);
         serial.write(type);
         serial.write(payloadLen);
         if (payloadLen > 0) {
            serial.write(payload, payloadLen);
         }
         uint8_t chkSum = START_BYTE + type + payloadLen + calcSum(payload, payloadLen);
         serial.write(chkSum);
      }
      
      void sendWifiConfig(String ssid, String psk) {
         uint8_t buffer[ssid.length() + 1 + psk.length()];
         memcpy(&buffer[0], ssid.c_str(), ssid.length() + 1);
         memcpy(&buffer[ssid.length() + 1], psk.c_str(), psk.length());
         sendPacket(PKT_TYPE_WIFI_CFG, &buffer[0], sizeof(buffer));
      }
      
      void sendState(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp) {
         uint16_t temp16 = isnan(temp) ? 0 : (uint16_t)(temp * 10);
         Serial.printf("temp: %f, isnan: %d, temp16: %d", temp, isnan(temp), temp16);
         uint8_t buffer[] = {
            (uint8_t)(temp16 & 0xff),
            (uint8_t)((temp16 >> 8) & 0xff), 
            mode,
            fanLevel,
            setTemp
         };
         sendPacket(type, &buffer[0], sizeof(buffer));
      }
};
