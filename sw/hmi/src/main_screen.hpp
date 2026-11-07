#pragma once

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_sntp.h"
#include "settings.hpp"

#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(amarath_40);
LV_IMAGE_DECLARE(img_blue_bg);
LV_IMAGE_DECLARE(img_fan);
LV_IMAGE_DECLARE(img_heat);
LV_IMAGE_DECLARE(img_no_wifi);
LV_IMAGE_DECLARE(img_wifi);
#ifdef __cplusplus
}
#endif

#define MARGIN_X 20
#define SCREEN_W 320
#define SCREEN_H 240
#define BOTTOM_MARGIN 14
#define FAN_LEVEL_Y 60
#define SET_TEMP_Y 108
#define MODE_Y (SCREEN_H - 50 - BOTTOM_MARGIN)
#define TOP_AIR_Y 3
#define TOP_HW_Y 24
#define TOP_TEMP_LABEL_W 40
#define TOP_TEMP_VALUE_W 78
#define TOP_TEMP_GAP 4
#define TOP_TEMP_TITLE_X (SCREEN_W - MARGIN_X - TOP_TEMP_VALUE_W - TOP_TEMP_GAP - TOP_TEMP_LABEL_W)

typedef void (*wifi_pressed_t)(void);
typedef void (*tp_pressed_t)(void);
typedef void (*manual_changed_t)(void);

class MainScreen {
   private:
      Settings *settings;
      wifi_pressed_t wifiPressedCb;
      tp_pressed_t tpPressedCb;
      manual_changed_t manualChangedCb;
      const lv_font_t *pFontLarge = &amarath_40;
      int setTemp = 21;
      lv_obj_t *screen;
      lv_obj_t *lblSetTemp;
      lv_obj_t *lblSetTempShadow;
      lv_obj_t *btnIncTemp;
      lv_obj_t *btnDecTemp;
      lv_obj_t *lblTime;
      lv_obj_t *lblAirName;
      lv_obj_t *lblAirTemp;
      lv_obj_t *lblHWName;
      lv_obj_t *lblHWTemp;
      lv_obj_t *btnModeHeat;
      lv_obj_t *btnModeFan;
      lv_obj_t *btnModeOff;
      lv_obj_t *slidFan;
      lv_obj_t *imgFan;
      lv_obj_t *lblFan;
      lv_obj_t *imgWifi;
      lv_obj_t *imgNoWifi;
      
      
      void updateSetTemp() {
         char buf[32];
         sprintf(buf, "%d°C", this->setTemp);
         lv_label_set_text(this->lblSetTemp, buf);
         lv_label_set_text(this->lblSetTempShadow, buf);
      }

      static void evtTempBtns(lv_event_t * e) {
         lv_event_code_t code = lv_event_get_code(e);
         
         if(code == LV_EVENT_CLICKED) {
            int val = (int)lv_event_get_user_data(e);
            MainScreen &main = getInstance();
            if (((val > 0) && (main.setTemp < 30)) ||
                ((val < 0) && (main.setTemp > 10)))
            {
               main.setTemp += val;
               main.settings->setSetTemp(main.setTemp);
               main.updateSetTemp();
               main.notifyManualChange();
            }
         }
      }
      
      static void setObjectsVisible(lv_obj_t *objs[], size_t objCount, bool visible) {
         
         for (int i = 0; i < objCount; i++) {
            if (visible) {
               lv_obj_clear_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
            } else {
               lv_obj_add_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
            }
         }
      }
      
      void setSetTempVisible(bool visible) {
         lv_obj_t *objs[4] = {
            lblSetTemp,
            lblSetTempShadow,
            btnIncTemp,
            btnDecTemp
         };
         setObjectsVisible(objs, 4, visible);
      }
      
      void setFanVisible(bool visible) {
         lv_obj_t *objs[3] = {
            lblFan,
            slidFan,
            imgFan
         };
         setObjectsVisible(objs, 3, visible);
      }

      void notifyManualChange() {
         if (manualChangedCb != NULL) {
            manualChangedCb();
         }
      }
      
