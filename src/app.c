/* Screen management, input, transitions and dialogs. */
#include "app.h"
#include "ui.h"
#include <math.h>
#include <string.h>

App app;
Progress progress;
CustomMeta custom_meta[CUSTOM_SLOTS];

/* Geometry Dash's icon colours (the classic 2.1 set). */
static const uint8_t icon_colors[42][3] = {
  {125, 255, 0}, {0, 255, 0}, {0, 255, 125}, {0, 255, 255}, {0, 125, 255}, {0, 0, 255}, {125, 0, 255},
  {255, 0, 255}, {255, 0, 125}, {255, 0, 0}, {255, 125, 0}, {255, 255, 0}, {255, 255, 255}, {185, 0, 255},
  {255, 185, 0}, {0, 0, 0}, {0, 200, 255}, {175, 175, 175}, {90, 90, 90}, {255, 125, 125}, {0, 175, 75},
  {0, 125, 125}, {0, 75, 175}, {75, 0, 175}, {125, 0, 125}, {175, 0, 75}, {175, 75, 0}, {125, 125, 0},
  {75, 175, 0}, {255, 75, 0}, {150, 50, 0}, {150, 100, 0}, {100, 150, 0}, {0, 150, 100}, {0, 100, 150},
  {100, 0, 150}, {150, 0, 100}, {150, 0, 0}, {0, 150, 0}, {0, 0, 150}, {125, 255, 175}, {125, 125, 255}};
color_t icon_color(int i) {
  if (i < 0 || i >= 42) i = 0;
  return rgb(icon_colors[i][0], icon_colors[i][1], icon_colors[i][2]);
}
color_t app_p1(void) { return icon_color(progress.color1); }
color_t app_p2(void) { return icon_color(progress.color2); }

static uint32_t rng = 0x9e3779b9u;
float frand(void) {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return (rng >> 8) * (1.0f / 16777216.0f);
}

uint32_t app_hit(uint32_t mask) { return app.hit & mask; }
bool app_accept(void) { return (app.hit & (K_OK | K_EXE)) != 0; }
void app_begin_press(int id) { app.press_id = id; app.press_t = 0; }

void app_notice(const char *s) {
  strncpy(app.notice, s, sizeof(app.notice) - 1);
  app.notice[sizeof(app.notice) - 1] = 0;
  app.notice_t = 2.5f;
}

bool app_save_progress(void) { return save_write_progress(&progress); }

static void enter(Screen s) {
  app.screen = s;
  app.t = 0;
  app.sel = 0;
  app.row = 0;
  app.press_id = -1;
  if (s == SCR_PLAY && !play_start(app.level, app.practice, app.testing)) {
    app_notice("LEVEL DATA INVALID");
    s = app.screen = app.testing ? SCR_EDITOR : app.level < LEVEL_COUNT ? SCR_SELECT : SCR_CREATOR;
  }
  if (s == SCR_EDITOR) editor_open(app.slot);
  else if (s != SCR_PLAY) menu_enter(s);
}

void app_go(Screen s) {
  if (app.fading_out) return;
  app.next = s;
  app.fading_out = true;
}

void app_init(void) {
  memset(&app, 0, sizeof(app));
  app.running = true;
  app.press_id = -1;
  rng ^= platform_random() | 1;
  if (!save_load_all(&progress, custom_meta)) {
    size_t size;
    if (platform_storage(&size)) app_notice("SAVE DATA UNREADABLE");
  }
  app.level = progress.last_level < LEVEL_COUNT ? progress.last_level : 0;
  app.select_page = app.level;
  app.select_scroll = (float)app.level;
  enter(SCR_LOADING);
  app.fade = 1;
}

static void quit(void) {
  if (app.screen == SCR_PLAY) play_flush();
  if (app.screen == SCR_EDITOR) editor_flush();
  app_save_progress();
  app.running = false;
}

/* ------------------------------------------------------------ dialogs */

