/* Gameplay screen: attempts, death and respawn, practice checkpoints, the
 * pause menu, the new-best popup and the level complete sequence (end wall
 * rays, rings and fireworks, then the level complete window). Timings follow
 * Geometry Dash (and the gd3ds recreation). */
#include "app.h"
#include "ui.h"
#include <math.h>
#include <string.h>

enum { PAUSE_PRACTICE, PAUSE_RESUME, PAUSE_MENU, PAUSE_REPLAY };
enum { POP_NONE, POP_COMPLETE, POP_NEWBEST };
#define FIREWORK_TIME 2.0f
#define MENU_TIME 3.5f

static SceneOpts opts;
static float shake_t, shake_amp, shake_dx, shake_dy;
static bool shake_applied;
static float respawn_t;
static int respawn_phase;          /* counts hide/show half cycles left */
static float death_len;
static uint16_t jumps_counted;

/* level complete */
typedef struct { float delay, dur, h, alpha, angle; } Ray;
static Ray rays[8];
static float rays_t, rays_fade_t;
static bool rays_on, rays_fading;
static int rings_a, rings_b, fireworks;
static bool phase2;
static int popup;
static float popup_t;
static float end_t, leave_t;
static int end_sel, leave_action;
static bool leaving;
static uint8_t coins_run, coins_new;
static bool stars_new;
static int msg_index;

static const char *const complete_msgs[] = {
  "AWESOME!", "GOOD JOB!", "WELL DONE!", "IMPRESSIVE!", "AMAZING!", "INCREDIBLE!", "SKILLFUL!", "BRILLIANT!",
  "NOT BAD!", "WARP SPEED!", "CHALLENGE BREAKER!", "REFLEX MASTER!", "I AM SPEECHLESS...", "YOU ARE... THE ONE!",
  "HOW IS THIS POSSIBLE!?", "YOU BEAT ME..."};

static color_t white_if_black(color_t c) { return c == 0 ? 0xffff : c; }

static unsigned level_coins(void) { return app.L.coin_count; }

static void flush_jumps(void) {
  unsigned j = app.g.jumps >= jumps_counted ? app.g.jumps - jumps_counted : 0;
  jumps_counted = app.g.jumps;
  app.session_jumps += j;
  if (!app.testing) {
    progress.lv[app.level].jumps += j;
    progress.jumps += j;
  }
}

void play_flush(void) { flush_jumps(); }

static void unshake(void) {
  if (!shake_applied) return;
  app.g.cam_x -= shake_dx;
  app.g.cam_y -= shake_dy;
  shake_applied = false;
}

static void shake(float t, float amp) {
  if (progress.options & OPT_NOSHAKE) return;
  shake_t = t;
  shake_amp = amp;
}

static void begin_attempt(bool first) {
  unshake();
  if (!first && app.practice && app.checkpoint_count) {
    game_load_checkpoint(&app.g, &app.checkpoints[app.checkpoint_count - 1]);
  } else {
    game_start(&app.g, &app.L, first);
  }
  app.attempt++;
  app.attempt_x = first ? VIEW_W / 2 : app.g.p.x + 150;
  app.attempt_y = app.g.cam_y + 150;
  jumps_counted = app.g.jumps;
  if (!app.testing) {
    progress.lv[app.level].attempts++;
    progress.attempts++;
  }
  fx_reset();
  app.death_t = 0;
  app.auto_check_t = 0;
  respawn_phase = first ? 0 : 8;
  respawn_t = 0;
}

bool play_start(int level, bool practice, bool testing) {
  app.level = level;
  app.practice = practice;
  app.testing = testing;
  bool ok = level < LEVEL_COUNT ? level_load_builtin(&app.L, (unsigned)level) : editor_load_slot(level - LEVEL_COUNT, &app.L);
  if (!ok) return false;
  app.attempt = 0;
  app.session_jumps = 0;
  app.session_ticks = 0;
  app.checkpoint_count = 0;
  app.paused = false;
  app.end_menu = false;
  popup = POP_NONE;
  shake_t = 0;
  shake_applied = false;
  if (level < LEVEL_COUNT && !testing) progress.last_level = (uint8_t)level;
  begin_attempt(true);
  return true;
}