      void selectMode(int mode)
      {
         if (mode < 0 || mode > 2) {
            mode = 0;
         }
         lv_obj_t *modeBtns[] = {
            btnModeOff,
            btnModeFan,
            btnModeHeat
         };
         for (int i = 0; i < 3; i++) {
            lv_obj_clear_state(modeBtns[i], LV_STATE_CHECKED);
         }
         lv_obj_add_state(modeBtns[mode], LV_STATE_CHECKED);
         bool modeHeat = mode == 2;
         bool modeOff = mode == 0;
         setSetTempVisible(modeHeat);
         setFanVisible(!modeOff);
      }
      
      void setFanLevel(int level)
      {
         const char *text[] = {
            "Auto",
            "Low",
            "Mid",
            "High"
         };
         if (level < 0 || level > 3) {
            level = 0;
         }

         lv_label_set_text(lblFan, text[level]);
      }
      
      void setWifiConnected(bool connected)
      {
         if (connected) {
            setObjectsVisible(&imgWifi, 1, true);
            setObjectsVisible(&imgNoWifi, 1, false);
         } else {
            setObjectsVisible(&imgWifi, 1, false);
            setObjectsVisible(&imgNoWifi, 1, true);
         }
      }
      
      static void evtModeBtn(lv_event_t * e) {
         lv_event_code_t code = lv_event_get_code(e);
         
         if(code == LV_EVENT_CLICKED) {
            MainScreen &main = getInstance();
            int mode = (int)lv_event_get_user_data(e);
            main.selectMode(mode);
            main.settings->setMode(mode);
            main.notifyManualChange();
         }
      }
      
      static void evtFanSlider(lv_event_t * e)
      {
         lv_obj_t *slider = lv_event_get_target_obj(e);
         MainScreen &main = getInstance();
         int level = lv_slider_get_value(slider);
         main.setFanLevel(level);
         main.settings->setFanLevel(level);
         main.notifyManualChange();
      }
      
      static void evtWifi(lv_event_t * e)
      {
         MainScreen &main = getInstance();
         main.wifiPressedCb();
      }

