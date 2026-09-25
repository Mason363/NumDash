#include "ui.h"
#include <math.h>

void ui_gradient_bg(color_t c) {
  /* GJ_gradientBG: white at the top fading to 44% grey, tinted */
  gfx_vgrad(0, 0, GFX_W, GFX_H, c, c_scale(c, 113));
}

void ui_dim(unsigned alpha) { gfx_blend(0, 0, GFX_W, GFX_H, 0, alpha); }

/* Rounded rectangle with a vertical gradient (corners without AA). */
static void round_vgrad(int x, int y, int w, int h, int r, color_t top, color_t bottom) {
  int j0 = y > gfx_y0 ? y : gfx_y0, j1 = y + h < gfx_y1 ? y + h : gfx_y1;
  for (int j = j0; j < j1; j++) {
    int k = j - y, d = k < r ? r - k : (k >= h - r ? k - (h - r - 1) : 0), in = 0;
    if (d) {
      float dy = d - .5f;
      in = (int)(r - sqrtf((float)(r * r) - dy * dy) + .5f);
    }
    gfx_fill(x + in, j, w - 2 * in, 1, c_mix(top, bottom, (unsigned)(k * 256 / (h > 1 ? h - 1 : 1))));
  }
}

static void box(int x, int y, int w, int h, int r, color_t body, color_t hi, color_t lo) {
  gfx_round_rect(x, y, w, h, r, 0, 256);
  gfx_round_rect(x + 2, y + 2, w - 4, h - 4, r - 1, lo, 256);
  gfx_round_rect(x + 2, y + 2, w - 4, h - 7, r - 1, body, 256);
  gfx_round_rect(x + 2, y + 2, w - 4, 4, r - 1 < 2 ? 2 : r - 1, hi, 256);
  gfx_blend(x + r, y + 5, w - 2 * r, 2, hi, 110);
}

void ui_window_brown(int x, int y, int w, int h) { box(x, y, w, h, 7, rgb(153, 85, 51), rgb(187, 118, 60), rgb(85, 51, 17)); }
void ui_window_blue(int x, int y, int w, int h) { box(x, y, w, h, 7, rgb(51, 68, 153), rgb(70, 72, 187), rgb(17, 17, 85)); }
void ui_window_dark(int x, int y, int w, int h, unsigned alpha) { gfx_round_rect(x, y, w, h, 6, 0, alpha); }

void ui_window_navy(int x, int y, int w, int h) {
  gfx_round_rect(x, y, w, h, 9, rgb(90, 92, 96), 256);
  gfx_round_rect(x + 1, y + 1, w - 2, h - 2, 8, rgb(215, 215, 228), 256);
  gfx_round_rect(x + 3, y + 3, w - 6, h - 6, 7, rgb(140, 142, 148), 256);
  gfx_round_rect(x + 4, y + 4, w - 8, h - 8, 6, rgb(0, 17, 34), 256);
  gfx_round_rect(x + 6, y + 6, w - 12, h - 12, 5, rgb(0, 36, 70), 256);
  /* bolts in the corners */
  const int bx[2] = {x + 6, x + w - 6}, by[2] = {y + 6, y + h - 6};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++) {
      gfx_disc(bx[i] * 16, by[j] * 16, 6 * 16, rgb(70, 72, 76), 256, BLEND_NORMAL);
      gfx_disc(bx[i] * 16, by[j] * 16, 5 * 16, rgb(205, 205, 218), 256, BLEND_NORMAL);
      gfx_disc(bx[i] * 16 - 16, by[j] * 16 - 16, 2 * 16, 0xffff, 200, BLEND_NORMAL);
    }
}

void ui_text_button(int cx, int cy, int w, int h, const char *s, int style, float scale) {
  const int font = h >= 24 ? FONT_BIG : FONT_SMALL;
  static const uint8_t cols[4][2][3] = {
    {{190, 255, 80}, {60, 170, 0}}, {{130, 255, 255}, {0, 160, 210}}, {{255, 150, 255}, {210, 40, 210}}, {{200, 200, 200}, {110, 110, 110}}};
  w = (int)(w * scale);
  h = (int)(h * scale);
  int x = cx - w / 2, y = cy - h / 2;
  gfx_round_rect(x + 1, y + 2, w, h, 6, 0, 110);
  gfx_round_rect(x, y, w, h, 6, 0, 256);
  color_t top = rgb(cols[style][0][0], cols[style][0][1], cols[style][0][2]);
  color_t bot = rgb(cols[style][1][0], cols[style][1][1], cols[style][1][2]);
  round_vgrad(x + 2, y + 2, w - 4, h - 4, 4, top, bot);
  gfx_blend(x + 5, y + 3, w - 10, 2, 0xffff, 90);
  int base = cy + fonts[font].cap / 2;
  gfx_text_center(font, cx, base, s, GOLD_TOP, GOLD_BOTTOM, 256);
}