static float dialog_t;
static int dialog_sel, saved_sel;
static void dialog_open(int kind, const char *title, const char *text) {
  app.dialog = kind;
  app.dialog_title = title;
  app.dialog_text = text;
  dialog_t = 0;
  dialog_sel = 0;
  saved_sel = app.sel;
}
static void dialog_close(void) {
  app.dialog = DLG_NONE;
  app.sel = saved_sel;
}
static void dialog_tick(void) {
  if (app.dialog == DLG_QUIT) {
    if (app_hit(K_LEFT | K_RIGHT)) dialog_sel ^= 1;
    if (app_hit(K_BACK)) { dialog_close(); return; }
    if (app_accept()) {
      if (dialog_sel == 1) quit();
      dialog_close();
    }
  } else if (app_accept() || app_hit(K_BACK)) {
    dialog_close();
  }
}
static void dialog_draw(void) {
  float s = ui_ease_elastic_out(dialog_t / 0.5f, 0.6f);
  ui_dim((unsigned)(fminf(dialog_t / 0.14f, 1) * 110));
  int w = (int)(250 * s), h = (int)(128 * s);
  if (w < 8 || h < 8) return;
  ui_window_brown(160 - w / 2, 120 - h / 2, w, h);
  if (s < 0.85f) return;
  ui_gold(FONT_BIG, 160, 84, app.dialog_title);
  /* message, split on '\n' */
  const char *p = app.dialog_text;
  int y = 110;
  char line[48];
  while (p && *p) {
    int n = 0;
    while (*p && *p != '\n' && n < 47) line[n++] = *p++;
    line[n] = 0;
    if (*p == '\n') p++;
    ui_title(FONT_SMALL, 160, y, line);
    y += 13;
  }
  if (app.dialog == DLG_QUIT) {
    ui_text_button(118, 154, 76, 26, "CANCEL", BTN_GREEN, dialog_sel == 0 ? 1.1f : 1.0f);
    ui_text_button(202, 154, 60, 26, "YES", BTN_GREEN, dialog_sel == 1 ? 1.1f : 1.0f);
  } else {
    ui_text_button(160, 154, 60, 26, "OK", BTN_GREEN, 1.1f);
  }
}
void app_dialog_quit(void) { dialog_open(DLG_QUIT, "QUIT GAME", "ARE YOU SURE YOU\nWANT TO QUIT?"); }
void app_dialog_info(const char *title, const char *text) { dialog_open(DLG_INFO, title, text); }

/* ------------------------------------------------------------ main hooks */

void app_tick(uint32_t keys) {
  app.ticks++;
  uint32_t hit = keys & ~app.prev_keys;
  const uint32_t arrows = K_LEFT | K_RIGHT | K_UP | K_DOWN;
  static uint32_t held_since;
  if (keys & arrows) {
    if ((keys & arrows) != (app.prev_keys & arrows)) {
      app.repeat_at = app.ticks + 96;
      held_since = app.ticks;
    } else if (app.ticks >= app.repeat_at) {
      hit |= keys & arrows;
      /* 10 repeats per second, then 24 after holding for a while */
      app.repeat_at = app.ticks + (app.ticks - held_since > 360 ? 10 : 24);
    }
  }
  app.prev_keys = keys;
  app.keys = keys;
  app.hit = hit;
  if (hit & K_HOME) { quit(); return; }
  if (app.fading_out) { app.hit = 0; if (app.screen != SCR_PLAY) return; }
  if (app.dialog) { dialog_tick(); return; }
  switch (app.screen) {
    case SCR_PLAY: play_tick(); break;
    case SCR_EDITOR: editor_tick(); break;
    case SCR_CREATOR: creator_tick(); break;
    default: menu_tick(); break;
  }
}

void app_frame(float dt) {
  app.t += dt;
  if (app.press_id >= 0) app.press_t += dt;
  if (app.notice_t > 0) app.notice_t -= dt;
  if (app.dialog) dialog_t += dt;
  if (app.fading_out) {
    app.fade += dt / 0.25f;
    if (app.fade >= 1) {
      app.fade = 1;
      app.fading_out = false;
      enter(app.next);
      dt = 0;   /* the new screen still prepares its first frame below */
    }
  } else if (app.fade > 0) {
    app.fade -= dt / 0.25f;
    if (app.fade < 0) app.fade = 0;
  }
  switch (app.screen) {
    case SCR_PLAY: play_frame(dt); break;
    case SCR_EDITOR: editor_frame(dt); break;
    default: menu_frame(dt); break;
  }
}

void app_draw(void) {
  switch (app.screen) {
    case SCR_PLAY: play_draw(); break;
    case SCR_EDITOR: editor_draw(); break;
    case SCR_CREATOR: creator_draw(); break;
    default: menu_draw(); break;
  }
  if (app.dialog) dialog_draw();
  if (app.notice_t > 0) {
    unsigned a = app.notice_t < 0.3f ? (unsigned)(app.notice_t / 0.3f * 256) : 256;
    int w = gfx_text_width(FONT_SMALL, app.notice) + 16;
    gfx_round_rect(160 - w / 2, 196, w, 16, 5, 0, a * 3 / 5);
    gfx_text_center(FONT_SMALL, 160, 208, app.notice, 0xffff, 0xffff, a);
  }
  if (progress.options & OPT_FPS) {
    char buf[12];
    int n = gfx_format_uint(buf, app.fps);
    memcpy(buf + n, " FPS", 5);
    gfx_text(FONT_SMALL, 4, 236, buf, 0xffff, 0xffff, 220);
  }
  if (app.fade > 0) gfx_scale_rect(0, 0, GFX_W, GFX_H, (unsigned)((1 - app.fade) * 256));
}