static void place_checkpoint(void) {
  if (app.g.dead || app.g.complete || app.g.ending) return;
  if (app.checkpoint_count == MAX_CHECKPOINTS) {
    memmove(app.checkpoints, app.checkpoints + 1, sizeof(Checkpoint) * (MAX_CHECKPOINTS - 1));
    app.checkpoint_count--;
  }
  game_save_checkpoint(&app.g, &app.checkpoints[app.checkpoint_count++]);
  app.auto_check_t = 0;
}

static void leave_level(void) {
  flush_jumps();
  if (!app.testing) app_save_progress();
  app_go(app.testing ? SCR_EDITOR : app.level < LEVEL_COUNT ? SCR_SELECT : SCR_CREATOR);
}

static void on_death(void) {
  int pct = (int)game_progress(&app.g);
  flush_jumps();
  death_len = 1.0f;
  if (!app.testing) {
    LevelStat *s = &progress.lv[app.level];
    if (app.practice) {
      if (pct > s->practice) s->practice = (uint8_t)pct;
    } else if (pct > s->normal) {
      s->normal = (uint8_t)pct;
      char buf[24] = "NEW BEST!\n";
      int n = gfx_format_uint(buf + 10, (unsigned long)pct);
      buf[10 + n] = '%';
      buf[11 + n] = 0;
      gfx_layer_build(FONT_HUGE, buf);
      gfx_layer_line_color(0, rgb(255, 255, 110), rgb(255, 140, 0));
      gfx_layer_line_color(1, 0xffff, 0xffff);
      popup = POP_NEWBEST;
      popup_t = 0;
      death_len = 1.4f;
    }
  }
  if (!app.practice) shake(0.15f, 1);
}

static void init_rays(void) {
  float angles[8];
  for (int i = 0; i < 8; i++) angles[i] = -135 + i * 11.25f;
  for (int i = 7; i > 0; i--) {
    int j = (int)(frand() * (i + 1));
    if (j > i) j = i;
    float t = angles[i]; angles[i] = angles[j]; angles[j] = t;
  }
  for (int i = 0; i < 8; i++) {
    rays[i].delay = i * 0.195f + 0.04f + 0.04f * (frand() * 2 - 1);
    rays[i].h = 30 + 20 * (frand() * 2 - 1);
    rays[i].dur = 0.18f + 0.04f * (frand() * 2 - 1);
    float a = 155 + 100 * (frand() * 2 - 1);
    rays[i].alpha = a < 0 ? 0 : a > 255 ? 255 : a;
    rays[i].angle = angles[i] + 11.25f * frand() + 90;
  }
  rays_t = 0;
  rays_on = true;
  rays_fading = false;
}

static void on_complete(void) {
  flush_jumps();
  app.complete_t = 0;
  phase2 = false;
  rings_a = rings_b = fireworks = 0;
  coins_run = coins_new = 0;
  for (unsigned k = 0; k < app.L.coin_count; k++)
    if (game_used(&app.g, app.L.coin_obj[k])) coins_run |= (uint8_t)(1u << k);
  stars_new = false;
  if (!app.testing) {
    LevelStat *s = &progress.lv[app.level];
    if (app.practice) {
      s->practice = 100;
    } else {
      stars_new = s->normal < 100 && app.level < LEVEL_COUNT && app.L.stars;
      s->normal = 100;
      coins_new = coins_run & (uint8_t)~s->coins;
      s->coins |= coins_run;
    }
    app_save_progress();
  }
  msg_index = (int)(frand() * (sizeof(complete_msgs) / sizeof(complete_msgs[0])));
  rays_on = false;
  if (app.practice) {
    app.complete_t = FIREWORK_TIME;
  } else {
    shake(FIREWORK_TIME, 3);
    init_rays();
  }
}

static void restart_session(void) {
  app.attempt = 0;
  app.session_jumps = 0;
  app.session_ticks = 0;
  app.checkpoint_count = 0;
  app.end_menu = false;
  popup = POP_NONE;
  begin_attempt(true);
}

/* ------------------------------------------------------------ input */

