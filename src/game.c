/* Geometry Dash 2.x cube/ship physics at 240 Hz.
 *
 * Constants and collision rules follow the reverse-engineered values used by
 * the gd3ds project (itself based on the Pathfinder mod's physics): velocities
 * are in GD units per second, "vy" is relative to the current gravity. */
#include "game.h"
#include <math.h>
#include <string.h>

enum { SPEED_SLOW, SPEED_NORMAL, SPEED_FAST, SPEED_FASTER };
static const float SPEEDS[4] = {251.16007972f, 311.58009371f, 387.42014040f, 468.00013884f};
static const float SPEED_MULT[4] = {0.7f, 0.9f, 1.1f, 1.3f};
static const float CUBE_JUMP[4] = {573.481728f, 603.7217172f, 616.681728f, 606.421728f};
static const float CUBE_GRAV[4] = {-2747.52f, -2794.1082f, -2786.4f, -2799.36f};
static const float VEL_THRESH[4] = {101.541492f, 103.485494592f, 103.377492f, 103.809492f};
enum { J_YPAD, J_YORB, J_BPAD, J_BORB, J_PPAD, J_PORB };
/* [speed][jump type][cube, ship] for normal-size players */
static const float JUMPS[4][6][2] = {
  {{864, 432}, {573.48f, 573.48f}, {-345.6f, -229.392f}, {-229.392f, -229.392f}, {561.6f, 302.4f}, {412.884f, 212.166f}},
  {{864, 432}, {603.72f, 603.72f}, {-345.6f, -345.6f}, {-241.488f, -241.488f}, {561.6f, 302.4f}, {434.7f, 223.398f}},
  {{864, 432}, {616.68f, 616.68f}, {-345.6f, -345.6f}, {-246.672f, -246.672f}, {561.6f, 302.4f}, {443.988f, 228.15f}},
  {{864, 432}, {606.42f, 606.42f}, {-345.6f, -345.6f}, {-242.568f, -242.568f}, {561.6f, 302.4f}, {436.644f, 224.37f}},
};
#define ROT_SPEED 415.3848f
#define CEILING_INVUL 0.1f
#define DRAG_TIME 0.1f
#define SHIP_MIN (-345.6f)
#define SHIP_MAX 432.0f
#define END_START (10 * 30.0f)

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float grav(const Player *p, float v) { return p->upside ? -v : v; }
static float grav_bottom(const Player *p) { return p->upside ? -(p->y + 15) : p->y - 15; }
static float grav_top(const Player *p) { return p->upside ? -(p->y - 15) : p->y + 15; }
static float grav_floor(const Player *p) { return p->upside ? -p->ceiling_y : p->ground_y; }

