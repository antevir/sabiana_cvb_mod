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


#define TEMP_INTERVAL_MS 500

#define PRINT_TEMP 1
#define PRINT_STATE 0

static void uartCtrlDataCb(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp);
static void uartCtrlDataTmoCb(void);
static void uartStartupCb(void);
static void uartWifiCfgCb(String ssid, String psk);

static const uart_com_callbacks_t uartComCallbacks = {
   .startup = NULL,
   .ctrlData = uartCtrlDataCb,
   .ctrlDataTmo = uartCtrlDataTmoCb,
   .wifiCfg = uartWifiCfgCb
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

static void uartCtrlDataCb(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp)
{
   gCtrlState.airTemp = temp;
   gCtrlState.mode = (op_mode_t)mode;
   gCtrlState.fanLevel = (fan_level_t)fanLevel;
   gCtrlState.setTemp = setTemp;
   gProcessState = true;
}

static void uartCtrlDataTmoCb(void)
{
   gCtrlState.airTemp = NAN;
   gProcessState = true;
}

static void uartWifiCfgCb(String ssid, String psk)
{
   gSettings.setSsid(ssid);
   gSettings.setPsk(psk);
   WiFi.begin(ssid, psk);
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

static void processHeatMode()
{
   static bool heat = false;
   
   if (isnan(gCtrlState.airTemp) || isnan(gCtrlState.hwTemp)) {
      gCtrlState.error = true;
      leds.set(LED_STATE_RED);
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
      }
   }
   if (gCtrlState.airTemp > (float)gCtrlState.setTemp + 0.5f) {
      heat = false;
   }
   
   sendModbusFrame(heat ? OP_MODE_HEAT : OP_MODE_OFF);
}

static void processFanMode()
{
   gCtrlState.error = false;
   if (isnan(gCtrlState.airTemp)) {
      leds.set(LED_STATE_GREEN_RED);
   } else {
      leds.set(LED_STATE_GREEN);
   }
   sendModbusFrame(OP_MODE_FAN);
}

void setup() {
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
   
   WiFi.mode(WIFI_STA);
   
   String ssid = gSettings.getSsid();
   String psk = gSettings.getPsk();
   WiFi.begin(ssid, psk);
   /*if (WiFi.waitForConnectResult() != WL_CONNECTED) {
   Serial.println("WiFiConnection Failed");
   } else {
   Serial.println("WiFiConnection Succeeded");
   Serial.print("Local IP address: ");
   Serial.println(WiFi.localIP());
   }*/
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
   MDNS.begin("FanCtrl");
   
   udp.beginMulticast(multicastAddr, UDP_PORT);
   
   hmiCom.init();
}


void loop() {
   static unsigned long lastUpdateTime;
   
   if (gCtrlState.gotHwSensor) {
      processTemp();
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
            gCtrlState.gotHw = gCtrlState.hwTemp >= 300; // 30.0 C
         } else {
            // Temp sensor error set gGotHotWater=true and let fan control as normal
            gCtrlState.gotHw = true;
         }
         sendUdpTemp(gCtrlState.hwTemp, gCtrlState.gotHw);
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
            leds.set(LED_STATE_OFF);
            sendModbusFrame(OP_MODE_OFF);
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
}