static void pause_tick(void) {
  if (app_hit(K_LEFT)) app.sel = (app.sel + 3) % 4;
  if (app_hit(K_RIGHT)) app.sel = (app.sel + 1) % 4;
  if (app_hit(K_BACK)) { app.paused = false; return; }
  if (!app_accept()) return;
  switch (app.sel) {
    case PAUSE_PRACTICE:
      flush_jumps();
      app.practice = !app.practice;
      app.checkpoint_count = 0;
      app.paused = false;
      begin_attempt(false);
      break;
    case PAUSE_RESUME: app.paused = false; break;
    case PAUSE_MENU: leave_level(); break;
    default:
      flush_jumps();
      app.paused = false;
      begin_attempt(false);
      break;
  }
}

static void end_tick(void) {
  if (leaving || end_t < 0.4f) return;
  if (app_hit(K_LEFT | K_RIGHT)) end_sel ^= 1;
  if (app_accept() || app_hit(K_BACK)) {
    leaving = true;
    leave_t = 0;
    leave_action = app_hit(K_BACK) ? 1 : end_sel;
  }
}

void play_tick(void) {
  unshake();
  if (app.fading_out || app.fade > 0.05f) return;
  if (app.end_menu) { end_tick(); return; }
  if (app.paused) { pause_tick(); return; }
  Game *g = &app.g;
  if (app_hit(K_BACK) && !g->complete) {
    app.paused = true;
    app.sel = PAUSE_RESUME;
    app.pause_t = 0;
    return;
  }
  if (g->dead || g->complete) return;
  if (app.practice) {
    if (app_hit(K_CHECK)) place_checkpoint();
    if (app_hit(K_ERASE) && app.checkpoint_count) app.checkpoint_count--;
  }
  bool hold = (app.keys & (K_OK | K_EXE | K_UP)) != 0;
  game_step(g, hold);
  app.session_ticks++;
  if (g->dead) {
    on_death();
  } else if (g->complete) {
    on_complete();
  } else if (app.practice && (progress.options & OPT_AUTOCHECK)) {
    app.auto_check_t += ND_DT;
    if (app.auto_check_t >= 2.0f && !g->ending && (g->p.mode == MODE_SHIP || (g->p.on_ground && !g->old_on_ground))) place_checkpoint();
  }
}

/* ------------------------------------------------------------ per frame */

static void complete_update(float dt) {
  Game *g = &app.g;
  float t = app.complete_t, wx = g->L->wall_x, wy = g->wall_y;
  color_t c1 = white_if_black(app_p1()), c2 = white_if_black(app_p2());
  if (!app.practice) {
    while (rings_a < 5 && t >= rings_a * 0.05f) { fx_circle_world(wx, wy, CE_RING_BIG, c1); rings_a++; }
  }
  if (t >= FIREWORK_TIME && !phase2) {
    phase2 = true;
    rays_fading = true;
    rays_fade_t = 0;
    gfx_layer_build(FONT_HUGE, app.practice ? "PRACTICE COMPLETE!" : "LEVEL COMPLETE!");
    popup = POP_COMPLETE;
    popup_t = 0;
    fx_circle_world(wx, wy, CE_WALL2, c1);
    float cx = g->cam_x + VIEW_W / 2, cy = g->cam_y + VIEW_H / 2 - GROUND_OFFSET;
    fx_circle_world(cx, cy, CE_TITLE, c1);
    fx_burst_world(cx, cy, PE_COMPLETE, c1, 100);
    fx_burst_world(cx, cy, PE_COMPLETE, c2, 100);
  }
  if (phase2 && !app.practice) {
    float u = t - FIREWORK_TIME;
    while (rings_b < 5 && u >= rings_b * 0.05f) { fx_circle_world(wx, wy, CE_RING_BIG, c1); rings_b++; }
    while (t <= MENU_TIME && u >= fireworks * 0.23f) {
      float fx_ = g->cam_x + 100 + frand() * (VIEW_W - 200), fy = g->cam_y - GROUND_OFFSET + frand() * VIEW_H;
      fx_circle_world(fx_, fy, CE_FIREWORK, c2);
      fx_burst_world(fx_, fy, PE_FIREWORK, c2, 25);
      fireworks++;
    }
  }
  if (t >= MENU_TIME && !app.end_menu) {
    app.end_menu = true;
    end_t = 0;
    end_sel = 0;
    leaving = false;
  }
  app.complete_t += dt;
  if (rays_on) {
    rays_t += dt;
    if (rays_fading) {
      rays_fade_t += dt;
      if (rays_fade_t > 0.4f) rays_on = false;
    }
  }
}