bool game_used(const Game *g, unsigned i) { return i < MAX_OBJECTS && (g->used[i >> 3] >> (i & 7) & 1); }
static void set_used(Game *g, unsigned i) { g->used[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static void fx(Game *g, uint8_t kind, uint8_t arg, int obj, float x, float y) {
  if (g->fx_count < sizeof(g->fx) / sizeof(g->fx[0])) g->fx[g->fx_count++] = (FxEvent){kind, arg, (int16_t)obj, x, y};
}
static void kill(Game *g, int obj) {
  if (g->dead) return;
  g->dead = true;
  g->death_obj = obj < 0 ? 0xffff : (uint16_t)obj;
  fx(g, FX_DEATH, 0, obj, g->p.x, g->p.y);
}

unsigned game_coin_index(const Level *L, unsigned obj) {
  unsigned k = 0;
  for (unsigned i = 0; i < obj && i < L->count; i++) if (L->objs[i].type == OT_COIN) k++;
  return k;
}

static void set_velocity(Player *p, float v, bool override) { p->vel_override = override; p->vy = v; }
static void landing(Player *p) { if (p->mode == MODE_CUBE) p->jumped = false; }
static void update_rot_dir(Player *p) { p->rot_dir_neg = p->upside; }
static float closest_rotation(float rot) {
  float r = fmodf(rot, 360.0f);
  if (r < 0) r += 360;
  float s = roundf(r / 90) * 90;
  s = fmodf(s, 360);
  return s < 0 ? s + 360 : s;
}
static float slerp_angle(float from, float to, float t) {
  float fx = cosf(from * .5f), fy = sinf(from * .5f), tx = cosf(to * .5f), ty = sinf(to * .5f);
  float dot = fx * tx + fy * ty;
  if (dot < 0) { dot = -dot; tx = -tx; ty = -ty; }
  float w0 = 1 - t, w1 = t;
  if (dot < 0.9999f) {
    float b = acosf(dot), sb = sinf(b);
    w0 = sinf(w0 * b) / sb;
    w1 = sinf(w1 * b) / sb;
  }
  return atan2f(w0 * fy + w1 * ty, w0 * fx + w1 * tx) * 2;
}
#define DEG (3.14159265f / 180.0f)

static void set_bounds_for_portal(Game *g, float portal_y) {
  Player *p = &g->p;
  float v = (portal_y - (300 + 60) / 2.0f) / 30.0f, c = ceilf(v);
  if (fabsf(v - roundf(v)) < 1e-6f) c += 1;
  p->ground_y = c > 0 ? c * 30 : 0;
  p->ceiling_y = p->ground_y + 300;
  g->cam_intended_y = (p->ground_y + p->ceiling_y) / 2 - (VIEW_H / 2 - GROUND_OFFSET);
}

void game_start(Game *g, const Level *L, bool first_attempt) {
  memset(g, 0, sizeof(*g));
  g->L = L;
  Player *p = &g->p;
  p->x = 0;
  p->y = 15;
  p->speed = SPEED_NORMAL;
  p->ceiling_y = 999999;
  p->snap_obj = -1;
  p->coyote = 1 << 30;
  p->on_ground = true;
  if (L->start_mode == MODE_SHIP) {
    p->mode = MODE_SHIP;
    set_bounds_for_portal(g, 150);
  }
  for (int c = 0; c < CH_COUNT; c++) {
    memcpy(g->ch[c].cur, L->colors[c], 3);
    memcpy(g->ch[c].to, L->colors[c], 3);
  }
  g->attempt_camera = first_attempt;
  g->cam_x = first_attempt ? 15 : p->x - PLAYER_SCREEN_X;
  g->ground_x = g->bg_x = g->cam_x;
  g->death_obj = 0xffff;
}

float game_progress(const Game *g) {
  if (g->complete) return 100;
  float v = g->p.x / g->L->end_x * 100;
  return clampf(v, 0, 100);
}

static void clamp_ground(Game *g) {
  Player *p = &g->p;
  if (p->y - 15 < p->ground_y) {
    if (p->ceil_inv <= 0 && p->mode == MODE_CUBE && p->upside) kill(g, -1);
    if (grav(p, p->vy) <= 0) set_velocity(p, 0, false);
    p->y = p->ground_y + 15;
    p->snap_frame = 0;
  }
  if (p->y + 15 > p->ceiling_y) {
    if (p->ceil_inv <= 0 && p->mode == MODE_CUBE && !p->upside) kill(g, -1);
    if (grav(p, p->vy) >= 0) set_velocity(p, 0, false);
    p->y = p->ceiling_y - 15;
  }
}

static bool overlap(float ax, float ay, float ahw, float ahh, float bx, float by, float bhw, float bhh) {
  return fabsf(ax - bx) < ahw + bhw && fabsf(ay - by) < ahh + bhh;
}

static void try_snap(Game *g, unsigned block) {
  Player *p = &g->p;
  if (p->snap_obj < 0) return;
  const LObj *a = &g->L->objs[block], *b = &g->L->objs[p->snap_obj];
  float dx = (float)a->x - b->x, dy = grav(p, (float)a->y - b->y);
  static const float stairs[3][2] = {{150, -30}, {120, 30}, {90, 60}};
  for (int k = 0; k < 3; k++)
    if (fabsf(dx - stairs[k][0]) <= 1 && fabsf(dy - stairs[k][1]) <= 1) {
      p->x = clampf(a->x + p->snap_diff, p->x - 1, p->x + 1);
      return;
    }
}

static void solid(Game *g, unsigned i, float ox, float oy, float hw, float hh) {
  Player *p = &g->p;
  float clip = (p->mode == MODE_SHIP ? 7.0f : 10.0f) + fabsf(p->vy) * ND_DT;
  float o_bottom = p->upside ? -(oy + hh) : oy - hh, o_top = p->upside ? -(oy - hh) : oy + hh;
  bool internal = overlap(p->x, p->y, 4.5f, 4.5f, ox, oy, hw, hh), grav_snap = false;
  if (p->ceil_inv > 0) {
    float in_bottom = p->upside ? -(p->y + 4.5f) : p->y - 4.5f, diff = o_bottom - in_bottom;
    grav_snap = internal && diff >= 0 && diff <= clip;
  }
  if (!grav_snap && internal) { kill(g, (int)i); return; }
  float bottom = grav_bottom(p);
  if (o_top - bottom <= clip && p->vy <= 0) {
    p->y = grav(p, o_top) + grav(p, 15);
    if (p->vy <= 0) p->vy = 0;
    p->on_ground = true;
    p->inverse_rot = false;
    p->time_since_ground = 0;
    landing(p);
    if (p->mode == MODE_CUBE) {
      /* GD nudges x by up to a unit so stair jumps land consistently */
      if (!g->old_on_ground && p->snap_frame > 0 && p->snap_frame + 1 < p->frame) try_snap(g, i);
      p->snap_frame = p->frame;
      p->snap_obj = (int16_t)i;
      p->snap_diff = p->x - g->L->objs[i].x;
    }
  } else if (p->mode != MODE_CUBE || grav_snap) {
    if ((grav_top(p) - o_bottom <= clip && p->vy >= 0) || grav_snap) {
      if (!grav_snap) p->on_ceiling = true; else p->vy = 0;
      p->inverse_rot = false;
      p->time_since_ground = 0;
      p->y = grav(p, o_bottom) - grav(p, 15);
      if (p->vy >= 0) p->vy = 0;
    }
  }
}

static void special(Game *g, unsigned i, const LObj *o, bool hold) {
  Player *p = &g->p;
  const ObjDef *d = &objdefs[o->type];
  int m = p->mode == MODE_SHIP ? 1 : 0;
  bool used = game_used(g, i);
  switch (d->special) {
    case SP_PAD_Y: case SP_PAD_P:
      if (used) break;
      p->vy = JUMPS[p->speed][d->special == SP_PAD_Y ? J_YPAD : J_PPAD][m];
      p->on_ground = false; p->inverse_rot = false; p->left_ground = true; p->jumped = true;
      set_used(g, i);
      update_rot_dir(p);
      fx(g, FX_PAD, d->special == SP_PAD_Y ? 0 : 2, (int)i, o->x, o->y);
      break;
    case SP_PAD_B: {
      if (used) break;
      int rot = (o->xf & 3) * 90;
      if (o->xf & 8) rot = (rot + 180) % 360;
      bool down = rot > 90 && rot < 270;
      if ((!down && p->upside) || (down && !p->upside)) break;
      p->left_ground = true;
      update_rot_dir(p);
      p->vy = JUMPS[p->speed][J_BPAD][m];
      p->upside = !p->upside;
      p->on_ground = false; p->inverse_rot = false; p->ceil_inv = CEILING_INVUL; p->jumped = true;
      set_used(g, i);
      fx(g, FX_PAD, 1, (int)i, o->x, o->y);
      break;
    }
    case SP_ORB_Y: case SP_ORB_P: case SP_ORB_B:
      if (!used && hold && p->buffer == BUF_READY) {
        int j = d->special == SP_ORB_Y ? J_YORB : d->special == SP_ORB_P ? J_PORB : J_BORB;
        p->vy = JUMPS[p->speed][j][m];
        if (d->special == SP_ORB_B) { p->upside = !p->upside; p->ceil_inv = CEILING_INVUL; }
        p->on_ground = false; p->on_ceiling = false; p->inverse_rot = false; p->left_ground = true;
        p->buffer = BUF_END; p->jumped = true;
        update_rot_dir(p);
        g->jumps++;
        set_used(g, i);
        fx(g, FX_ORB, (uint8_t)(d->special - SP_ORB_Y), (int)i, o->x, o->y);
      }
      break;
    case SP_GRAV_N: case SP_GRAV_F:
      if (used) break;
      p->ceil_inv = CEILING_INVUL;
      if (p->upside != (d->special == SP_GRAV_F)) {
        p->vy /= -2;
        p->upside = d->special == SP_GRAV_F;
        p->inverse_rot = false;
        p->snap_rot = true;
        p->left_ground = true;
        fx(g, FX_GRAVITY, d->special == SP_GRAV_F, (int)i, o->x, o->y);
      }
      set_used(g, i);
      break;
    case SP_PORTAL_CUBE:
      if (used) break;
      p->ground_y = 0;
      p->ceiling_y = 999999;
      if (p->mode != MODE_CUBE) {
        p->vy /= 2;
        p->ceil_inv = CEILING_INVUL;
        p->snap_rot = true;
        p->mode = MODE_CUBE;
        update_rot_dir(p);
        fx(g, FX_PORTAL, 0, (int)i, o->x, o->y);
      }
      set_used(g, i);
      break;
    case SP_PORTAL_SHIP:
      if (used) break;
      set_bounds_for_portal(g, o->y);
      if (p->mode != MODE_SHIP) {
        p->vy /= 2;
        p->mode = MODE_SHIP;
        p->inverse_rot = false;
        p->snap_rot = true;
        p->vy = clampf(p->vy, SHIP_MIN, SHIP_MAX);
        fx(g, FX_PORTAL, 1, (int)i, o->x, o->y);
      }
      set_used(g, i);
      break;
    case SP_COIN:
      if (used) break;
      set_used(g, i);
      g->coins |= (uint8_t)(1u << (game_coin_index(g->L, i) & 7));
      fx(g, FX_COIN, 0, (int)i, o->x, o->y);
      break;
    default: break;
  }
}

static void collide(Game *g, bool hold) {
  Player *p = &g->p;
  const Level *L = g->L;
  unsigned first = level_lower_bound(L, (int)(p->x - 60)), end = first;
  while (end < L->count && L->objs[end].x < p->x + 60) end++;
  bool touching_orb = false;
  /* GD resolves special objects, then solids, then hazards. */
  for (int pass = 0; pass < 3 && !g->dead; pass++)
    for (unsigned i = first; i < end && !g->dead; i++) {
      const LObj *o = &L->objs[i];
      const ObjDef *d = &objdefs[o->type];
      int want = pass == 0 ? HIT_SPECIAL : pass == 1 ? HIT_SOLID : HIT_HAZARD;
      if (d->hit != want) continue;
      if (d->special == SP_COIN && game_used(g, i)) continue;
      float hw, hh;
      obj_hitbox(o, &hw, &hh);
      if (!overlap(p->x, p->y, 15, 15, o->x, o->y, hw, hh)) continue;
      if (pass == 0) {
        if (d->special >= SP_ORB_Y && d->special <= SP_ORB_B) {
          touching_orb = true;
          if (!g->orb_touching && !game_used(g, i)) fx(g, FX_ORB_TOUCH, 0, (int)i, o->x, o->y);
        }
        special(g, i, o, hold);
      } else if (pass == 1) {
        solid(g, i, o->x, o->y, hw, hh);
      } else {
        kill(g, (int)i);
      }
    }
  g->orb_touching = touching_orb;
}

static void cube_mode(Game *g, bool hold, bool pressed) {
  Player *p = &g->p;
  float mult = p->rot_dir_neg ? -1.0f : 1.0f;
  p->gravity = CUBE_GRAV[p->speed];
  if (p->vy < -810) p->vy = -810;
  if (p->vy > 1080) p->vy = 1080;
  if (p->y > 2794) kill(g, -1);
  if (p->snap_rot) p->target_rot = p->rot;
  if (!p->on_ground) {
    if (p->inverse_rot) p->target_rot -= ROT_SPEED / 2 * ND_DT * mult;
    else p->target_rot += ROT_SPEED * ND_DT * mult;
  }
  if (p->on_ground) update_rot_dir(p);
  bool coyote = p->upside && hold && p->coyote < 10;
  if ((p->on_ground || coyote) && hold) {
    set_velocity(p, CUBE_JUMP[p->speed], g->hold_prev);
    p->inverse_rot = false;
    p->buffer = BUF_END;
    p->on_ground = false;
    p->jumped = true;
    g->jumps++;
    if (!pressed) p->time_since_ground = DRAG_TIME;
    fx(g, FX_JUMP, 0, -1, p->x, p->y);
  }
  if (p->on_ground) p->target_rot = closest_rotation(p->rot);
}

static void ship_mode(Game *g, bool hold) {
  Player *p = &g->p;
  float t = grav(p, VEL_THRESH[p->speed]);
  if (hold) {
    p->buffer = BUF_END;
    p->gravity = p->vy <= t ? 1397.0491f : 1117.64328f;
  } else {
    p->gravity = p->vy >= t ? -1341.1719f : -894.11464f;
  }
  if (p->gravity < 0 && p->vy < SHIP_MIN) p->vy = SHIP_MIN;
  else if (p->gravity > 0 && p->vy > SHIP_MAX) p->vy = SHIP_MAX;
}

static void run_player(Game *g, const Player *old, bool hold, bool pressed) {
  Player *p = &g->p;
  if (!p->left_ground) {
    if (p->y - 15 <= p->ground_y) {
      if (p->upside) p->on_ceiling = true; else { p->on_ground = true; landing(p); }
      p->inverse_rot = false;
      p->time_since_ground = 0;
    }
    if (p->y + 15 >= p->ceiling_y) {
      if (p->upside) { p->on_ground = true; landing(p); } else p->on_ceiling = true;
      p->inverse_rot = false;
      p->time_since_ground = 0;
    }
  }
  if (!old->on_ground && p->on_ground) fx(g, FX_LAND, 0, -1, p->x, p->y);
  if (grav_bottom(old) > grav_floor(old) && p->upside == old->upside && !p->on_ground && p->vy <= 0) {
    if (old->on_ground && !g->hold_prev) p->coyote = 0;
    p->coyote++;
  } else {
    p->coyote = 1 << 30;
  }
  if (p->mode == MODE_CUBE) cube_mode(g, hold, pressed); else ship_mode(g, hold);
  p->time_since_ground += ND_DT;
  if (!p->vel_override) {
    float nv = p->vy + p->gravity * ND_DT;
    if (!(p->on_ground || p->on_ceiling) && (old->on_ground || old->on_ceiling) &&
        ((!hold && g->pressed_prev) || p->buffer == BUF_READY) && grav_bottom(old) > grav_floor(old)) {
      p->y += grav(old, old->gravity) * ND_DT * ND_DT;
      if (p->vy == 0) nv += old->gravity * ND_DT;
    }
    p->vy = nv;
  }
  if (g->ending) return;
  p->rot = fmodf(p->rot, 360.0f);
  p->left_ground = false;
  if (p->ceil_inv > 0) p->ceil_inv -= ND_DT; else p->ceil_inv = 0;
  clamp_ground(g);
  if (p->mode == MODE_CUBE) {
    float lerp = SPEED_MULT[p->speed] * 0.175f;
    if (p->on_ground) {
      lerp *= 3;
      float t = fminf(ND_DT, ND_DT * lerp) * 60;
      p->rot = slerp_angle(p->rot * DEG, p->target_rot * DEG, t) / DEG;
    } else {
      p->rot = p->target_rot;
    }
  } else {
    float dx = p->x - old->x, dy = p->y - old->y, ang = atan2f(-dy, dx);
    if (p->snap_rot) p->rot = ang / DEG;
    else if (ND_DT * 72 <= dx * dx + dy * dy) p->rot = slerp_angle(p->rot * DEG, ang, ND_DT * 60 * 0.15f) / DEG;
  }
  p->snap_rot = false;
}

static void ease_channel(Channel *c) {
  if (c->t >= c->dur) { memcpy(c->cur, c->to, 3); return; }
  c->t++;
  for (int k = 0; k < 3; k++) c->cur[k] = (uint8_t)(c->from[k] + ((int)c->to[k] - c->from[k]) * (int)c->t / (int)c->dur);
}
static void fire_event(Game *g, const LevelEvent *e) {
  if (e->kind == EV_COLOR) {
    int chans[2] = {e->arg, -1};
    if (e->arg == CH_BG && (e->flags & EVF_TINT_GROUND)) chans[1] = CH_G1;
    for (int k = 0; k < 2; k++) {
      if (chans[k] < 0 || chans[k] >= CH_COUNT) continue;
      Channel *c = &g->ch[chans[k]];
      memcpy(c->from, c->cur, 3);
      c->to[0] = e->r; c->to[1] = e->g; c->to[2] = e->b;
      c->t = 0;
      c->dur = (uint16_t)((uint32_t)e->dur * ND_HZ / 1000);
      if (!c->dur) memcpy(c->cur, c->to, 3);
    }
  } else if (e->kind == EV_FADE) {
    g->fade_effect = e->arg;
  } else if (e->kind == EV_TRAIL) {
    g->trail = e->arg != 0;
  }
}
static void triggers(Game *g) {
  const Level *L = g->L;
  Player *p = &g->p;
  while (g->next_event < L->event_count) {
    const LevelEvent *e = &L->events[g->next_event];
    if (e->x >= p->x) break;
    if (!(e->flags & EVF_TOUCH)) fire_event(g, e);
    g->next_event++;
  }
  /* touch-triggered events fire once when the player overlaps them */
  for (unsigned k = 0; k < L->event_count && k < 64; k++) {
    const LevelEvent *e = &L->events[k];
    if (!(e->flags & EVF_TOUCH) || (g->touch_done[k >> 3] >> (k & 7) & 1)) continue;
    if (e->x > p->x + 30 || e->x < p->x - 60) continue;
    if (overlap(p->x, p->y, 15, 15, e->x, e->y, 15, 15)) {
      g->touch_done[k >> 3] |= (uint8_t)(1u << (k & 7));
      fire_event(g, e);
    }
  }
  for (int c = 0; c < CH_COUNT; c++) ease_channel(&g->ch[c]);
}

static float ease_in_out(float t, float rate) {
  t *= 2;
  if (t < 1) return 0.5f * powf(t, rate);
  return 1 - 0.5f * powf(2 - t, rate);
}

static void camera(Game *g) {
  Player *p = &g->p;
  const Level *L = g->L;
  if (g->menu_camera) {
    float v = SPEEDS[SPEED_NORMAL] * ND_DT;
    g->ground_x += v;
    g->bg_x += v;
    g->ground_gfx = 0;
    return;
  }
  float playable = p->ceiling_y - p->ground_y, want_gfx = 0;
  if (p->mode != MODE_CUBE) want_gfx = (VIEW_H - playable) / 2;
  g->ground_gfx += (want_gfx - g->ground_gfx) * 0.02f;
  if (g->wall_y == 0 && g->cam_x + VIEW_W >= L->wall_x - 4.5f * 30) {
    float mid = g->cam_y + VIEW_H / 2 - GROUND_OFFSET, lo = 60 + (VIEW_H / 2 - GROUND_OFFSET);
    g->wall_y = mid > lo ? mid : lo;
  }
  if (g->wall_y > 0 && g->cam_x + VIEW_W >= L->wall_x - 60) {
    if (g->cam_wall_t == 0) { g->cam_wall_y0 = g->cam_y; g->bg_wall_x0 = g->bg_x; g->ground_wall_x0 = g->ground_x; }
    float t = clampf(g->cam_wall_t / 1.0f, 0, 1), e = ease_in_out(t, 2);
    float fx_ = L->wall_x - VIEW_W, fy = g->wall_y - (VIEW_H / 2 - GROUND_OFFSET);
    g->cam_x = fx_ - 60 + 60 * e;
    g->cam_y = g->cam_wall_y0 + (fy - g->cam_wall_y0) * e;
    g->bg_x = g->bg_wall_x0 + 60 * e;
    g->ground_x = g->ground_wall_x0 + 60 * e;
    g->cam_wall_t += ND_DT;
    return;
  }
  float target = g->cam_y;
  if (p->mode == MODE_CUBE) {
    float off = p->upside ? -30.0f : 0;
    if (p->y > g->cam_y + 180 + off) target = p->y - (180 + off);
    else if (p->y < g->cam_y + 30 + off) target = p->y - (30 + off);
  } else {
    target = g->cam_intended_y;
  }
  if (target < 0) target = 0;
  g->cam_y += (target - g->cam_y) / 40.0f;
  if (g->cam_y < 0) g->cam_y = 0;
  float want = p->x - PLAYER_SCREEN_X;
  if (g->attempt_camera && want < 15) want = 15;
  float moved = want - g->cam_x;
  g->cam_x = want;
  if (moved > 0) { g->ground_x += moved; g->bg_x += moved; }
}

void game_step(Game *g, bool hold) {
  if (g->dead || g->complete) return;
  Player *p = &g->p;
  Player old = *p;
  bool pressed = hold && !g->hold_prev;
  g->old_on_ground = p->on_ground;
  if (hold) { if (p->buffer == BUF_NONE) p->buffer = BUF_READY; }
  else p->buffer = BUF_NONE;
  p->on_ground = p->on_ceiling = false;
  p->vel_override = false;
  p->x += SPEEDS[p->speed] * ND_DT;
  p->y += grav(p, p->vy) * ND_DT;
  clamp_ground(g);
  p->frame++;
  g->tick++;
  if (!g->dead && !g->ending) collide(g, hold);
  if (!g->dead) {
    const Level *L = g->L;
    if (p->x >= L->wall_x - END_START) {
      if (!g->ending) {
        g->ending = true;
        g->end_x0 = p->x;
        g->end_y0 = p->y;
        if (g->wall_y == 0) g->wall_y = g->end_y0 > 60 ? g->end_y0 : 60;
      }
      float t = clampf(powf(g->end_t, 1.2f), 0, 1), u = 1 - t;
      float mx = g->end_x0 + 40, my = g->wall_y + 150, ex = L->wall_x + 50, ey = g->wall_y - 20;
      p->x = u * u * u * g->end_x0 + 3 * u * u * t * g->end_x0 + 3 * u * t * t * mx + t * t * t * ex;
      p->y = u * u * u * g->end_y0 + 3 * u * u * t * g->end_y0 + 3 * u * t * t * my + t * t * t * ey;
      float e = g->end_t / 0.5f;
      p->rot += (e > 1 ? 1 : e * e) * ROT_SPEED * ND_DT;
      g->end_t += ND_DT;
      if (p->x > L->wall_x && !g->complete) {
        g->complete = true;
        fx(g, FX_WALL, 0, -1, (float)L->wall_x, g->wall_y);
      }
    }
    run_player(g, &old, hold, pressed);
  }
  g->hold_prev = hold;
  g->pressed_prev = pressed;
  if (!g->dead) {
    camera(g);
    triggers(g);
  }
}

void game_save_checkpoint(const Game *g, Checkpoint *c) {
  c->p = g->p;
  c->cam_x = g->cam_x; c->cam_y = g->cam_y; c->ground_x = g->ground_x; c->bg_x = g->bg_x;
  c->ground_gfx = g->ground_gfx; c->cam_intended_y = g->cam_intended_y;
  memcpy(c->ch, g->ch, sizeof(c->ch));
  c->tick = g->tick; c->next_event = g->next_event; c->jumps = g->jumps;
  c->fade_effect = g->fade_effect; memcpy(c->touch_done, g->touch_done, sizeof(c->touch_done)); c->trail = g->trail;
}

void game_load_checkpoint(Game *g, const Checkpoint *c) {
  const Level *L = g->L;
  uint8_t coins = g->coins;
  game_start(g, L, false);
  g->p = c->p;
  g->p.buffer = BUF_NONE;
  g->cam_x = c->cam_x; g->cam_y = c->cam_y; g->ground_x = c->ground_x; g->bg_x = c->bg_x;
  g->ground_gfx = c->ground_gfx; g->cam_intended_y = c->cam_intended_y;
  memcpy(g->ch, c->ch, sizeof(g->ch));
  g->tick = c->tick; g->next_event = c->next_event; g->jumps = c->jumps;
  g->fade_effect = c->fade_effect; g->trail = c->trail;
  memcpy(g->touch_done, c->touch_done, sizeof(g->touch_done));
  g->coins = coins;
  /* Objects behind the checkpoint can no longer be reached; mark them used
   * so coins and pads there do not fire again. */
  for (unsigned i = 0; i < L->count && L->objs[i].x < c->p.x - 60; i++) set_used(g, i);
}
