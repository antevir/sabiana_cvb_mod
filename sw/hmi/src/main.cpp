#include <lvgl.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_sntp.h>
#include <TelnetSerialStream.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <ArtronShop_SHT3x.h>

#include "main_screen.hpp"
#include "wifi_screen.hpp"
#include "settings.hpp"
#include "uart_com.hpp"
#include "time_program_screen.hpp"
#include "fan_cycle.hpp"

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

/*Set to your screen resolution*/
#define TFT_HOR_RES   240
#define TFT_VER_RES   320

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

#define I2C_SDA_GPIO 27
#define I2C_SCL_GPIO 22

#define AIR_TEMP_COMP (-0.7f)  // Heat dissipation compensating

#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3" // Stockholm

#define TEMP_UPDATE_INTERVAL_MS 10000
#define BACKLIGHT_TMO_MS        5000

static void uartCtrlDataCb(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp);
static void uartCtrlDataTmoCb(void);
static void uartStartupCb(void);

typedef struct {
   bool active;
   int lastWakeTime;
} Backlight_t;

static const uart_com_callbacks_t uartComCallbacks = {
   .startup = uartStartupCb,
   .ctrlData = uartCtrlDataCb,
   .ctrlDataTmo = uartCtrlDataTmoCb,
   .wifiCfg = NULL
};

SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240,touchScreenMaximumY = 3800;

ArtronShop_SHT3x sht3x(0x44, &Wire);

bool gSendState = true;
uint32_t gSettingModTimeMs = 0;
MainScreen& gMainScreen = MainScreen::getInstance();
WifiSetupScreen& gWifiScreen = WifiSetupScreen::getInstance();
TimeProgramUI& gTpUi = TimeProgramUI::getInstance();
FanCycle gFanCycle;
Settings& gSettings = Settings::getInstance();
UartCom fanCom(Serial, &uartComCallbacks);
TelnetSerialStream telnetSerialStream = TelnetSerialStream();
Backlight_t backlight = { 0 };
lv_indev_t *indev; //Touchscreen input device
uint8_t *drawBuf;  //drawBuf is allocated on heap otherwise the static area is too big on ESP32 at compile
uint32_t lastTick = 0;  //Used to track the tick timer


static void backlightWake()
{
   ledcWrite(1, 250);
   backlight.lastWakeTime = millis();
   backlight.active = true;
}

static void handleBacklight()
{
   if (backlight.active) {
      if (millis() - backlight.lastWakeTime > BACKLIGHT_TMO_MS) {
         ledcWrite(1, 50);
         backlight.active = false;
      }
   }
}

