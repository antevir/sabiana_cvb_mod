#pragma once

#include <lvgl.h>
#include <stdint.h>
#include <stdio.h>

LV_IMAGE_DECLARE(img_blue_bg);
LV_IMAGE_DECLARE(img_fan);
LV_IMAGE_DECLARE(img_heat);
LV_FONT_DECLARE(amarath_40);

#define TP_MARGIN_X 20
#define TP_SET_TEMP_Y 95
#define TP_FAN_LEVEL_Y 40
#define TP_MODE_Y 170

typedef void (*tp_close_cb_t)(void);

class TimeProgramUI {
   private:
      struct ProfileData {
         int setTemp;
         int fanLevel;
         int mode;
      };

      struct ProfileScreen {
         lv_obj_t *screen = nullptr;
         lv_obj_t *lblTemp = nullptr;
         lv_obj_t *lblTempShadow = nullptr;
         lv_obj_t *btnInc = nullptr;
         lv_obj_t *btnDec = nullptr;
         lv_obj_t *lblFan = nullptr;
         lv_obj_t *imgFan = nullptr;
         lv_obj_t *sliderFan = nullptr;
         lv_obj_t *btnMode[3] = { nullptr, nullptr, nullptr };
         struct TempCtx {
            int profile;
            int delta;
         } tempCtx[2];
         struct ModeCtx {
            int profile;
            int mode;
         } modeCtx[3];
         struct FanCtx {
            int profile;
         } fanCtx;
      };

      struct WeekRow {
         lv_obj_t *slider = nullptr;
         lv_obj_t *lblRange = nullptr;
         int dayIndex = 0;
      };

      tp_close_cb_t closeCb = nullptr;
      lv_obj_t *menuScreen = nullptr;
      lv_obj_t *weekScreen = nullptr;
      ProfileScreen profileScreens[2];

      static constexpr int WEEK_DAYS = 7;
      static constexpr int SLOT_COUNT = 24; // 1 hour increments
      int dayStart[WEEK_DAYS];
      int dayEnd[WEEK_DAYS];
      WeekRow weekRows[WEEK_DAYS];

      ProfileData profiles[2];

      static const char * const PROFILE_NAMES[2];
      static const char * const DAY_NAMES[WEEK_DAYS];

      TimeProgramUI() {
         for (int i = 0; i < WEEK_DAYS; i++) {
            dayStart[i] = 6;
            dayEnd[i] = 18;
            weekRows[i].dayIndex = i;
         }
         profiles[0] = { 21, 1, 2 };
         profiles[1] = { 18, 0, 0 };
      }

      static TimeProgramUI& instance() {
         static TimeProgramUI inst;
         return inst;
      }

      static void evtMenuClose(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         TimeProgramUI &ui = instance();
         if (ui.closeCb) {
            ui.closeCb();
         }
      }

      static void evtMenuOpenWeek(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         instance().showWeek();
      }

      static void evtMenuOpenProfile(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         intptr_t profile = (intptr_t)lv_event_get_user_data(e);
         instance().showProfile((int)profile);
      }

