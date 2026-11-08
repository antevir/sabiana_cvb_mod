#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebSerialStream.h>
#include <TelnetSerialStream.h>
#include <ArduinoOTA.h>

#include "ir.hpp"
#include "leds.hpp"
#include "modbus.hpp"
#include "uart_com.hpp"
#include "settings.hpp"
#include "fan_cycle.hpp"
#include "wifi_link.hpp"

#define UDP_PORT 15007

#define IR_RX_GPIO         10
#define IR_TX_GPIO         1
#define MODBUS_RX_GPIO     7
#define MODBUS_TX_GPIO     6
#define ONEWIRE_GPIO       5
#define WINDOW_OPEN_GPIO   0
#define RED_LED_IN_GPIO    3
#define GREEN_LED_IN_GPIO  2
#define RED_LED_OUT_GPIO   8
#define GREEN_LED_OUT_GPIO 9
#define HMI_RX_GPIO        20
#define HMI_TX_GPIO        21


#define TEMP_INTERVAL_MS 2000

#define PRINT_TEMP 1
#define PRINT_STATE 0

static void uartCtrlDataCb(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp);
static void uartCtrlDataTmoCb(void);
static void uartStartupCb(void);
static void uartWifiCfgCb(String ssid, String psk);
static void uartFanCycleCfgCb(bool enabled, bool activePeriod, uint8_t fanLevel,
                              uint16_t periodMinutes, uint16_t durationMinutes);

static const uart_com_callbacks_t uartComCallbacks = {
   .startup = NULL,
   .ctrlData = uartCtrlDataCb,
   .ctrlDataTmo = uartCtrlDataTmoCb,
   .wifiCfg = uartWifiCfgCb,
   .fanCycleCfg = uartFanCycleCfgCb
};

typedef struct {
   op_mode_t mode;
   fan_level_t fanLevel;
   uint8_t setTemp;
   float airTemp;
   float hwTemp;
   bool gotHwSensor;
   bool gotHw;
   bool error;
} ctrl_state_t;


Leds leds(GREEN_LED_OUT_GPIO, RED_LED_OUT_GPIO);
Ir ir(IR_TX_GPIO, IR_RX_GPIO);
OneWire oneWire;
DallasTemperature tempSensor(&oneWire);
WebSerialStream webSerialStream = WebSerialStream();
TelnetSerialStream telnetSerialStream = TelnetSerialStream();

Settings& gSettings = Settings::getInstance();
UartCom hmiCom(Serial, &uartComCallbacks);
Modbus modbus(Serial1);
FanCycle fanCycle;
WiFiUDP udp;
IPAddress multicastAddr = IPAddress(224,3,29,71);
bool gProcessState = true;

static ctrl_state_t gCtrlState = {
   .mode = OP_MODE_OFF,
   .fanLevel = FAN_LEVEL_1,
   .setTemp = 21,
   .airTemp = NAN,
   .hwTemp = NAN,
   .gotHwSensor = false,
   .gotHw = false,
   .error = false
};

static fan_level_t convertFanLevel(uint8_t rawFanLevel)
{
   // HMI/IR protocol: 0=auto, 1=level1, 2=level2, 3=level3
   // Enum: AUTO=1, 1=2, 2=3, 3=4
   switch(rawFanLevel)
   {
      case 0: return FAN_LEVEL_AUTO;
      case 1: return FAN_LEVEL_1;
      case 2: return FAN_LEVEL_2;
      case 3: return FAN_LEVEL_3;
      default: return FAN_LEVEL_AUTO;
   }
}

static void uartCtrlDataCb(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp)
{
   gCtrlState.airTemp = temp;
   gCtrlState.mode = (op_mode_t)mode;
   gCtrlState.fanLevel = convertFanLevel(fanLevel);
   gCtrlState.setTemp = setTemp;
   gProcessState = true;
}

static void uartCtrlDataTmoCb(void)
{
   gCtrlState.airTemp = NAN;
   fanCycle.disable();
   gProcessState = true;
}

static void uartWifiCfgCb(String ssid, String psk)
{
   gSettings.setSsid(ssid);
   gSettings.setPsk(psk);
   WifiLink::reconnect(ssid, psk);
}