      static void evtTimeProgram(lv_event_t * e)
      {
         MainScreen &main = getInstance();
         if (main.tpPressedCb != NULL) {
            main.tpPressedCb();
         }
      }
      
      
   public:
      void init(Settings *settings, wifi_pressed_t wifiPressedCb, tp_pressed_t tpPressedCb, manual_changed_t manualChangedCb)
      {
         static lv_style_t styleSetTemp;
         static lv_style_t styleSetTempShadow;
         static lv_style_t styleText;
         static lv_style_t styleTopText;
         static lv_style_t styleBtn;
         static lv_style_t styleBtnSel;
         static lv_style_t styleWifiBtn;
         
         static lv_style_t style_bg;
         
         this->settings = settings;
         this->wifiPressedCb = wifiPressedCb;
         this->tpPressedCb = tpPressedCb;
         this->manualChangedCb = manualChangedCb;
         setTemp = settings->getSetTemp();
         
         screen = lv_obj_create(NULL);
         
         lv_style_init(&style_bg);
         lv_style_set_bg_color(&style_bg, lv_color_hex(0x00));
         
         lv_obj_t * bg_img = lv_image_create(screen);
         lv_image_set_src(bg_img, &img_blue_bg);
         
         lv_obj_add_style(screen, &style_bg, 0);
         
         lv_style_init(&styleSetTempShadow);
         lv_style_init(&styleSetTemp);
         lv_style_init(&styleText);
         lv_style_init(&styleTopText);
         lv_style_init(&styleBtn);
         lv_style_init(&styleWifiBtn);
         lv_style_set_text_font(&styleSetTempShadow, pFontLarge);
         lv_style_set_text_font(&styleSetTemp, pFontLarge);
         lv_style_set_text_font(&styleText,  &lv_font_montserrat_20);
         lv_style_set_text_font(&styleTopText, &lv_font_montserrat_18);
         lv_style_set_text_font(&styleBtn,  &lv_font_montserrat_20);
         lv_style_set_text_color(&styleSetTempShadow, lv_color_hex(0x333333));
         lv_style_set_text_color(&styleSetTemp, lv_color_hex(0x888888));
         lv_style_set_text_color(&styleText, lv_color_hex(0x888888));
         lv_style_set_text_color(&styleTopText, lv_color_hex(0x888888));
         lv_style_set_text_color(&styleBtn, lv_color_hex(0x888888));
         lv_style_set_bg_color(&styleBtn, lv_color_hex(0x333388));
         lv_style_set_pad_all(&styleBtn, 0);
         lv_style_set_bg_grad_color(&styleBtn, lv_color_hex(0x333355));
         lv_style_set_bg_grad_dir(&styleBtn, LV_GRAD_DIR_VER);
         lv_style_set_shadow_color(&styleBtn, lv_color_hex(0x333333));
         lv_style_set_shadow_offset_x(&styleBtn, 4);
         lv_style_set_shadow_offset_y(&styleBtn, 4);
         
         lv_style_init(&styleBtnSel);
         lv_style_set_text_color(&styleBtnSel, lv_color_hex(0x888888));
         lv_style_set_bg_color(&styleBtnSel, lv_color_hex(0x5555AA));
         lv_style_set_pad_all(&styleBtnSel, 0);
         lv_style_set_bg_grad_color(&styleBtnSel, lv_color_hex(0x606080));
         lv_style_set_bg_grad_dir(&styleBtnSel, LV_GRAD_DIR_VER);
         lv_style_set_shadow_color(&styleBtnSel, lv_color_hex(0x555555));
         lv_style_set_shadow_offset_x(&styleBtnSel, 0);
         lv_style_set_shadow_offset_y(&styleBtnSel, 0);

         lv_style_set_bg_opa(&styleWifiBtn, 0);
         lv_style_set_shadow_offset_x(&styleWifiBtn, 0);
         lv_style_set_shadow_offset_y(&styleWifiBtn, 0);
         lv_style_set_border_color(&styleWifiBtn, lv_color_hex(0x888888));
         lv_style_set_border_width(&styleWifiBtn, 1);


         lblSetTempShadow = lv_label_create(screen);
         lblSetTemp = lv_label_create(screen);
         lblTime = lv_label_create(screen);
         lblAirName = lv_label_create(screen);
         lblAirTemp = lv_label_create(screen);
         lblHWName = lv_label_create(screen);
         lblHWTemp = lv_label_create(screen);

         lv_obj_add_style(lblSetTempShadow, &styleSetTempShadow, 0);
         lv_obj_add_style(lblSetTemp, &styleSetTemp, 0);
         lv_obj_add_style(lblTime, &styleTopText, 0);
         lv_obj_add_style(lblAirName, &styleTopText, 0);
         lv_obj_add_style(lblAirTemp, &styleTopText, 0);
         lv_obj_add_style(lblHWName, &styleTopText, 0);
         lv_obj_add_style(lblHWTemp, &styleTopText, 0);
         
         updateSetTemp();
         lv_obj_set_pos(lblSetTempShadow, 103, SET_TEMP_Y + 3);
         lv_obj_set_pos(lblSetTemp, 100, SET_TEMP_Y);
         lv_obj_set_pos(lblTime, MARGIN_X + 65, 4);

         lv_label_set_text(lblAirName, "Air:");
         lv_label_set_text(lblHWName, "HW:");
         lv_obj_set_pos(lblAirName, TOP_TEMP_TITLE_X, TOP_AIR_Y);
         lv_obj_set_pos(lblHWName, TOP_TEMP_TITLE_X, TOP_HW_Y);
         

        lv_obj_t *btnWifi = lv_btn_create(screen);
        lv_obj_set_pos(btnWifi, MARGIN_X-2, 3);
        lv_obj_set_size(btnWifi, 26, 26);
        lv_obj_add_style(btnWifi, &styleWifiBtn, 0);
        lv_obj_add_event_cb(btnWifi, evtWifi, LV_EVENT_CLICKED, NULL);
        imgWifi = lv_image_create(btnWifi);
        lv_image_set_src(imgWifi, &img_wifi);
        lv_obj_center(imgWifi);
        imgNoWifi = lv_image_create(btnWifi);
        lv_image_set_src(imgNoWifi, &img_no_wifi);
        lv_obj_center(imgNoWifi);

        lv_obj_t *btnTp = lv_btn_create(screen);
        lv_obj_set_pos(btnTp, MARGIN_X + 32, 3);
        lv_obj_set_size(btnTp, 26, 26);
        lv_obj_add_style(btnTp, &styleWifiBtn, 0);
        lv_obj_add_event_cb(btnTp, evtTimeProgram, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lblTp = lv_label_create(btnTp);
        lv_label_set_text(lblTp, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_color(lblTp, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(lblTp, &lv_font_montserrat_18, 0);
        lv_obj_center(lblTp);
         
         lv_obj_t * lblDecTemp; 
         btnDecTemp = lv_btn_create(screen);
         lblDecTemp = lv_label_create(btnDecTemp);
         lv_label_set_text(lblDecTemp, LV_SYMBOL_MINUS);
         lv_obj_center(lblDecTemp);
         lv_obj_set_pos(btnDecTemp, MARGIN_X, SET_TEMP_Y + 4);
         lv_obj_set_size(btnDecTemp,50,50);
         
         lv_obj_t * lblIncTemp; 
         btnIncTemp = lv_btn_create(screen);
         lblIncTemp = lv_label_create(btnIncTemp);
         lv_label_set_text(lblIncTemp, LV_SYMBOL_PLUS);
         lv_obj_center(lblIncTemp);
         lv_obj_set_pos(btnIncTemp, 250, SET_TEMP_Y + 4);
         lv_obj_set_size(btnIncTemp,50,50);
         lv_obj_add_style(btnDecTemp, &styleBtn, 0);
         lv_obj_add_style(btnIncTemp, &styleBtn, 0);
         lv_obj_add_event_cb(btnDecTemp, evtTempBtns, LV_EVENT_CLICKED, (void *)-1);
         lv_obj_add_event_cb(btnIncTemp, evtTempBtns, LV_EVENT_CLICKED, (void *)1);
         
         
         slidFan = lv_slider_create(screen);
         lv_slider_set_range(slidFan, 0, 3);
         lv_obj_set_pos(slidFan, 95, FAN_LEVEL_Y + 12);
         lv_obj_add_event_cb(slidFan, evtFanSlider, LV_EVENT_VALUE_CHANGED, NULL);
         
         lv_obj_set_width(slidFan, 130);
         imgFan = lv_image_create(screen);
         lv_image_set_src(imgFan, &img_fan);
         lv_obj_set_pos(imgFan, MARGIN_X + 5, FAN_LEVEL_Y);
         
         lblFan = lv_label_create(screen);
         lv_label_set_text(lblFan, "Auto");
         lv_obj_add_style(lblFan, &styleText, 0);
         lv_obj_set_pos(lblFan, 250, FAN_LEVEL_Y + 6);
         
         lv_obj_t * lblMode = lv_label_create(screen);
         lv_label_set_text(lblMode, "Mode:");
         lv_obj_add_style(lblMode, &styleText, 0);
         lv_obj_set_pos(lblMode, MARGIN_X, MODE_Y + 13);
         
         btnModeOff = lv_btn_create(screen);
         lv_obj_add_flag(btnModeOff, LV_OBJ_FLAG_CHECKABLE);
         lv_obj_add_style(btnModeOff, &styleBtn, 0);
         lv_obj_set_size(btnModeOff,50,50);
         lv_obj_set_pos(btnModeOff, 110, MODE_Y);
         lv_obj_t * lblPower = lv_label_create(btnModeOff);
         lv_label_set_text(lblPower, "OFF");
         lv_obj_center(lblPower);
         
         btnModeFan = lv_btn_create(screen);
         lv_obj_add_flag(btnModeFan, LV_OBJ_FLAG_CHECKABLE);
         lv_obj_add_style(btnModeFan, &styleBtn, 0);
         lv_obj_set_size(btnModeFan,50,50);
         lv_obj_set_pos(btnModeFan, 180, MODE_Y);
         
         lv_obj_t * imgModeFan = lv_image_create(btnModeFan);
         lv_image_set_src(imgModeFan, &img_fan);
         lv_obj_center(imgModeFan);
         
         btnModeHeat = lv_btn_create(screen);
         lv_obj_add_flag(btnModeHeat, LV_OBJ_FLAG_CHECKABLE);
         lv_obj_add_style(btnModeHeat, &styleBtn, 0);
         lv_obj_set_size(btnModeHeat,50,50);
         lv_obj_set_pos(btnModeHeat, 250, MODE_Y);
         
         lv_obj_t * imgModeHeat = lv_image_create(btnModeHeat);
         lv_image_set_src(imgModeHeat, &img_heat);
         lv_obj_center(imgModeHeat);
         
         lv_obj_add_event_cb(btnModeHeat, evtModeBtn, LV_EVENT_CLICKED, (void *)2);
         lv_obj_add_event_cb(btnModeFan, evtModeBtn, LV_EVENT_CLICKED, (void *)1);
         lv_obj_add_event_cb(btnModeOff, evtModeBtn, LV_EVENT_CLICKED, (void *)0);
         
         setAirTemp(NAN);
         setHWTemp(NAN);
         lv_label_set_text(lblTime, "-:-");
         setWifiConnected(false);
         reloadSettings();
      }
      
      static MainScreen& getInstance()
      {
         static MainScreen instance;
         return instance;
      }
      
      void placeTopTempValue(lv_obj_t *lbl, int y) {
         lv_obj_align(lbl, LV_ALIGN_TOP_RIGHT, -MARGIN_X, y);
      }

      void setAirTemp(float airTemp) {
         char buffer[16];
         if (isnan(airTemp)) {
            snprintf(buffer, sizeof(buffer), "-");
         } else {
            snprintf(buffer, sizeof(buffer), "%.1f°C", airTemp);
         }
         lv_label_set_text(lblAirTemp, buffer);
         placeTopTempValue(lblAirTemp, TOP_AIR_Y);
      }
      
      void setHWTemp(float hwTemp) {
         char buffer[16];
         if (isnan(hwTemp)) {
            snprintf(buffer, sizeof(buffer), "-");
         } else {
            snprintf(buffer, sizeof(buffer), "%d°C", (int)hwTemp);
         }
         lv_label_set_text(lblHWTemp, buffer);
         placeTopTempValue(lblHWTemp, TOP_HW_Y);
      }
      
      void update(void)
      {
         static bool wifiWasConnected = false;
         static int cnt = 0;
         if (cnt++ % 100 == 0) {
            time_t now = time(nullptr);

            bool wifiConnected = (WiFi.status() == WL_CONNECTED);
            if (wifiWasConnected != wifiConnected) {
               wifiWasConnected = wifiConnected;
               setWifiConnected(wifiConnected);
            }

            if (wifiConnected) {
               struct tm timeinfo;
               if(getLocalTime(&timeinfo)) {
                  char buffer[16];
                  strftime(buffer,sizeof(buffer), "%H:%M", &timeinfo);
                  lv_label_set_text(lblTime, buffer);
               }
            }
         }
      }
      
      void show(void)
      {
         lv_scr_load(screen); 
      }

      void reloadSettings(void)
      {
         int mode = settings->getMode();
         int fanLevel = settings->getFanLevel();
         int temp = settings->getSetTemp();
         if (mode < 0 || mode > 2) {
            settings->setMode(0);
            return;
         }
         if (fanLevel < 0 || fanLevel > 3) {
            settings->setFanLevel(0);
            return;
         }
         if (temp < 10 || temp > 30) {
            settings->setSetTemp(21);
            return;
         }
         selectMode(mode);
         setFanLevel(fanLevel);
         lv_slider_set_value(slidFan, fanLevel, LV_ANIM_OFF);
         setTemp = temp;
         updateSetTemp();
      }
};
