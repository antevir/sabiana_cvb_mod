#pragma once

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#include "WiFi.h"
#include "settings.hpp"

LV_IMAGE_DECLARE(img_blue_bg);

#define MARGIN_X 10
#define SET_TEMP_Y 95
#define FAN_LEVEL_Y 40
#define MODE_Y 170

typedef void (*wifi_setup_done_t)(bool store);

class WifiSetupScreen {
   private:
      Settings *settings;
      wifi_setup_done_t wifiSetupDoneCb;
      lv_obj_t *screen;
      lv_obj_t *lblTime;
      lv_obj_t *lblAirTemp;
      lv_obj_t *lblHWTemp;
      lv_obj_t *btnModeHeat;
      lv_obj_t *btnModeFan;
      lv_obj_t *btnModeOff;
      lv_obj_t *slidFan;
      lv_obj_t *imgFan;
      lv_obj_t *lblFan;
      lv_obj_t *imgWifi;
      lv_obj_t *keyboard;
      lv_obj_t *ddSsid;
      lv_obj_t *taPassword;
      
      void doWifiScan(void) {
         int n = WiFi.scanNetworks();
         lv_dropdown_clear_options(ddSsid);
         for (int i = 0; i < n; i++) {
            lv_dropdown_add_option(ddSsid, WiFi.SSID(i).c_str(), LV_DROPDOWN_POS_LAST);
         }
         WiFi.scanDelete();
         lv_dropdown_open(ddSsid);
      }
      
      void storeSettings(void) {
         char buf[128];
         lv_dropdown_get_selected_str(ddSsid, buf, sizeof(buf));
         String psk = lv_textarea_get_text(taPassword);
         settings->setSsid(buf);
         settings->setPsk(psk);
      }
      
      static void evtBtnScan(lv_event_t * e)
      {
         lv_event_code_t code = lv_event_get_code(e);
         if (code == LV_EVENT_CLICKED) {
            WifiSetupScreen &wifi = getInstance();
            wifi.doWifiScan();
         }
      }
      
      static void evtBtnOkCancel(lv_event_t * e)
      {
         lv_event_code_t code = lv_event_get_code(e);
         int val = (int)lv_event_get_user_data(e);
         if (code == LV_EVENT_CLICKED) {
            WifiSetupScreen &wifi = getInstance();
            if (val > 0) {
               wifi.storeSettings();
            }
            wifi.wifiSetupDoneCb(val > 0);
         }
      }
      
      static void evtTextArea(lv_event_t * e)
      {
         lv_event_code_t code = lv_event_get_code(e);
         lv_obj_t *ta = lv_event_get_target_obj(e);
         if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
            WifiSetupScreen &wifi = getInstance();
            /*Focus on the clicked text area*/
            if (wifi.keyboard != NULL) {
               lv_keyboard_set_textarea(wifi.keyboard, ta);
            }
         }
      }
      
   public:
      void init(Settings *settings, wifi_setup_done_t wifiSetupDoneCb) {
         static lv_style_t styleText;
         static lv_style_t styleBtn;
         static lv_style_t styleBtnSel;
         
         static lv_style_t style_bg;
         
         this->settings = settings;
         this->wifiSetupDoneCb = wifiSetupDoneCb;
         screen = lv_obj_create(NULL);
         
         lv_style_init(&style_bg);
         lv_style_set_bg_color(&style_bg, lv_color_hex(0x00));
         
         lv_obj_t * bg_img = lv_image_create(screen);
         lv_image_set_src(bg_img, &img_blue_bg);
         
         lv_obj_add_style(screen, &style_bg, 0);
         
         lv_style_init(&styleText);
         lv_style_init(&styleBtn);
         lv_style_set_text_font(&styleText,  &lv_font_montserrat_14);
         lv_style_set_text_font(&styleBtn,  &lv_font_montserrat_14);
         lv_style_set_text_color(&styleText, lv_color_hex(0xBBBBBB));
         lv_style_set_text_color(&styleBtn, lv_color_hex(0xBBBBBB));
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
         
         lv_obj_t *lblSsid = lv_label_create(screen);
         lv_obj_t *lblPassword = lv_label_create(screen);
         ddSsid = lv_dropdown_create(screen);

         taPassword = lv_textarea_create(screen);
         lv_obj_add_style(lblSsid, &styleText, 0);
         lv_obj_add_style(lblPassword, &styleText, 0);
         lv_obj_set_pos(lblSsid, MARGIN_X, 10);
         lv_obj_set_pos(lblPassword, MARGIN_X, 50);
         lv_label_set_text(lblSsid, "SSID:");
         lv_label_set_text(lblPassword, "PSK:");
         
         lv_textarea_set_password_mode(taPassword, true);
         lv_textarea_set_one_line(taPassword, true);

         lv_obj_set_pos(ddSsid, 55, 0);
         lv_obj_set_pos(taPassword, 55, 40);
         lv_obj_set_width(ddSsid, 200);
         lv_obj_set_width(taPassword, 256);
         lv_obj_add_event_cb(taPassword, evtTextArea, LV_EVENT_ALL, NULL);
         
         lv_obj_t *btnScan = lv_btn_create(screen);
         lv_obj_t *lblScan = lv_label_create(btnScan);
         lv_label_set_text(lblScan, "Scan");
         lv_obj_center(lblScan);
         lv_obj_add_style(btnScan, &styleBtn, 0);
         lv_obj_align_to(btnScan, ddSsid, LV_ALIGN_OUT_RIGHT_TOP, 5, 0);
         lv_obj_set_size(btnScan, 50,35);
         lv_obj_add_event_cb(btnScan, evtBtnScan, LV_EVENT_ALL, NULL);
         
         keyboard = lv_keyboard_create(screen);
         lv_obj_set_size(keyboard,  LV_HOR_RES, LV_VER_RES / 2);
         lv_keyboard_set_textarea(keyboard, taPassword);
         
         
         lv_obj_t *btnCancel = lv_btn_create(screen);
         lv_obj_t *lblCancel = lv_label_create(btnCancel);
         lv_label_set_text(lblCancel, "Cancel");
         lv_obj_center(lblCancel);
         lv_obj_add_style(btnCancel, &styleBtn, 0);
         lv_obj_set_pos(btnCancel, 250, 82);
         lv_obj_set_size(btnCancel, 60,30);
         lv_obj_add_event_cb(btnCancel, evtBtnOkCancel, LV_EVENT_ALL, (void *)-1);
         
         lv_obj_t *btnOk = lv_btn_create(screen);
         lv_obj_t *lblOk = lv_label_create(btnOk);
         lv_label_set_text(lblOk, "OK");
         lv_obj_center(lblOk);
         lv_obj_add_style(btnOk, &styleBtn, 0);
         lv_obj_set_pos(btnOk, 180, 82);
         lv_obj_set_size(btnOk, 60,30);
         lv_obj_add_event_cb(btnOk, evtBtnOkCancel, LV_EVENT_ALL, (void *)1);
         
         String ssid = settings->getSsid();
         String psk = settings->getPsk();
         lv_dropdown_set_options(ddSsid, ssid.c_str());
         lv_textarea_set_text(taPassword, psk.c_str());
      }
      
      static WifiSetupScreen& getInstance()
      {
         static WifiSetupScreen instance;
         return instance;
      }

      void show(void)
      {
         lv_scr_load(screen); 
      }
};
