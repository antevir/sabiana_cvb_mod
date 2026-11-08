#include <cstdio>
#include <cstdint>
#include <cstring>

#include <lvgl.h>
#include "settings.hpp"
#include "main_screen.hpp"
#include "fan_cycle_screen.hpp"

static constexpr int kWidth = 320;
static constexpr int kHeight = 240;

static uint8_t s_draw_buf[kWidth * kHeight * 2];
static uint32_t s_tick_ms = 0;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
   LV_UNUSED(area);
   LV_UNUSED(px_map);
   lv_display_flush_ready(disp);
}

static uint32_t tick_cb(void)
{
   return s_tick_ms;
}

// Screenshot only. The panel backlight makes the same pixels look brighter,
// so scale them here. A straight multiply keeps the hue: blue stays blue,
// and anything already light (the gray text) clips to white.
static uint8_t backlight(uint8_t channel)
{
   const int lifted = (int)channel * 22 / 10;
   return (uint8_t)(lifted > 255 ? 255 : lifted);
}

static bool write_bmp(const char *path, const uint16_t *rgb565, int w, int h)
{
   const int row_stride = (w * 3 + 3) & ~3;
   const int pixel_bytes = row_stride * h;
   const int file_size = 54 + pixel_bytes;

   FILE *f = fopen(path, "wb");
   if (f == nullptr) {
      return false;
   }

   uint8_t hdr[54];
   memset(hdr, 0, sizeof(hdr));
   hdr[0] = 'B';
   hdr[1] = 'M';
   hdr[2] = (uint8_t)file_size;
   hdr[3] = (uint8_t)(file_size >> 8);
   hdr[4] = (uint8_t)(file_size >> 16);
   hdr[5] = (uint8_t)(file_size >> 24);
   hdr[10] = 54;
   hdr[14] = 40;
   hdr[18] = (uint8_t)w;
   hdr[19] = (uint8_t)(w >> 8);
   hdr[20] = (uint8_t)(w >> 16);
   hdr[21] = (uint8_t)(w >> 24);
   hdr[22] = (uint8_t)h;
   hdr[23] = (uint8_t)(h >> 8);
   hdr[24] = (uint8_t)(h >> 16);
   hdr[25] = (uint8_t)(h >> 24);
   hdr[26] = 1;
   hdr[28] = 24;
   fwrite(hdr, 1, sizeof(hdr), f);

   uint8_t row[320 * 3 + 4];
   for (int y = h - 1; y >= 0; --y) {
      memset(row, 0, (size_t)row_stride);
      for (int x = 0; x < w; ++x) {
         const uint16_t c = rgb565[y * w + x];
         const uint8_t r = backlight((uint8_t)(((c >> 11) & 0x1F) * 255 / 31));
         const uint8_t g = backlight((uint8_t)(((c >> 5) & 0x3F) * 255 / 63));
         const uint8_t b = backlight((uint8_t)((c & 0x1F) * 255 / 31));
         row[x * 3 + 0] = b;
         row[x * 3 + 1] = g;
         row[x * 3 + 2] = r;
      }
      fwrite(row, 1, (size_t)row_stride, f);
   }

   fclose(f);
   return true;
}

static void noop(void) {}

static void render_frames(void)
{
   for (int i = 0; i < 8; ++i) {
      s_tick_ms += 33;
      lv_timer_handler();
   }
}

int main(int argc, char **argv)
{
   const char *out_path = (argc > 1) ? argv[1] : "main_screen.bmp";
   const char *which = (argc > 2) ? argv[2] : "main";

   lv_init();
   lv_tick_set_cb(tick_cb);

   lv_display_t *disp = lv_display_create(kWidth, kHeight);
   lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
   lv_display_set_flush_cb(disp, flush_cb);
   lv_display_set_buffers(disp, s_draw_buf, nullptr, sizeof(s_draw_buf),
                          LV_DISPLAY_RENDER_MODE_FULL);

   Settings settings;
   if (strcmp(which, "fan_cycle") == 0) {
      settings.setFanCycleEn(1);
      settings.setFanCycleFan(1);
      settings.setFanCyclePeriod(60);
      settings.setFanCycleDur(5);
      FanCycleScreen &ui = FanCycleScreen::getInstance();
      ui.init(&settings, noop);
      ui.show();
   } else {
      MainScreen &ui = MainScreen::getInstance();
      ui.init(&settings, noop, noop, noop);
      ui.setAirTemp(22.4f);
      ui.setHWTemp(38.0f);
      ui.show();
      ui.update();
   }

   render_frames();

   if (!write_bmp(out_path, reinterpret_cast<const uint16_t *>(s_draw_buf), kWidth, kHeight)) {
      fprintf(stderr, "Failed to write %s\n", out_path);
      return 1;
   }

   printf("Wrote %s (%dx%d) screen=%s\n", out_path, kWidth, kHeight, which);
   return 0;
}