void play_frame(float dt) {
  Game *g = &app.g;
  if (app.paused) {
    app.pause_t += dt;
  } else {
    fx_update(g, dt, app_p1(), app_p2(), app.practice);
    if (g->dead) {
      app.death_t += dt;
      if (app.death_t >= death_len) begin_attempt(false);
    }
    if (respawn_phase > 0) {
      if (respawn_t <= 0 && (respawn_phase & 1) == 0) fx_circle_world(g->p.x, g->p.y, CE_RESPAWN, white_if_black(app_p1()));
      respawn_t += dt;
      if (respawn_t >= 0.05f) { respawn_t = 0; respawn_phase--; }
    }
    if (g->complete) complete_update(dt);
    if (popup) popup_t += dt;
    if (app.end_menu) {
      end_t += dt;
      if (leaving) {
        leave_t += dt;
        if (leave_t >= 0.5f) {
          leaving = false;
          if (leave_action == 0) restart_session();
          else { app.end_menu = false; leave_level(); }
        }
      }
    }
    if (shake_t > 0) {
      shake_t -= dt;
      shake_dx = shake_amp * (frand() * 2 - 1);
      shake_dy = shake_amp * (frand() * 2 - 1);
      g->cam_x += shake_dx;
      g->cam_y += shake_dy;
      shake_applied = true;
    }
  }
  memset(&opts, 0, sizeof(opts));
  opts.p1 = app_p1();
  opts.p2 = app_p2();
  opts.practice = app.practice;
  opts.show_percent = (progress.options & OPT_PERCENT) != 0;
  opts.show_bar = (progress.options & OPT_BAR) != 0;
  opts.low_detail = (progress.options & OPT_LOWDETAIL) != 0;
  opts.hide_player = g->complete || (respawn_phase > 0 && (respawn_phase & 1) == 0);
  opts.attempt = app.attempt;
  opts.attempt_x = app.attempt_x;
  opts.attempt_y = app.attempt_y;
  opts.time = g->tick / (float)ND_HZ;
  opts.checkpoints = app.checkpoints;
  opts.checkpoint_count = app.practice ? app.checkpoint_count : 0;
  opts.coins_saved = app.testing ? 0 : progress.lv[app.level].coins;
  scene_prepare(g, &opts);
}

/* ------------------------------------------------------------ drawing */

static void draw_rays(void) {
  const Game *g = &app.g;
  float ox = g->L->wall_x + 30, oy = g->wall_y, len = 632;
  color_t c = app_p1() ? app_p1() : app_p2();
  for (int i = 0; i < 8; i++) {
    const Ray *r = &rays[i];
    float el = rays_t - r->delay;
    if (el < 0) continue;
    float t = el / r->dur;
    if (t > 1) t = 1;
    t = t * (2 - t);
    float a = r->alpha;
    if (rays_fading) a *= fmaxf(0, 1 - rays_fade_t / 0.4f);
    float w0 = r->h / 4, w1 = w0 + (r->h - w0) * t, l = len * t;
    float ca = cosf(r->angle * 3.14159265f / 180), sa = sinf(r->angle * 3.14159265f / 180);
    float lx[4] = {0, 0, -l, -l}, ly[4] = {w0 / 2, -w0 / 2, -w1 / 2, w1 / 2}, xy[8];
    for (int k = 0; k < 4; k++) {
      float wx = ox + lx[k] * ca - ly[k] * sa, wy = oy + lx[k] * sa + ly[k] * ca;
      xy[2 * k] = wx_to_sx(g, wx);
      xy[2 * k + 1] = wy_to_sy(g, wy);
    }
    gfx_poly(xy, 4, c, (unsigned)(a * 0.8f), BLEND_ADD);
  }
}

