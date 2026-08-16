#pragma once

#include <lvgl.h>
#include <stdio.h>

#include "fan_cycle_config.hpp"
#include "settings.hpp"

#ifdef __cplusplus
extern "C" {
#endif
LV_IMAGE_DECLARE(img_blue_bg);
LV_IMAGE_DECLARE(img_fan);
#ifdef __cplusplus
}
#endif

#define FC_MARGIN_X 20
#define FC_SLIDER_X 95
#define FC_SLIDER_W 130
#define FC_ENABLE_Y 46
#define FC_FAN_Y 84
#define FC_PERIOD_Y 138
#define FC_DUR_Y 190

typedef void (*fan_cycle_close_cb_t)(void);

class FanCycleScreen {
   private:
      Settings *settings = nullptr;
      fan_cycle_close_cb_t closeCb = nullptr;
      lv_obj_t *screen = nullptr;
      lv_obj_t *cbEnable = nullptr;
      lv_obj_t *imgFan = nullptr;
      lv_obj_t *sliderFan = nullptr;
      lv_obj_t *lblFan = nullptr;
      lv_obj_t *lblPeriod = nullptr;
      lv_obj_t *sliderPeriod = nullptr;
      lv_obj_t *lblPeriodVal = nullptr;
      lv_obj_t *lblDuration = nullptr;
      lv_obj_t *sliderDuration = nullptr;
      lv_obj_t *lblDurationVal = nullptr;

      static const char *fanText(int level) {
         static const char *text[] = { "Auto", "Low", "Mid", "High" };
         if (level < 0) level = 0;
         if (level > 3) level = 3;
         return text[level];
      }

      void setControlsEnabled(bool enabled) {
         lv_obj_t *objs[] = {
            imgFan, sliderFan, lblFan,
            lblPeriod, sliderPeriod, lblPeriodVal,
            lblDuration, sliderDuration, lblDurationVal
         };
         for (lv_obj_t *obj : objs) {
            if (!obj) continue;
            if (enabled) {
               lv_obj_remove_state(obj, LV_STATE_DISABLED);
            } else {
               lv_obj_add_state(obj, LV_STATE_DISABLED);
            }
         }
      }

      void updateFanLabel() {
         if (!lblFan || !sliderFan) return;
         lv_label_set_text(lblFan, fanText(lv_slider_get_value(sliderFan)));
         lv_obj_align(lblFan, LV_ALIGN_TOP_RIGHT, -FC_MARGIN_X, FC_FAN_Y + 6);
      }

      int selectedPeriodMinutes() const {
         int idx = FanCycleConfig::clamp((int)lv_slider_get_value(sliderPeriod),
                                         0, FanCycleConfig::PERIOD_COUNT - 1);
         return FanCycleConfig::periodValues()[idx];
      }

      int selectedDurationMinutes() const {
         int idx = FanCycleConfig::clamp((int)lv_slider_get_value(sliderDuration),
                                         0, FanCycleConfig::DUR_COUNT - 1);
         return FanCycleConfig::durationValues()[idx];
      }

      void updatePeriodLabel() {
         if (!lblPeriodVal || !sliderPeriod) return;
         char buf[16];
         FanCycleConfig::formatMinutes(selectedPeriodMinutes(), buf, sizeof(buf));
         lv_label_set_text(lblPeriodVal, buf);
         lv_obj_align(lblPeriodVal, LV_ALIGN_TOP_RIGHT, -FC_MARGIN_X, FC_PERIOD_Y + 6);
      }

      void updateDurationLabel() {
         if (!lblDurationVal || !sliderDuration) return;
         char buf[16];
         FanCycleConfig::formatMinutes(selectedDurationMinutes(), buf, sizeof(buf));
         lv_label_set_text(lblDurationVal, buf);
         lv_obj_align(lblDurationVal, LV_ALIGN_TOP_RIGHT, -FC_MARGIN_X, FC_DUR_Y + 6);
      }

      void clampDurationToPeriod() {
         if (!sliderPeriod || !sliderDuration) return;
         int period = selectedPeriodMinutes();
         int maxIdx = FanCycleConfig::maxDurationIndexForPeriod(period);
         int idx = FanCycleConfig::clamp((int)lv_slider_get_value(sliderDuration),
                                         0, FanCycleConfig::DUR_COUNT - 1);
         if (idx > maxIdx) {
            lv_slider_set_value(sliderDuration, maxIdx, LV_ANIM_OFF);
            if (settings) {
               settings->setFanCycleDur(FanCycleConfig::durationValues()[maxIdx]);
            }
            updateDurationLabel();
         }
      }