void ui_progress_bar(int cx, int cy, int w, int h, int percent, color_t fill, bool label) {
  int x = cx - w / 2, y = cy - h / 2, r = h / 2;
  gfx_round_rect(x, y, w, h, r, 0, 128);
  if (percent > 0) {
    int fw = (w - 4) * (percent > 100 ? 100 : percent) / 100;
    if (fw < h - 4) fw = h - 4;
    gfx_round_rect(x + 2, y + 2, fw, h - 4, r - 2, fill, 256);
  }
  if (label) {
    char buf[8];
    int n = gfx_format_uint(buf, (unsigned long)percent);
    buf[n] = '%';
    buf[n + 1] = 0;
    gfx_text_center(FONT_SMALL, cx, cy + fonts[FONT_SMALL].cap / 2, buf, 0xffff, 0xffff, 256);
  }
}

void ui_checkbox(int cx, int cy, bool on, float scale) { ui_sprite(on ? SPR_CHECK_ON : SPR_CHECK_OFF, (float)cx, (float)cy, scale, 256); }

void ui_top_bar(int cy, bool center) {
  const int x0 = 46, x1 = 274, y0 = cy - 14, y1 = cy + 7;
  color_t lime_t = rgb(190, 242, 72), lime_b = rgb(80, 150, 30), cyan_t = rgb(120, 250, 250), cyan_b = rgb(0, 170, 180);
  gfx_round_rect(x0 - 3, y0 - 3, x1 - x0 + 6, y1 - y0 + 5, 4, rgb(24, 75, 153), 200);
  gfx_round_rect(x0 - 2, y0 - 2, x1 - x0 + 4, y1 - y0 + 3, 3, rgb(242, 245, 250), 256);
  gfx_fill(x0, y0, x1 - x0, y1 - y0, 0);
  gfx_vgrad(x0 + 1, y0, x1 - x0 - 2, y1 - y0 - 1, lime_t, lime_b);
  /* cyan caps and the deeper centre block */
  gfx_fill(x0, y0, 16, y1 - y0 - 4, 0);
  gfx_vgrad(x0 + 1, y0, 14, y1 - y0 - 5, cyan_t, cyan_b);
  gfx_fill(x1 - 16, y0, 16, y1 - y0 - 4, 0);
  gfx_vgrad(x1 - 15, y0, 14, y1 - y0 - 5, cyan_t, cyan_b);
  int cxm = (x0 + x1) / 2;
  if (center) {
    gfx_fill(cxm - 18, y0, 36, y1 - y0 + 5, 0);
    gfx_vgrad(cxm - 17, y0, 34, y1 - y0 + 4, cyan_t, cyan_b);
  }
  gfx_blend(x0 + 1, y0, x1 - x0 - 2, 2, 0xffff, 80);
}

void ui_side_art(void) {
  gfx_sprite(SPR_SIDE_ART, 0, GFX_H, 0, 0xffff, 256, BLEND_NORMAL);
  gfx_sprite(SPR_SIDE_ART, GFX_W, GFX_H, XF_FLIPX, 0xffff, 256, BLEND_NORMAL);
}

void ui_nav_dots(int cx, int cy, int n, int cur) {
  const int step = 12;
  int x = cx - (n - 1) * step / 2;
  for (int i = 0; i < n; i++, x += step) {
    gfx_disc(x * 16, cy * 16, 4 * 16, 0, 200, BLEND_NORMAL);
    gfx_disc(x * 16, cy * 16, 3 * 16, i == cur ? rgb(0, 255, 255) : rgb(125, 125, 125), 256, BLEND_NORMAL);
  }
}

void ui_sprite(int spr, float cx, float cy, float scale, unsigned alpha) {
  if (fabsf(scale - 1) < 0.01f)
    gfx_sprite(spr, (int)floorf(cx + .5f), (int)floorf(cy + .5f), 0, 0xffff, alpha, BLEND_NORMAL);
  else
    gfx_sprite_ex(spr, (int)(cx * 16), (int)(cy * 16), 0, (int)(scale * 256), 0, 0xffff, alpha, BLEND_NORMAL);
}

void ui_title(int font, int cx, int baseline, const char *s) { gfx_text_center(font, cx, baseline, s, 0xffff, 0xffff, 256); }
void ui_gold(int font, int cx, int baseline, const char *s) { gfx_text_center(font, cx, baseline, s, GOLD_TOP, GOLD_BOTTOM, 256); }

float ui_ease_elastic_out(float t, float period) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  float s = period / 4;
  return powf(2, -10 * t) * sinf((t - s) * 6.2831853f / period) + 1;
}

float ui_ease_bounce_out(float t) {
  if (t >= 1) return 1;
  if (t < 1 / 2.75f) return 7.5625f * t * t;
  if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + .75f; }
  if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + .9375f; }
  t -= 2.625f / 2.75f;
  return 7.5625f * t * t + .984375f;
}