static void uartFanCycleCfgCb(bool enabled, bool activePeriod, uint8_t fanLevel,
                              uint16_t periodMinutes, uint16_t durationMinutes)
{
   fanCycle.configure(enabled, activePeriod, fanLevel, periodMinutes, durationMinutes);
   gProcessState = true;
}

void processTemp(void)
{
   static unsigned long lastTempTime = 0;
   if (millis() - lastTempTime >= TEMP_INTERVAL_MS) {
      tempSensor.requestTemperatures(); 
      float temperatureC = tempSensor.getTempCByIndex(0);
#if PRINT_TEMP
      Log.printf("temp: %f\r\n", temperatureC);
#endif
      if ((temperatureC > 10.0f) && (temperatureC < 80.0f)) {
         gCtrlState.hwTemp = temperatureC;
      } else {
         gCtrlState.hwTemp = NAN;
      }
      lastTempTime = millis();
   }
}


void sendUdpTemp(uint16_t hwTemp, bool gotHw)
{
   char buf[64];
   if (!udp.beginMulticastPacket()) {
      Log.println("Failed to start multicast packet!");
      return;
   }
   sprintf(buf, "HWT:%d,HW:%d", hwTemp, gotHw);
   udp.write((uint8_t *)&buf[0], strlen(buf));
   udp.endPacket();
}

void checkUdpTemp(void)
{
   static int lastUdpTempMs = 0;
   char buf[64];
   if (udp.available() && udp.parsePacket()) {
      int hwt, hw;
      int bytes = udp.read(&buf[0], sizeof(buf) - 1);
      buf[bytes] = 0;
      if (sscanf(buf, "HWT:%d,HW:%d", &hwt, &hw) == 2) {
         gCtrlState.hwTemp = (float)hwt / 10.0f;
         gCtrlState.gotHw = hw > 0;
         lastUdpTempMs = millis();
      }
   } else if (millis() - lastUdpTempMs >= (10 * 1000)) {
      // If we lost contact we set gGotHotWater=true and let fan control as normal
      gCtrlState.hwTemp = NAN;
      gCtrlState.gotHw = true;
   }
}

static void irRxCommandCb(op_mode_t mode, uint8_t fan, bool swing, uint8_t temp)
{
   leds.blink();
   hmiCom.sendState(PKT_TYPE_IR_CTRL, gCtrlState.hwTemp, mode, fan, temp);
}

static void sendModbusFrame(op_mode_t mode)
{
   modbus.sendFrame(mode, gCtrlState.fanLevel, gCtrlState.setTemp, gCtrlState.airTemp);
}

static void sendFanCycleFrame()
{
   modbus.sendFrame(OP_MODE_FAN, convertFanLevel(fanCycle.getFanLevel()),
                    gCtrlState.setTemp, gCtrlState.airTemp);
}

static void processHeatMode()
{
   static bool heat = false;
   
   if (isnan(gCtrlState.airTemp) || isnan(gCtrlState.hwTemp)) {
      gCtrlState.error = true;
      leds.set(LED_STATE_RED);
      fanCycle.stop();
      sendModbusFrame(OP_MODE_OFF);
      return;
   }
   gCtrlState.error = false;
   
   leds.set(LED_STATE_GREEN);
   
   if (gCtrlState.airTemp < (float)gCtrlState.setTemp - 0.5f) {
      if (gCtrlState.gotHw) {
         heat = true;
      } else {
         leds.set(LED_STATE_GREEN_RED);
         heat = false;
      }
   }
   if (gCtrlState.airTemp > (float)gCtrlState.setTemp + 0.5f) {
      heat = false;
   }
   
   if (heat) {
      fanCycle.tick(true, false, millis());
      sendModbusFrame(OP_MODE_HEAT);
   } else if (fanCycle.tick(false, false, millis())) {
      sendFanCycleFrame();
   } else {
      sendModbusFrame(OP_MODE_OFF);
   }
}

static void processFanMode()
{
   fanCycle.tick(false, true, millis());
   gCtrlState.error = false;
   if (isnan(gCtrlState.airTemp)) {
      leds.set(LED_STATE_GREEN_RED);
   } else {
      leds.set(LED_STATE_GREEN);
   }
   sendModbusFrame(OP_MODE_FAN);
}