      void loadFromSettings() {
         if (!settings) return;
         bool enabled = settings->getFanCycleEn() != 0;
         if (cbEnable) {
            if (enabled) {
               lv_obj_add_state(cbEnable, LV_STATE_CHECKED);
            } else {
               lv_obj_remove_state(cbEnable, LV_STATE_CHECKED);
            }
         }
         if (sliderFan) {
            lv_slider_set_value(sliderFan, FanCycleConfig::clamp(settings->getFanCycleFan(),
                                                                 FanCycleConfig::FAN_MIN,
                                                                 FanCycleConfig::FAN_MAX), LV_ANIM_OFF);
         }
         if (sliderPeriod) {
            lv_slider_set_value(sliderPeriod, FanCycleConfig::periodIndex(settings->getFanCyclePeriod()), LV_ANIM_OFF);
         }
         if (sliderDuration) {
            lv_slider_set_value(sliderDuration, FanCycleConfig::durationIndex(settings->getFanCycleDur()), LV_ANIM_OFF);
         }
         updateFanLabel();
         updatePeriodLabel();
         updateDurationLabel();
         clampDurationToPeriod();
         setControlsEnabled(enabled);
      }

      static void evtBack(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
         }
         FanCycleScreen &ui = getInstance();
         if (ui.closeCb) {
            ui.closeCb();
         }
      }

      static void evtEnable(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            return;
         }
         FanCycleScreen &ui = getInstance();
         bool enabled = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
         if (ui.settings) {
            ui.settings->setFanCycleEn(enabled ? 1 : 0);
         }
         ui.setControlsEnabled(enabled);
      }

      static void evtFan(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            return;
         }
         FanCycleScreen &ui = getInstance();
         int level = FanCycleConfig::clamp((int)lv_slider_get_value(lv_event_get_target_obj(e)),
                                           FanCycleConfig::FAN_MIN, FanCycleConfig::FAN_MAX);
         if (ui.settings) {
            ui.settings->setFanCycleFan(level);
         }
         ui.updateFanLabel();
      }

      static void evtPeriod(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            return;
         }
         FanCycleScreen &ui = getInstance();
         int period = ui.selectedPeriodMinutes();
         if (ui.settings) {
            ui.settings->setFanCyclePeriod(period);
         }
         ui.updatePeriodLabel();
         ui.clampDurationToPeriod();
      }

      static void evtDuration(lv_event_t * e) {
         if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            return;
         }
         FanCycleScreen &ui = getInstance();
         ui.clampDurationToPeriod();
         int duration = ui.selectedDurationMinutes();
         if (ui.settings) {
            ui.settings->setFanCycleDur(duration);
         }
         ui.updateDurationLabel();
      }

   public:
      static FanCycleScreen& getInstance() {
         static FanCycleScreen instance;
         return instance;
      }

      void init(Settings *settings, fan_cycle_close_cb_t closeCb) {
         static lv_style_t styleBg;
         static lv_style_t styleTitle;
         static lv_style_t styleText;
         static lv_style_t styleValue;
         static lv_style_t styleBtn;

         this->settings = settings;
         this->closeCb = closeCb;

         lv_style_init(&styleBg);
         lv_style_set_bg_color(&styleBg, lv_color_hex(0x000000));

         lv_style_init(&styleTitle);
         lv_style_set_text_font(&styleTitle, &lv_font_montserrat_20);
         lv_style_set_text_color(&styleTitle, lv_color_hex(0xFFFFFF));

         lv_style_init(&styleText);
         lv_style_set_text_font(&styleText, &lv_font_montserrat_14);
         lv_style_set_text_color(&styleText, lv_color_hex(0xBBBBBB));

         lv_style_init(&styleValue);
         lv_style_set_text_font(&styleValue, &lv_font_montserrat_14);
         lv_style_set_text_color(&styleValue, lv_color_hex(0xEEEEEE));

         lv_style_init(&styleBtn);
         lv_style_set_text_font(&styleBtn, &lv_font_montserrat_16);
         lv_style_set_text_color(&styleBtn, lv_color_hex(0xFFFFFF));
         lv_style_set_bg_color(&styleBtn, lv_color_hex(0x3C3C7A));
         lv_style_set_bg_grad_color(&styleBtn, lv_color_hex(0x2A2A58));
         lv_style_set_bg_grad_dir(&styleBtn, LV_GRAD_DIR_VER);

         screen = lv_obj_create(NULL);
         lv_obj_add_style(screen, &styleBg, 0);
         lv_obj_t *bg = lv_image_create(screen);
         lv_image_set_src(bg, &img_blue_bg);

         lv_obj_t *title = lv_label_create(screen);
         lv_obj_add_style(title, &styleTitle, 0);
         lv_label_set_text(title, "Fan cycle");
         lv_obj_set_pos(title, 12, 8);

         lv_obj_t *btnBack = lv_btn_create(screen);
         lv_obj_add_style(btnBack, &styleBtn, 0);
         lv_obj_set_size(btnBack, 70, 30);
         lv_obj_set_pos(btnBack, 232, 8);
         lv_obj_t *lblBack = lv_label_create(btnBack);
         lv_label_set_text(lblBack, "Back");
         lv_obj_center(lblBack);
         lv_obj_add_event_cb(btnBack, evtBack, LV_EVENT_CLICKED, NULL);

         cbEnable = lv_checkbox_create(screen);
         lv_checkbox_set_text(cbEnable, "Enable");
         lv_obj_set_pos(cbEnable, FC_MARGIN_X, FC_ENABLE_Y);
         lv_obj_set_style_text_font(cbEnable, &lv_font_montserrat_16, 0);
         lv_obj_set_style_text_color(cbEnable, lv_color_hex(0xFFFFFF), 0);
         lv_obj_set_style_bg_color(cbEnable, lv_color_hex(0x2A2A4A), LV_PART_INDICATOR);
         lv_obj_set_style_bg_opa(cbEnable, LV_OPA_COVER, LV_PART_INDICATOR);
         lv_obj_set_style_border_color(cbEnable, lv_color_hex(0x8888AA), LV_PART_INDICATOR);
         lv_obj_set_style_border_width(cbEnable, 2, LV_PART_INDICATOR);
         lv_obj_set_style_radius(cbEnable, 4, LV_PART_INDICATOR);
         lv_obj_set_style_bg_color(cbEnable, lv_color_hex(0x5555AA), LV_PART_INDICATOR | LV_STATE_CHECKED);
         lv_obj_set_style_border_color(cbEnable, lv_color_hex(0xAAAAFF), LV_PART_INDICATOR | LV_STATE_CHECKED);
         lv_obj_add_event_cb(cbEnable, evtEnable, LV_EVENT_VALUE_CHANGED, NULL);

         imgFan = lv_image_create(screen);
         lv_image_set_src(imgFan, &img_fan);
         lv_obj_set_pos(imgFan, FC_MARGIN_X + 5, FC_FAN_Y);

         sliderFan = lv_slider_create(screen);
         lv_slider_set_range(sliderFan, FanCycleConfig::FAN_MIN, FanCycleConfig::FAN_MAX);
         lv_obj_set_pos(sliderFan, FC_SLIDER_X, FC_FAN_Y + 12);
         lv_obj_set_width(sliderFan, FC_SLIDER_W);
         lv_obj_add_event_cb(sliderFan, evtFan, LV_EVENT_VALUE_CHANGED, NULL);

         lblFan = lv_label_create(screen);
         lv_obj_add_style(lblFan, &styleValue, 0);

         lblPeriod = lv_label_create(screen);
         lv_obj_add_style(lblPeriod, &styleText, 0);
         lv_label_set_text(lblPeriod, "Period");
         lv_obj_set_pos(lblPeriod, FC_MARGIN_X, FC_PERIOD_Y + 6);

         sliderPeriod = lv_slider_create(screen);
         lv_slider_set_range(sliderPeriod, 0, FanCycleConfig::PERIOD_COUNT - 1);
         lv_obj_set_pos(sliderPeriod, FC_SLIDER_X, FC_PERIOD_Y + 12);
         lv_obj_set_width(sliderPeriod, FC_SLIDER_W);
         lv_obj_add_event_cb(sliderPeriod, evtPeriod, LV_EVENT_VALUE_CHANGED, NULL);

         lblPeriodVal = lv_label_create(screen);
         lv_obj_add_style(lblPeriodVal, &styleValue, 0);

         lblDuration = lv_label_create(screen);
         lv_obj_add_style(lblDuration, &styleText, 0);
         lv_label_set_text(lblDuration, "Duration");
         lv_obj_set_pos(lblDuration, FC_MARGIN_X, FC_DUR_Y + 6);

         sliderDuration = lv_slider_create(screen);
         lv_slider_set_range(sliderDuration, 0, FanCycleConfig::DUR_COUNT - 1);
         lv_obj_set_pos(sliderDuration, FC_SLIDER_X, FC_DUR_Y + 12);
         lv_obj_set_width(sliderDuration, FC_SLIDER_W);
         lv_obj_add_event_cb(sliderDuration, evtDuration, LV_EVENT_VALUE_CHANGED, NULL);

         lblDurationVal = lv_label_create(screen);
         lv_obj_add_style(lblDurationVal, &styleValue, 0);

         loadFromSettings();
      }

      void show() {
         loadFromSettings();
         lv_scr_load(screen);
      }
};
