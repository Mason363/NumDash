#ifndef NUMDASH_UI_H
#define NUMDASH_UI_H
#include "gfx.h"
#include <stdbool.h>

/* Widgets in the style of Geometry Dash's menus. */
enum { BTN_GREEN, BTN_CYAN, BTN_PINK, BTN_GRAY };
#define GOLD_TOP rgb(255, 255, 140)
#define GOLD_BOTTOM rgb(255, 170, 0)

void ui_gradient_bg(color_t c);
void ui_dim(unsigned alpha256);
void ui_window_brown(int x, int y, int w, int h);
void ui_window_blue(int x, int y, int w, int h);
void ui_window_dark(int x, int y, int w, int h, unsigned alpha256);
void ui_window_navy(int x, int y, int w, int h);
void ui_text_button(int cx, int cy, int w, int h, const char *s, int style, float scale);
void ui_progress_bar(int cx, int cy, int w, int h, int percent, color_t fill, bool label);
void ui_checkbox(int cx, int cy, bool on, float scale);
void ui_top_bar(int cy, bool center);
void ui_side_art(void);
void ui_nav_dots(int cx, int cy, int n, int cur);
/* Sprite drawn at scale; exact 1:1 when scale is 1. */
void ui_sprite(int spr, float cx, float cy, float scale, unsigned alpha);
void ui_title(int font, int cx, int baseline, const char *s);
void ui_gold(int font, int cx, int baseline, const char *s);
float ui_ease_elastic_out(float t, float period);
float ui_ease_bounce_out(float t);
#endif