static void touchpadRead( lv_indev_t * indev, lv_indev_data_t * data )
{
   if(touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      //Some very basic auto calibration so it doesn't go out of range
      if(p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
      if(p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
      if(p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
      if(p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;
      //Map this to the pixel position
      data->point.x = map(p.x,touchScreenMinimumX,touchScreenMaximumX,1,TFT_HOR_RES); /* Touchscreen X calibration */
      data->point.y = map(p.y,touchScreenMinimumY,touchScreenMaximumY,1,TFT_VER_RES); /* Touchscreen Y calibration */
      data->state = LV_INDEV_STATE_PRESSED;

      backlightWake();
   } else {
      data->state = LV_INDEV_STATE_RELEASED;
   }
}

static void uartCtrlDataCb(uint8_t type, float temp, uint8_t mode, uint8_t fanLevel, uint8_t setTemp)
{
   gMainScreen.setHWTemp(temp);
   if (type == PKT_TYPE_IR_CTRL) {
      gSettings.setMode(mode);
      gSettings.setFanLevel(fanLevel);
      gSettings.setSetTemp(setTemp);
   }
}

static void uartCtrlDataTmoCb(void)
{
   gMainScreen.setHWTemp(NAN);
}

static void uartStartupCb(void)
{
   String ssid = gSettings.getSsid();
   String psk = gSettings.getPsk();
   fanCom.sendWifiConfig(ssid, psk);
}

void timeofdayCb(struct timeval *t)
{
   time_t now;
   now = time(nullptr);      // Updates the 'now' variable to the current time value
   
   // human readable
   Serial.print("ctime:     ");
   Serial.print(ctime(&now));
   // Here is one example showing how you can get the current month
   Serial.print("current month: ");
   Serial.println(localtime(&now)->tm_mon);
   // Here is another example showing how you can get the current year
   Serial.print("current year: ");
   Serial.println(localtime(&now)->tm_year);
   // Look in the printTM method to see other data that is available
   Serial.println();
}

static void wifiPressed(void)
{
   gWifiScreen.show();
}

static void programPressed(void)
{
   gTpUi.showMenu();
}

static void wifiSetupDoneCb(bool store)
{
   if (store) {
      String ssid = gSettings.getSsid();
      String psk = gSettings.getPsk();
      WiFi.begin(ssid, psk);
      fanCom.sendWifiConfig(ssid, psk);
   }
   gMainScreen.show();
}

static void tpCloseCb(void)
{
   gMainScreen.show();
}

static void manualSettingChangedCb(void)
{
   gTpUi.notifyManualOverride();
}

static void settingModified(void)
{
   gSettingModTimeMs = millis();
   gSendState = true;
   gMainScreen.reloadSettings();
}

void setup()
{
   gSettings.init(settingModified);
   
   Serial.begin(115200);
   Serial.println("Starting fan HMI");
   
   Serial1.begin(115200);
   
   WiFi.begin(gSettings.getSsid(), gSettings.getPsk());
   WiFi.setSleep(true);
   
   ArduinoOTA
   .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
         type = "sketch";
      } else {
         type = "filesystem";
      }
      
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
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
   Log.disableSerial(true);
   Log.begin();
   MDNS.begin("FanHmi");
   
   timeofdayCb(NULL);
   sntp_set_time_sync_notification_cb(timeofdayCb);
   configTzTime(TIME_ZONE, "pool.ntp.org", "time.nist.gov");
   
   
   //Initialise the touchscreen
   touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
   touchscreen.begin(touchscreenSpi); /* Touchscreen init */
   touchscreen.setRotation(2); /* Inverted landscape orientation to match screen */
   
   //Initialise LVGL
   lv_init();
   drawBuf = new uint8_t[DRAW_BUF_SIZE];
   lv_display_t * disp;
   disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, drawBuf, DRAW_BUF_SIZE);
   lv_display_set_rotation(disp,LV_DISPLAY_ROTATION_90);
   
   //Initialize the XPT2046 input device driver
   indev = lv_indev_create();
   lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);  
   lv_indev_set_read_cb(indev, touchpadRead);

   // Use backlight PWM
   ledcAttachPin(TFT_BL, 1);
   ledcSetup(1, 25000, 8); // 25 kHz, 8-bit
   backlightWake();

   Wire.begin(I2C_SDA_GPIO, I2C_SCL_GPIO);
   sht3x.begin();

   gMainScreen.init(&gSettings, wifiPressed, programPressed, manualSettingChangedCb);
   gWifiScreen.init(&gSettings, wifiSetupDoneCb);
   gTpUi.init(tpCloseCb, &gSettings);
   gFanCycle.init(&gSettings);
   gMainScreen.show();
   
   fanCom.init();

   // Lower CPU speed to minimize heat dissipation
   setCpuFrequencyMhz(80);

   //Done
   Serial.println( "Setup done" );
}

void loop()
{   
   static uint32_t lastTick = 0;
   static uint32_t lastTempTimeMs = 0;
   static float airTemp = NAN;
   
   lv_tick_inc(millis() - lastTick);
   lastTick = millis();
   lv_timer_handler();
   delay(10);
   
   if (millis() - lastTempTimeMs >= TEMP_UPDATE_INTERVAL_MS) {
      sht3x.measure();
      lastTempTimeMs = millis();
      airTemp = sht3x.temperature() + AIR_TEMP_COMP;
      gMainScreen.setAirTemp(airTemp);
      gSendState = true;
   }
   if ((gSettingModTimeMs != 0) && 
       ((millis() - gSettingModTimeMs) > 10000)) {
      gSettings.store();
      gSettingModTimeMs = 0;
   }
   gTpUi.tick();
   if (gFanCycle.tick(gTpUi.isActivePeriod(), gSettings.getMode(), gSettings.getFanLevel(),
                      airTemp, gSettings.getSetTemp())) {
      gSendState = true;
   }
   if (gSendState) {
      gSendState = false;
      int mode = gSettings.getMode();
      int fanLevel = gSettings.getFanLevel();
      if (gFanCycle.overrideActive()) {
         mode = gFanCycle.effectiveMode();
         fanLevel = gFanCycle.effectiveFan();
      }
      fanCom.sendState(PKT_TYPE_HMI_STATE, airTemp, mode, fanLevel, gSettings.getSetTemp());
   }
   
   fanCom.handleRx();
   ArduinoOTA.handle();
   gMainScreen.update();
   handleBacklight();
}