static void draw_popup(void) {
  float s;
  if (popup == POP_COMPLETE) {
    if (popup_t < 0.66f) s = 0.01f + 1.09f * ui_ease_elastic_out(popup_t / 0.66f, 0.3f);
    else if (popup_t < 1.54f) s = 1.1f;
    else if (popup_t < 1.76f) { float u = (popup_t - 1.54f) / 0.22f; s = 1.1f - 1.09f * u * u; }
    else { popup = POP_NONE; return; }
    color_t top = app.practice ? rgb(170, 255, 255) : rgb(210, 255, 90), bot = app.practice ? rgb(0, 200, 255) : rgb(60, 200, 0);
    gfx_layer_draw(160, 116, (int)(s * 256), top, bot, 256);
  } else if (popup == POP_NEWBEST) {
    if (popup_t < 0.4f) s = 0.01f + 0.99f * ui_ease_elastic_out(popup_t / 0.4f, 0.3f);
    else if (popup_t < 1.1f) s = 1;
    else if (popup_t < 1.3f) { float u = (popup_t - 1.1f) / 0.2f; s = 1 - 0.99f * u * u; }
    else { popup = POP_NONE; return; }
    gfx_layer_draw(160, 112, (int)(s * 256), 0xffff, 0xffff, 256);
  }
}

static float sel_scale(int id, int sel) { return id == sel ? 1.12f + 0.03f * sinf(app.t * 6) : 1.0f; }

static void pause_draw(void) {
  ui_dim(70);
  ui_window_dark(10, 8, 300, 224, 130);
  ui_title(FONT_BIG, 160, 34, app.L.name);
  const LevelStat *s = &progress.lv[app.level];
  ui_title(FONT_SMALL, 160, 56, "NORMAL MODE");
  ui_progress_bar(160, 67, 210, 14, app.testing ? 0 : s->normal, rgb(0, 255, 0), true);
  ui_title(FONT_SMALL, 160, 90, "PRACTICE MODE");
  ui_progress_bar(160, 101, 210, 14, app.testing ? 0 : s->practice, rgb(0, 255, 255), true);
  unsigned nc = level_coins();
  if (nc && !app.testing)
    for (unsigned i = 0; i < nc; i++)
      gfx_sprite((s->coins >> i & 1) ? SPR_COIN_UI : SPR_COIN_UI_EMPTY, 160 + ((int)i * 2 - (int)nc + 1) * 12, 126, 0, 0xffff, 256, BLEND_NORMAL);
  ui_sprite(app.practice ? SPR_BTN_NORMAL : SPR_BTN_PRACTICE, 56, 176, sel_scale(PAUSE_PRACTICE, app.sel), 256);
  ui_sprite(SPR_BTN_RESUME, 130, 176, sel_scale(PAUSE_RESUME, app.sel), 256);
  ui_sprite(SPR_BTN_MENU, 204, 176, sel_scale(PAUSE_MENU, app.sel), 256);
  ui_sprite(SPR_BTN_REPLAY, 268, 176, sel_scale(PAUSE_REPLAY, app.sel), 256);
  static const char *const hints[4] = {"PRACTICE MODE", "RESUME", "MENU", "RESTART"};
  const char *h = hints[app.sel];
  if (app.sel == PAUSE_PRACTICE && app.practice) h = "NORMAL MODE";
  gfx_text_center(FONT_SMALL, 160, 222, h, rgb(255, 255, 140), rgb(255, 200, 0), 256);
}

static void draw_chain(int x, int y0, int y1) {
  for (int y = y0; y < y1; y += 12) {
    gfx_round_rect(x - 4, y, 8, 11, 3, rgb(60, 60, 64), 256);
    gfx_round_rect(x - 3, y + 1, 6, 9, 2, rgb(190, 190, 200), 256);
    gfx_round_rect(x - 1, y + 3, 2, 5, 1, rgb(60, 60, 64), 256);
  }
}