void setup() {
   // Power optimizations
   setCpuFrequencyMhz(80);  // Reduce from 240MHz to 80MHz
   btStop();  // Ensure Bluetooth is disabled - saves power

   gSettings.init();
   
#if ARDUINO_USB_CDC_ON_BOOT
   Serial.begin(115200);
#else
   Log.disableSerial(true);
   Serial.begin(115200, SERIAL_8N1, HMI_RX_GPIO, HMI_TX_GPIO);
#endif
   
   Serial1.begin(9600, SERIAL_8N1, MODBUS_RX_GPIO, MODBUS_TX_GPIO);
   
   pinMode(ONEWIRE_GPIO, INPUT_PULLDOWN);
   if (digitalRead(ONEWIRE_GPIO) == HIGH) {
      gCtrlState.gotHwSensor = true;
      oneWire.begin(ONEWIRE_GPIO);
      tempSensor.begin();
   }
   
   pinMode(WINDOW_OPEN_GPIO, OUTPUT);
   pinMode(RED_LED_IN_GPIO, INPUT_PULLUP);
   pinMode(GREEN_LED_IN_GPIO, INPUT_PULLUP);
   
   leds.init();
   ir.init(irRxCommandCb);
   
   WifiLink::start(gSettings.getSsid(), gSettings.getPsk());

   ArduinoOTA.setHostname("FanCtrl");
   ArduinoOTA
   .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
         type = "sketch";
      } else {
         type = "filesystem";
      }
      
      Serial.println("Start updating " + type);
   })
   .onEnd([]() {
      Serial.println("\nEnd");
   })
   .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
   })
   .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
         Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
         Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
         Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
         Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
         Serial.println("End Failed");
      }
   });
   
   ArduinoOTA.begin();
   
   Log.addPrintStream(std::make_shared<TelnetSerialStream>(telnetSerialStream));
   Log.begin();
   
   hmiCom.init();
}


void loop() {
   static unsigned long lastUpdateTime;
   
   if (gCtrlState.gotHwSensor) {
      processTemp();
   }
   
   WifiLink::loop();
   if (WifiLink::takeConnected()) {
      ArduinoOTA.end();
      ArduinoOTA.begin();
      udp.stop();
      udp.beginMulticast(multicastAddr, UDP_PORT);
   }

   modbus.handle();
   ir.handle();
   leds.handle();
   ArduinoOTA.handle();
   hmiCom.handleRx();
   Log.loop();
   
   if (millis() - lastUpdateTime >= 3000) {
      // Each 3 second
      lastUpdateTime = millis();
      
      if (gCtrlState.gotHwSensor) {
         // We got a hot water sensor - broadcast temp to all slaves
         if (!isnan(gCtrlState.hwTemp)) {
            gCtrlState.gotHw = gCtrlState.hwTemp >= 30.0; // 30.0 C
         } else {
            // Temp sensor error set gGotHotWater=true and let fan control as normal
            gCtrlState.gotHw = true;
         }
         sendUdpTemp((uint16_t)(gCtrlState.hwTemp * 10.0f), gCtrlState.gotHw);
      } else {
         // No sensor - listen to master
         checkUdpTemp();
      }
#if PRINT_TEMP
      Log.printf("HWT: %.02f, GotHW: %d\r\n", gCtrlState.hwTemp, gCtrlState.gotHw);
#endif
      
      gProcessState = true;
      hmiCom.sendState(PKT_TYPE_FAN_STATE, gCtrlState.hwTemp, gCtrlState.mode, gCtrlState.fanLevel, gCtrlState.setTemp);
   }
   
   if (gProcessState) {
      gProcessState = false;
      switch (gCtrlState.mode) {
         case OP_MODE_OFF:
            if (fanCycle.tick(false, false, millis())) {
               leds.set(LED_STATE_GREEN);
               sendFanCycleFrame();
            } else {
               leds.set(LED_STATE_OFF);
               sendModbusFrame(OP_MODE_OFF);
            }
            break;
         case OP_MODE_HEAT:
            processHeatMode();
            break;
         case OP_MODE_FAN:
            processFanMode();
            break;
         default:
            leds.set(LED_STATE_RED);
            sendModbusFrame(OP_MODE_OFF);
            break;
      }
   }

   delay(10);
}