      static void evtWeekSlider(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            return;
         }
         WeekRow *row = (WeekRow *)lv_event_get_user_data(e);
         TimeProgramUI &ui = instance();
         int start = lv_slider_get_left_value(row->slider);
         int end = lv_slider_get_value(row->slider);
         if (start > end) start = end;
         ui.dayStart[row->dayIndex] = start;
         ui.dayEnd[row->dayIndex] = end;
         ui.updateWeekLabel(row->dayIndex);
      }

      static void evtWeekBack(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         instance().showMenu();
      }

      static void evtProfileBack(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         instance().showMenu();
      }

      static void evtTempButton(lv_event_t * e) {
         lv_event_code_t code = lv_event_get_code(e);
         if (code != LV_EVENT_CLICKED && code != LV_EVENT_LONG_PRESSED_REPEAT) {
            return;
         }
         ProfileScreen::TempCtx *ctx = (ProfileScreen::TempCtx *)lv_event_get_user_data(e);
         TimeProgramUI &ui = instance();
         ProfileData &data = ui.profiles[ctx->profile];
         data.setTemp += ctx->delta;
         if (data.setTemp < 10) data.setTemp = 10;
         if (data.setTemp > 30) data.setTemp = 30;
         ui.updateProfileTemp(ctx->profile);
      }

      static void evtFanSlider(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            return;
         }
         ProfileScreen::FanCtx *ctx = (ProfileScreen::FanCtx *)lv_event_get_user_data(e);
         TimeProgramUI &ui = instance();
         ProfileData &data = ui.profiles[ctx->profile];
         data.fanLevel = lv_slider_get_value(lv_event_get_target_obj(e));
         ui.updateProfileFan(ctx->profile);
      }

      static void evtModeButton(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         ProfileScreen::ModeCtx *ctx = (ProfileScreen::ModeCtx *)lv_event_get_user_data(e);
         instance().selectProfileMode(ctx->profile, ctx->mode);
      }

      void buildMenuScreen() {
         static lv_style_t styleBg;
         static lv_style_t styleText;
         static lv_style_t styleBtn;

         lv_style_init(&styleBg);
         lv_style_set_bg_color(&styleBg, lv_color_hex(0x000000));

         lv_style_init(&styleText);
         lv_style_set_text_font(&styleText, &lv_font_montserrat_20);
         lv_style_set_text_color(&styleText, lv_color_hex(0xFFFFFF));

         lv_style_init(&styleBtn);
         lv_style_set_text_font(&styleBtn, &lv_font_montserrat_16);
         lv_style_set_text_color(&styleBtn, lv_color_hex(0xFFFFFF));
         lv_style_set_bg_color(&styleBtn, lv_color_hex(0x3C3C7A));
         lv_style_set_bg_grad_color(&styleBtn, lv_color_hex(0x2A2A58));
         lv_style_set_bg_grad_dir(&styleBtn, LV_GRAD_DIR_VER);

         menuScreen = lv_obj_create(NULL);
         lv_obj_add_style(menuScreen, &styleBg, 0);
         lv_obj_t *bg = lv_image_create(menuScreen);
         lv_image_set_src(bg, &img_blue_bg);

         lv_obj_t *title = lv_label_create(menuScreen);
         lv_obj_add_style(title, &styleText, 0);
         lv_label_set_text(title, "Time program");
         lv_obj_set_pos(title, 12, 8);

         lv_obj_t *btnClose = lv_btn_create(menuScreen);
         lv_obj_add_style(btnClose, &styleBtn, 0);
         lv_obj_set_size(btnClose, 70, 30);
         lv_obj_set_pos(btnClose, 230, 8);
         lv_obj_t *lblClose = lv_label_create(btnClose);
         lv_label_set_text(lblClose, "Close");
         lv_obj_center(lblClose);
         lv_obj_add_event_cb(btnClose, evtMenuClose, LV_EVENT_CLICKED, NULL);

         auto addButton = [&](const char *text, int y, lv_event_cb_t cb, intptr_t data = 0) {
            lv_obj_t *btn = lv_btn_create(menuScreen);
            lv_obj_add_style(btn, &styleBtn, 0);
            lv_obj_set_size(btn, 220, 40);
            lv_obj_set_pos(btn, 50, y);
            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, text);
            lv_obj_center(lbl);
            lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void *)data);
         };

         addButton("Week schedule", 60, evtMenuOpenWeek);
         addButton("Active settings", 110, evtMenuOpenProfile, 0);
         addButton("Inactive settings", 160, evtMenuOpenProfile, 1);
      }

      void buildWeekScreen() {
         static lv_style_t styleBg;
         static lv_style_t styleText;
         static lv_style_t styleSlider;
         static lv_style_t styleBtn;

         lv_style_init(&styleBg);
         lv_style_set_bg_color(&styleBg, lv_color_hex(0x000000));

         lv_style_init(&styleText);
         lv_style_set_text_font(&styleText, &lv_font_montserrat_20);
         lv_style_set_text_color(&styleText, lv_color_hex(0xFFFFFF));

         lv_style_init(&styleSlider);
         lv_style_set_bg_color(&styleSlider, lv_color_hex(0x2A2A4A));
         lv_style_set_border_color(&styleSlider, lv_color_hex(0x555577));
         lv_style_set_border_width(&styleSlider, 1);

         lv_style_init(&styleBtn);
         lv_style_set_text_font(&styleBtn, &lv_font_montserrat_14);
         lv_style_set_text_color(&styleBtn, lv_color_hex(0xFFFFFF));
         lv_style_set_bg_color(&styleBtn, lv_color_hex(0x3C3C7A));
         lv_style_set_bg_grad_color(&styleBtn, lv_color_hex(0x2A2A58));
         lv_style_set_bg_grad_dir(&styleBtn, LV_GRAD_DIR_VER);

         weekScreen = lv_obj_create(NULL);
         lv_obj_add_style(weekScreen, &styleBg, 0);
         lv_obj_t *bg = lv_image_create(weekScreen);
         lv_image_set_src(bg, &img_blue_bg);

         lv_obj_t *btnBack = lv_btn_create(weekScreen);
         lv_obj_add_style(btnBack, &styleBtn, 0);
         lv_obj_set_size(btnBack, 70, 28);
         lv_obj_set_pos(btnBack, 232, 8);
         lv_obj_t *lblBack = lv_label_create(btnBack);
         lv_label_set_text(lblBack, "Back");
         lv_obj_center(lblBack);
         lv_obj_add_event_cb(btnBack, evtWeekBack, LV_EVENT_CLICKED, NULL);

         lv_obj_t *title = lv_label_create(weekScreen);
         lv_obj_add_style(title, &styleText, 0);
         lv_label_set_text(title, "Week schedule");
         lv_obj_set_pos(title, 12, 8);

         for (int i = 0; i < WEEK_DAYS; i++) {
            lv_obj_t *lblDay = lv_label_create(weekScreen);
            lv_obj_add_style(lblDay, &styleText, 0);
            lv_label_set_text(lblDay, DAY_NAMES[i]);
            lv_obj_set_pos(lblDay, 12, 40 + i * 28);

            lv_obj_t *slider = lv_slider_create(weekScreen);
            lv_obj_add_style(slider, &styleSlider, 0);
            lv_slider_set_mode(slider, LV_SLIDER_MODE_RANGE);
            lv_slider_set_range(slider, 0, SLOT_COUNT);
            lv_obj_set_size(slider, 180, 10);
            lv_obj_set_pos(slider, 70, 44 + i * 28);
            lv_slider_set_left_value(slider, dayStart[i], LV_ANIM_OFF);
            lv_slider_set_value(slider, dayEnd[i], LV_ANIM_OFF);

            lv_obj_t *lblRange = lv_label_create(weekScreen);
            lv_obj_add_style(lblRange, &styleText, 0);
            lv_obj_set_pos(lblRange, 260, 40 + i * 28);

            weekRows[i].slider = slider;
            weekRows[i].lblRange = lblRange;
            weekRows[i].dayIndex = i;
            lv_obj_add_event_cb(slider, evtWeekSlider, LV_EVENT_VALUE_CHANGED, &weekRows[i]);
            updateWeekLabel(i);
         }
      }

      void buildProfileScreen(int profile) {
         static lv_style_t styleBg;
         static lv_style_t styleText;
         static lv_style_t styleTitle;
         static lv_style_t styleTemp;
         static lv_style_t styleTempShadow;
         static lv_style_t styleBtn;
         static lv_style_t styleBtnSel;
         static bool stylesInit = false;

         if (!stylesInit) {
            stylesInit = true;
            lv_style_init(&styleBg);
            lv_style_set_bg_color(&styleBg, lv_color_hex(0x000000));

            lv_style_init(&styleText);
            lv_style_set_text_font(&styleText, &lv_font_montserrat_14);
            lv_style_set_text_color(&styleText, lv_color_hex(0xBBBBBB));

            lv_style_init(&styleTitle);
            lv_style_set_text_font(&styleTitle, &lv_font_montserrat_20);
            lv_style_set_text_color(&styleTitle, lv_color_hex(0xFFFFFF));

            lv_style_init(&styleTempShadow);
            lv_style_set_text_font(&styleTempShadow, &amarath_40);
            lv_style_set_text_color(&styleTempShadow, lv_color_hex(0x333333));

            lv_style_init(&styleTemp);
            lv_style_set_text_font(&styleTemp, &amarath_40);
            lv_style_set_text_color(&styleTemp, lv_color_hex(0xEEEEEE));

            lv_style_init(&styleBtn);
            lv_style_set_text_font(&styleBtn, &lv_font_montserrat_16);
            lv_style_set_text_color(&styleBtn, lv_color_hex(0xFFFFFF));
            lv_style_set_bg_color(&styleBtn, lv_color_hex(0x333388));
            lv_style_set_bg_grad_color(&styleBtn, lv_color_hex(0x2A2A58));
            lv_style_set_bg_grad_dir(&styleBtn, LV_GRAD_DIR_VER);
            lv_style_set_radius(&styleBtn, 6);

            lv_style_init(&styleBtnSel);
            lv_style_set_text_color(&styleBtnSel, lv_color_hex(0xFFFFFF));
            lv_style_set_bg_color(&styleBtnSel, lv_color_hex(0x5555AA));
            lv_style_set_bg_grad_color(&styleBtnSel, lv_color_hex(0x6767CC));
            lv_style_set_bg_grad_dir(&styleBtnSel, LV_GRAD_DIR_VER);
            lv_style_set_radius(&styleBtnSel, 6);
         }

         ProfileScreen &screen = profileScreens[profile];
         screen.screen = lv_obj_create(NULL);
         lv_obj_add_style(screen.screen, &styleBg, 0);
         lv_obj_t *bg = lv_image_create(screen.screen);
         lv_image_set_src(bg, &img_blue_bg);

         lv_obj_t *title = lv_label_create(screen.screen);
         lv_obj_add_style(title, &styleTitle, 0);
         lv_label_set_text_fmt(title, "%s settings", PROFILE_NAMES[profile]);
         lv_obj_set_pos(title, 12, 8);

         lv_obj_t *btnBack = lv_btn_create(screen.screen);
         lv_obj_add_style(btnBack, &styleBtn, 0);
         lv_obj_set_size(btnBack, 70, 30);
         lv_obj_set_pos(btnBack, 232, 8);
         lv_obj_t *lblBack = lv_label_create(btnBack);
         lv_label_set_text(lblBack, "Back");
         lv_obj_center(lblBack);
         lv_obj_add_event_cb(btnBack, evtProfileBack, LV_EVENT_CLICKED, NULL);

         screen.lblTempShadow = lv_label_create(screen.screen);
         lv_obj_add_style(screen.lblTempShadow, &styleTempShadow, 0);
         lv_obj_set_pos(screen.lblTempShadow, 103, TP_SET_TEMP_Y + 3);

         screen.lblTemp = lv_label_create(screen.screen);
         lv_obj_add_style(screen.lblTemp, &styleTemp, 0);
         lv_obj_set_pos(screen.lblTemp, 100, TP_SET_TEMP_Y);

         screen.btnDec = lv_btn_create(screen.screen);
         lv_obj_set_size(screen.btnDec, 50, 50);
         lv_obj_set_pos(screen.btnDec, TP_MARGIN_X, TP_SET_TEMP_Y + 4);
         lv_obj_add_style(screen.btnDec, &styleBtn, 0);
         lv_obj_t *lblDec = lv_label_create(screen.btnDec);
         lv_label_set_text(lblDec, LV_SYMBOL_MINUS);
         lv_obj_center(lblDec);
         screen.tempCtx[0].profile = profile;
         screen.tempCtx[0].delta = -1;
         lv_obj_add_event_cb(screen.btnDec, evtTempButton, LV_EVENT_ALL, &screen.tempCtx[0]);

         screen.btnInc = lv_btn_create(screen.screen);
         lv_obj_set_size(screen.btnInc, 50, 50);
         lv_obj_set_pos(screen.btnInc, 250, TP_SET_TEMP_Y + 4);
         lv_obj_add_style(screen.btnInc, &styleBtn, 0);
         lv_obj_t *lblInc = lv_label_create(screen.btnInc);
         lv_label_set_text(lblInc, LV_SYMBOL_PLUS);
         lv_obj_center(lblInc);
         screen.tempCtx[1].profile = profile;
         screen.tempCtx[1].delta = 1;
         lv_obj_add_event_cb(screen.btnInc, evtTempButton, LV_EVENT_ALL, &screen.tempCtx[1]);

         screen.imgFan = lv_image_create(screen.screen);
         lv_image_set_src(screen.imgFan, &img_fan);
         lv_obj_set_pos(screen.imgFan, TP_MARGIN_X + 5, TP_FAN_LEVEL_Y);

         screen.lblFan = lv_label_create(screen.screen);
         lv_obj_add_style(screen.lblFan, &styleText, 0);
         lv_obj_set_pos(screen.lblFan, 250, TP_FAN_LEVEL_Y + 6);

         screen.sliderFan = lv_slider_create(screen.screen);
         lv_slider_set_range(screen.sliderFan, 0, 3);
         lv_obj_set_pos(screen.sliderFan, 95, TP_FAN_LEVEL_Y + 12);
         lv_obj_set_width(screen.sliderFan, 130);
         screen.fanCtx.profile = profile;
         lv_obj_add_event_cb(screen.sliderFan, evtFanSlider, LV_EVENT_VALUE_CHANGED, &screen.fanCtx);

         for (int i = 0; i < 3; i++) {
            screen.btnMode[i] = lv_btn_create(screen.screen);
            lv_obj_add_flag(screen.btnMode[i], LV_OBJ_FLAG_CHECKABLE);
            lv_obj_add_style(screen.btnMode[i], &styleBtn, 0);
            lv_obj_add_style(screen.btnMode[i], &styleBtnSel, LV_STATE_CHECKED);
            lv_obj_set_size(screen.btnMode[i], 50, 50);
            lv_obj_set_pos(screen.btnMode[i], 110 + i * 70, TP_MODE_Y);

            if (i == 0) {
               lv_obj_t *lbl = lv_label_create(screen.btnMode[i]);
               lv_label_set_text(lbl, "OFF");
               lv_obj_center(lbl);
            } else if (i == 1) {
               lv_obj_t *img = lv_image_create(screen.btnMode[i]);
               lv_image_set_src(img, &img_fan);
               lv_obj_center(img);
            } else {
               lv_obj_t *img = lv_image_create(screen.btnMode[i]);
               lv_image_set_src(img, &img_heat);
               lv_obj_center(img);
            }

            screen.modeCtx[i].profile = profile;
            screen.modeCtx[i].mode = i;
            lv_obj_add_event_cb(screen.btnMode[i], evtModeButton, LV_EVENT_CLICKED, &screen.modeCtx[i]);
         }
      }

      void updateWeekLabel(int day) {
         if (!weekRows[day].lblRange) return;
         char buf[16];
         snprintf(buf, sizeof(buf), "%02d-%02d", dayStart[day], dayEnd[day]);
         lv_label_set_text(weekRows[day].lblRange, buf);
      }

      void updateProfileTemp(int profile) {
         ProfileScreen &screen = profileScreens[profile];
         if (!screen.lblTemp) return;
         char buf[8];
         snprintf(buf, sizeof(buf), "%d°C", profiles[profile].setTemp);
         lv_label_set_text(screen.lblTemp, buf);
         if (screen.lblTempShadow) {
            lv_label_set_text(screen.lblTempShadow, buf);
         }
      }

      void updateProfileFan(int profile) {
         ProfileScreen &screen = profileScreens[profile];
         if (!screen.lblFan) return;
         static const char *text[] = { "Auto", "Low", "Mid", "High" };
         int lvl = profiles[profile].fanLevel;
         if (lvl < 0) lvl = 0;
         if (lvl > 3) lvl = 3;
         lv_label_set_text(screen.lblFan, text[lvl]);
      }

      void setTempControlsVisible(int profile, bool visible) {
         ProfileScreen &screen = profileScreens[profile];
         lv_obj_t *objs[] = { screen.lblTemp, screen.lblTempShadow, screen.btnInc, screen.btnDec };
         for (lv_obj_t *obj : objs) {
            if (!obj) continue;
            if (visible) {
               lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
               lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
         }
      }

      void setFanControlsVisible(int profile, bool visible) {
         ProfileScreen &screen = profileScreens[profile];
         lv_obj_t *objs[] = { screen.lblFan, screen.imgFan, screen.sliderFan };
         for (lv_obj_t *obj : objs) {
            if (!obj) continue;
            if (visible) {
               lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
               lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
         }
      }

      void refreshProfileVisibility(int profile) {
         int mode = profiles[profile].mode;
         bool heat = (mode == 2);
         bool off = (mode == 0);
         setTempControlsVisible(profile, heat);
         setFanControlsVisible(profile, !off);
      }

      void selectProfileMode(int profile, int mode) {
         if (mode < 0) mode = 0;
         if (mode > 2) mode = 2;
         profiles[profile].mode = mode;
         for (int i = 0; i < 3; i++) {
            if (profileScreens[profile].btnMode[i]) {
               lv_obj_clear_state(profileScreens[profile].btnMode[i], LV_STATE_CHECKED);
            }
         }
         if (profileScreens[profile].btnMode[mode]) {
            lv_obj_add_state(profileScreens[profile].btnMode[mode], LV_STATE_CHECKED);
         }
         refreshProfileVisibility(profile);
      }

      void loadProfile(int profile) {
         ProfileScreen &screen = profileScreens[profile];
         lv_slider_set_value(screen.sliderFan, profiles[profile].fanLevel, LV_ANIM_OFF);
         updateProfileTemp(profile);
         updateProfileFan(profile);
         selectProfileMode(profile, profiles[profile].mode);
      }

   public:
      static TimeProgramUI& getInstance() {
         return instance();
      }

      void init(tp_close_cb_t cb) {
         closeCb = cb;
         buildMenuScreen();
         buildWeekScreen();
         buildProfileScreen(0);
         buildProfileScreen(1);
      }

      void showMenu() {
         lv_scr_load(menuScreen);
      }

      void showWeek() {
         lv_scr_load(weekScreen);
      }

      void showProfile(int profile) {
         loadProfile(profile);
         lv_scr_load(profileScreens[profile].screen);
      }
};