static void end_draw(void) {
  float drop = ui_ease_bounce_out(end_t / 1.0f);
  float cy = -100 + 224 * drop;
  if (leaving) {
    float u = leave_t / 0.5f;
    cy += (-130 - cy) * u * u;
  }
  ui_dim((unsigned)(fminf(end_t, 0.6f) * 256 * (leaving ? 1 - leave_t / 0.5f : 1)));
  int y = (int)cy, top = y - 100;
  draw_chain(64, -8, top + 6);
  draw_chain(256, -8, top + 6);
  ui_window_navy(26, top, 268, 204);
  y -= 6;
  if (app.practice) gfx_text_center(FONT_BIG, 160, y - 66, "PRACTICE COMPLETE!", rgb(170, 255, 255), rgb(0, 200, 255), 256);
  else gfx_text_center(FONT_BIG, 160, y - 66, "LEVEL COMPLETE!", rgb(210, 255, 90), rgb(60, 200, 0), 256);
  char buf[32];
  int n;
  memcpy(buf, "ATTEMPTS: ", 10);
  gfx_format_uint(buf + 10, app.attempt);
  ui_gold(FONT_BIG, 160, y - 42, buf);
  memcpy(buf, "JUMPS: ", 7);
  gfx_format_uint(buf + 7, app.session_jumps);
  ui_gold(FONT_BIG, 160, y - 24, buf);
  unsigned secs = app.session_ticks / ND_HZ;
  memcpy(buf, "TIME: ", 6);
  n = 6;
  if (secs >= 3600) { n += gfx_format_uint(buf + n, secs / 3600); buf[n++] = ':'; }
  buf[n++] = (char)('0' + secs / 600 % 6);
  buf[n++] = (char)('0' + secs / 60 % 10);
  buf[n++] = ':';
  buf[n++] = (char)('0' + secs % 60 / 10);
  buf[n++] = (char)('0' + secs % 10);
  buf[n] = 0;
  ui_gold(FONT_BIG, 160, y - 6, buf);
  unsigned nc = level_coins();
  if (app.practice) {
    ui_title(FONT_SMALL, 160, y + 14, "WELL DONE... NOW TRY TO COMPLETE");
    ui_title(FONT_SMALL, 160, y + 26, "IT WITHOUT ANY CHECKPOINTS!");
  } else if (nc && !app.testing) {
    for (unsigned i = 0; i < nc; i++) {
      int x = 160 + ((int)i * 2 - (int)nc + 1) * 22;
      bool got = coins_run >> i & 1;
      float sc = 1;
      if ((coins_new >> i & 1)) {
        float t = end_t - 0.6f - 0.35f * (float)__builtin_popcount(coins_new & ((1u << i) - 1));
        if (t < 0) got = false;
        else if (t < 0.35f) sc = 3 - 2 * ui_ease_bounce_out(t / 0.35f);
      }
      if (got && sc > 1.01f) gfx_sprite(SPR_COIN_BIG_EMPTY, x, y + 20, 0, 0xffff, 256, BLEND_NORMAL);
      ui_sprite(got ? SPR_COIN_BIG : SPR_COIN_BIG_EMPTY, (float)x, (float)(y + 20), sc, 256);
    }
  } else {
    ui_title(FONT_BIG, 160, y + 26, complete_msgs[msg_index]);
  }
  if (stars_new) {
    float t = end_t - 0.6f - 0.35f * (float)__builtin_popcount(coins_new);
    if (t > 0) {
      float sc = t < 0.35f ? 3 - 2 * ui_ease_bounce_out(t / 0.35f) : 1;
      ui_sprite(SPR_STAR_BIG, 262, (float)(y - 24), sc, (unsigned)fminf(256, t / 0.1f * 256));
      char sb[8] = "+";
      gfx_format_uint(sb + 1, app.L.stars);
      if (t > 0.1f) gfx_text_center(FONT_BIG, 262, y + 2, sb, 0xffff, 0xffff, 256);
    }
  }
  float bs = ui_ease_elastic_out(end_t / 1.0f, 0.3f) * 0.9f;
  ui_sprite(SPR_BTN_REPLAY, 112, (float)(y + 68), bs * sel_scale(0, end_sel), 256);
  ui_sprite(SPR_BTN_MENU, 208, (float)(y + 68), bs * sel_scale(1, end_sel), 256);
}

void play_draw(void) {
  scene_draw();
  if (rays_on) draw_rays();
  fx_draw_screen();
  if (popup) draw_popup();
  if (!app.paused && !app.end_menu && !app.g.complete) gfx_sprite(SPR_BTN_PAUSE, 302, 16, 0, 0xffff, 110, BLEND_NORMAL);
  if (app.paused) pause_draw();
  if (app.end_menu) end_draw();
}
