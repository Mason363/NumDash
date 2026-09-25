/* Particles, circle "use effects", motion trail and ghost trail.
 * Particle and circle parameters follow Geometry Dash's effect definitions
 * (sizes and speeds in GD units, additive blending). */
#include "fx.h"
#include "scene.h"
#include <math.h>
#include <string.h>

#define PX (2.0f / 3.0f)
#define MAX_PARTICLES 240
#define MAX_CIRCLES 20

typedef struct {
  uint8_t max; uint8_t radial;
  float life, life_var, angle, angle_var, speed, speed_var, gx, gy, s0, s0_var, s1, pvx, pvy;
  uint8_t c0[3], c1[3], a0, a1;
  float r_max, r_min, rot;
} PDef;
static const PDef pdefs[] = {
  /* drag */ {30, 0, .3f, .15f, 90, 45, 75, 20, 0, -300, 4, 3, 0, 2, 2, {255, 255, 255}, {0, 0, 0}, 255, 255, 0, 0, 0},
  /* ship fire */ {30, 0, .3f, .15f, 90, 45, 75, 20, 0, -300, 8, 2, 0, 1, 5, {255, 255, 255}, {0, 0, 0}, 255, 255, 0, 0, 0},
  /* ship smoke */ {30, 0, .4f, .15f, -90, 45, 0, 0, 0, -320, 3, 3, 0, 1, 2, {255, 255, 255}, {0, 0, 0}, 255, 255, 0, 0, 0},
  /* land */ {100, 0, 0, .6f, 90, 60, 150, 25, 0, -500, 5, 3, 0, 10, 0, {255, 255, 255}, {0, 0, 0}, 255, 255, 0, 0, 0},
  /* explode */ {100, 0, 0, .8f, 0, 360, 250, 150, 0, 0, 10, 5, 0, 10, 10, {255, 255, 255}, {0, 0, 0}, 255, 255, 0, 0, 0},
  /* bump (pads) */ {30, 0, .4f, .2f, 90, 0, 100, 20, 0, 0, 3, 1, 0, 14, 0, {255, 255, 0}, {0, 0, 0}, 255, 0, 0, 0, 0},
  /* ring (orbs) */ {30, 1, .3f, .1f, 0, 360, 75, 20, 0, -300, 4, 2, 0, 3, 5, {255, 255, 255}, {255, 255, 255}, 255, 255, 20, 0, 360},
  /* portal */ {30, 0, .6f, .4f, 180, 95, 75, 20, 0, 0, 6, 2, 1, 3, 30, {0, 255, 255}, {0, 255, 255}, 128, 255, 0, 0, 0},
  /* coin sparkle */ {15, 0, .25f, .15f, 270, 0, 70, 10, 0, 0, 5, 2, 1, 6, 5, {255, 255, 0}, {255, 255, 0}, 128, 255, 0, 0, 0},
  /* coin pickup */ {50, 0, 0, .6f, 0, 360, 100, 30, 0, -400, 5, 2, 2, 10, 10, {255, 255, 0}, {255, 255, 0}, 255, 0, 0, 0, 0},
  /* level complete */ {200, 0, 0, 1.0f, 270, 180, 125, 50, 0, 0, 6, 2, 2, 200, 20, {255, 255, 255}, {255, 255, 255}, 255, 255, 0, 0, 0},
  /* firework */ {200, 0, 0, .5f, 0, 180, 180, 50, 0, 0, 6, 2, 2, 5, 5, {255, 255, 255}, {255, 255, 255}, 255, 0, 0, 0, 0},
};

typedef struct {
  float x, y, vx, vy, life, age;
  uint16_t c0, c1;
  uint8_t a0, a1, s0, s1, def, flags;
} Particle;
enum { PF_SCREEN = 1, PF_RADIAL = 2 };
static Particle parts[MAX_PARTICLES];
static int nparts;

enum { EI, EO, EL, EQ, EC };   /* ease in, ease out, linear, quad out, cubic out */
typedef struct { float dur, a0, a1, r0, r1, thick; uint8_t hollow, tri, ea0, ea1, er0, er1; } CDef;
static const CDef cdefs[] = {
  /* pad */ {.3f, 1, 0, 4, 40, 0, 0, 0, EI, EL, EQ, EQ},
  /* orb */ {.4f, .03f, .9f, 37, 3, 0, 0, 1, EC, EL, EL, EL},
  /* portal */ {.4f, .01f, .91f, 48, 0, 0, 0, 1, EC, EL, EL, EL},
  /* orb touch */ {.35f, .7f, 0, 0, 65, 1, 1, 0, EI, EO, EQ, EQ},
  /* death */ {.5f, .9f, 0, 8, 80, 0, 0, 0, EI, EL, EQ, EQ},
  /* coin */ {.4f, .03f, .9f, 30, 4, 0, 0, 1, EC, EL, EL, EL},
  /* coin ring */ {.35f, .7f, 0, 0, 65, 2.5f, 1, 0, EI, EO, EQ, EQ},
  /* wall 1 */ {.5f, 1, 0, 2.5f, 250, 0, 0, 0, EI, EO, EQ, EQ},
  /* wall 2 */ {.8f, 1, 0, 2.5f, 480, 0, 0, 0, EI, EL, EQ, EQ},
  /* title */ {1, .9f, 0, 25, 250, 0, 0, 0, EI, EL, EQ, EQ},
  /* firework */ {.5f, 0, 1, 10, 42.5f, 0, 0, 1, EC, EL, EL, EL},
  /* big ring */ {.5f, 0, 1, 2.5f, 480, 1.8f, 1, 1, EL, EL, EL, EL},
  /* respawn */ {.3f, .1f, .95f, 67, 3, 1, 1, 1, EC, EL, EL, EL},
};
typedef struct { float x, y, t; uint16_t color; uint8_t def, screen; int16_t obj; } Circle;
static Circle circles[MAX_CIRCLES];

static const Game *G;
static float cam_x, cam_y;
static uint32_t seed = 12345;
static float rnd(void) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) * (1.0f / 16777216.0f); }
static float rnd1(void) { return rnd() * 2 - 1; }

/* trail */
#define TRAIL_N 12
static float trail_x[TRAIL_N], trail_y[TRAIL_N];
static int trail_len;
static bool trail_on;
static float trail_acc;
#define GHOST_N 10
typedef struct { float x, y, rot, life; uint8_t mode, upside; } Ghost;
static Ghost ghosts[GHOST_N];
static int ghost_next;
static float ghost_acc;
static color_t col_p1, col_p2;

void fx_reset(void) {
  nparts = 0;
  memset(circles, 0, sizeof(circles));
  trail_len = 0;
  trail_on = false;
  memset(ghosts, 0, sizeof(ghosts));
}

static void spawn(int def, float x, float y, color_t c0, bool use_c0, color_t c1, bool use_c1, int flags, float angle_override) {
  if (nparts >= MAX_PARTICLES) return;
  const PDef *d = &pdefs[def];
  Particle *p = &parts[nparts++];
  float life = d->life + d->life_var * rnd1();
  if (life < 0.05f) life = 0.05f + rnd() * 0.1f;
  float ang = ((angle_override < 1e8f ? angle_override : d->angle) + d->angle_var * rnd1()) * 3.14159265f / 180.f;
  float sp = d->speed + d->speed_var * rnd1();
  p->x = x + d->pvx * rnd1();
  p->y = y + d->pvy * rnd1();
  if (d->radial) {
    p->x = x; p->y = y;
    p->vx = ang;              /* angle */
    p->vy = d->r_max;         /* start radius */
    flags |= PF_RADIAL;
  } else {
    p->vx = cosf(ang) * sp;
    p->vy = sinf(ang) * sp;
  }
  p->life = life;
  p->age = 0;
  color_t sc = use_c0 ? c0 : rgb(d->c0[0], d->c0[1], d->c0[2]);
  color_t ec = use_c1 ? c1 : rgb(d->c1[0], d->c1[1], d->c1[2]);
  p->c0 = sc; p->c1 = ec;
  p->a0 = d->a0; p->a1 = d->a1;
  float s0 = d->s0 + d->s0_var * rnd1();
  if (s0 < 1) s0 = 1;
  p->s0 = (uint8_t)(s0 * 4);
  p->s1 = (uint8_t)(d->s1 * 4);
  p->def = (uint8_t)def;
  p->flags = (uint8_t)flags;
}

static void circle(float x, float y, int def, color_t c, int obj, bool screen) {
  for (int i = 0; i < MAX_CIRCLES; i++)
    if (circles[i].def == 0 || circles[i].t >= cdefs[circles[i].def - 1].dur) {
      circles[i] = (Circle){x, y, 0, c, (uint8_t)(def + 1), screen, (int16_t)obj};
      return;
    }
}
void fx_circle_screen(float x, float y, int def, color_t c) { circle(x, y, def, c, -1, true); }
void fx_circle_world(float x, float y, int def, color_t c) { circle(x, y, def, c, -1, false); }
void fx_burst_world(float x, float y, int def, color_t c, int count) {
  for (int i = 0; i < count; i++) spawn(def, x, y, c, true, c, true, 0, 1e9f);
}
void fx_burst_screen(float x, float y, int def, color_t c, int count) {
  for (int i = 0; i < count; i++) spawn(def, x, y, c, true, c, true, PF_SCREEN, 1e9f);
}

static float ease(int e, float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  switch (e) {
    case EI: return t * t;
    case EO: return sqrtf(t);
    case EQ: return t * (2 - t);
    case EC: { float u = 1 - t; return 1 - u * u * u; }
    default: return t;
  }
}

static void emit_rate(float *acc, float rate, float dt) { *acc += rate * dt; }

static float drag_acc, fire_acc, smoke_acc, obj_acc;

void fx_update(Game *g, float dt, color_t p1, color_t p2, bool practice) {
  G = g;
  cam_x = g->cam_x;
  cam_y = g->cam_y;
  col_p1 = p1;
  col_p2 = p2;
  const Player *p = &g->p;
  /* events from physics */
  for (int k = 0; k < g->fx_count; k++) {
    const FxEvent *e = &g->fx[k];
    switch (e->kind) {
      case FX_LAND:
        if (p->mode == MODE_CUBE)
          for (int i = 0; i < 10; i++) spawn(PE_LAND, e->x, e->y + (p->upside ? 11 : -11), p1, true, 0, false, 0, p->upside ? 270 : 90);
        break;
      case FX_PAD: {
        color_t c = e->arg == 0 ? rgb(255, 200, 0) : e->arg == 1 ? rgb(0, 255, 255) : rgb(255, 50, 255);
        circle(e->x, e->y, CE_PAD, c, e->obj, false);
        trail_on = true;
        break;
      }
      case FX_ORB: {
        color_t c = e->arg == 0 ? rgb(255, 200, 0) : e->arg == 1 ? rgb(255, 50, 255) : rgb(0, 255, 255);
        circle(e->x, e->y, CE_ORB, c, e->obj, false);
        trail_on = true;
        break;
      }
      case FX_ORB_TOUCH: circle(e->x, e->y, CE_ORB_TOUCH, 0xffff, e->obj, false); break;
      case FX_PORTAL: circle(e->x, e->y, CE_PORTAL, e->arg ? rgb(255, 0, 255) : rgb(0, 255, 50), e->obj, false); break;
      case FX_GRAVITY:
        circle(e->x, e->y, CE_PORTAL, e->arg ? rgb(255, 200, 0) : rgb(0, 200, 255), e->obj, false);
        trail_on = true;
        break;
      case FX_COIN:
        if (practice) break;
        circle(e->x, e->y, CE_COIN, rgb(255, 190, 0), e->obj, false);
        circle(e->x, e->y, CE_COIN_RING, rgb(255, 190, 0), e->obj, false);
        for (int i = 0; i < 30; i++) spawn(PE_COIN_PICKUP, e->x, e->y, 0, false, 0, false, 0, 1e9f);
        break;
      case FX_DEATH:
        circle(e->x, e->y, CE_DEATH, p1, -1, false);
        for (int i = 0; i < 60; i++) spawn(PE_EXPLODE, e->x, e->y, 0, false, 0, false, 0, 1e9f);
        trail_len = 0;
        break;
      case FX_WALL:
        circle(e->x, e->y, CE_WALL1, p1, -1, false);
        break;
      default: break;
    }
  }
  g->fx_count = 0;
  /* continuous emitters */
  if (!g->dead && !g->complete) {
    if (p->mode == MODE_CUBE) {
      if (p->time_since_ground < 0.1f && !g->ending) {
        emit_rate(&drag_acc, 100, dt);
        while (drag_acc >= 1) {
          drag_acc -= 1;
          float by = p->upside ? p->y + 13 : p->y - 13;
          spawn(PE_DRAG, p->x - 15, by, p1, true, 0, false, 0, p->upside ? 270 : 90);
        }
      } else {
        drag_acc = 0;
      }
      if (p->on_ground) trail_on = false;
    } else {
      trail_on = true;
      float r = p->rot * 3.14159265f / 180.f, s = p->upside ? -1.f : 1.f;
      float lx = -14, ly = -8 * s;
      float ex = p->x + lx * cosf(r) + ly * sinf(r), ey = p->y - lx * sinf(r) + ly * cosf(r);
      if (g->hold_prev) {
        emit_rate(&fire_acc, 90, dt);
        while (fire_acc >= 1) { fire_acc -= 1; spawn(PE_SHIP_FIRE, ex, ey, rgb(255, 150, 0), true, 0, false, 0, 180 + (p->upside ? 20 : -20)); }
      }
      emit_rate(&smoke_acc, 60, dt);
      while (smoke_acc >= 1) { smoke_acc -= 1; spawn(PE_SHIP_SMOKE, ex, ey, p1, true, 0, false, 0, 1e9f); }
    }
    /* ambient particles of visible pads, orbs, portals and coins */
    emit_rate(&obj_acc, 1, dt * 40);
    int n = (int)obj_acc;
    obj_acc -= n;
    if (n > 0) {
      const Level *L = g->L;
      unsigned i = level_lower_bound(L, (int)(cam_x - 30));
      for (; i < L->count && L->objs[i].x < cam_x + VIEW_W + 30; i++) {
        const LObj *o = &L->objs[i];
        int sp = objdefs[o->type].special;
        for (int k = 0; k < n; k++) {
          if (sp == SP_PAD_Y || sp == SP_PAD_P || sp == SP_PAD_B) {
            color_t c = sp == SP_PAD_Y ? rgb(255, 255, 0) : sp == SP_PAD_B ? rgb(0, 255, 255) : rgb(255, 0, 255);
            bool down = (o->xf & 3) == 2 || (o->xf & 8);
            if (rnd() < 0.8f) spawn(PE_BUMP, o->x, o->y, c, true, 0, false, 0, down ? 270 : 90);
          } else if (sp >= SP_ORB_Y && sp <= SP_ORB_B && !game_used(g, i)) {
            color_t c = sp == SP_ORB_Y ? rgb(255, 255, 0) : sp == SP_ORB_P ? rgb(255, 0, 255) : rgb(0, 255, 255);
            if (rnd() < 0.9f) spawn(PE_RING, o->x, o->y, c, true, c, true, 0, 1e9f);
          } else if (sp >= SP_GRAV_N && sp <= SP_PORTAL_SHIP) {
            color_t c = sp == SP_GRAV_N ? rgb(0, 255, 255) : sp == SP_GRAV_F ? rgb(255, 255, 0) : sp == SP_PORTAL_CUBE ? rgb(0, 255, 0) : rgb(255, 0, 255);
            if (rnd() < 0.6f) spawn(PE_PORTAL, o->x + 14, o->y, c, true, c, true, 0, 180);
          } else if (sp == SP_COIN && !game_used(g, i)) {
            if (rnd() < 0.5f) spawn(PE_COIN, o->x, o->y - 10, 0, false, 0, false, 0, 1e9f);
          }
        }
      }
    }
  }
  /* motion trail samples */
  trail_acc += dt;
  bool sample = trail_acc >= 1.0f / 40;
  if (sample) trail_acc -= 1.0f / 40;
  if (trail_acc > 0.1f) trail_acc = 0;
  if (!g->dead && (trail_on || g->ending) && !(p->mode == MODE_CUBE && p->on_ground)) {
    if (sample) {
      if (trail_len < TRAIL_N) trail_len++;
      for (int i = trail_len - 1; i > 0; i--) { trail_x[i] = trail_x[i - 1]; trail_y[i] = trail_y[i - 1]; }
    }
    trail_x[0] = p->x;
    trail_y[0] = p->y;
  } else if (trail_len > 0 && sample) {
    trail_len--;
  }
  /* ghost trail (Polargeist style afterimages) */
  for (int i = 0; i < GHOST_N; i++) if (ghosts[i].life > 0) ghosts[i].life -= dt;
  if ((g->trail || g->ending) && !g->dead) {
    ghost_acc += dt;
    if (ghost_acc >= 0.05f) {
      ghost_acc = 0;
      ghosts[ghost_next] = (Ghost){p->x, p->y, p->rot, 0.45f, p->mode, p->upside};
      ghost_next = (ghost_next + 1) % GHOST_N;
    }
  }
  /* advance particles */
  for (int i = 0; i < nparts;) {
    Particle *q = &parts[i];
    q->age += dt;
    if (q->age >= q->life) { parts[i] = parts[--nparts]; continue; }
    const PDef *d = &pdefs[q->def];
    if (!(q->flags & PF_RADIAL)) {
      q->vx += d->gx * dt;
      q->vy += d->gy * dt;
      q->x += q->vx * dt;
      q->y += q->vy * dt;
    }
    i++;
  }
  for (int i = 0; i < MAX_CIRCLES; i++) if (circles[i].def) circles[i].t += dt;
}

void fx_update_screen(float dt) {
  for (int i = 0; i < nparts;) {
    Particle *q = &parts[i];
    q->age += dt;
    if (q->age >= q->life) { parts[i] = parts[--nparts]; continue; }
    const PDef *d = &pdefs[q->def];
    q->vx += d->gx * dt;
    q->vy += d->gy * dt;
    q->x += q->vx * dt;
    q->y += q->vy * dt;
    i++;
  }
  for (int i = 0; i < MAX_CIRCLES; i++) if (circles[i].def) circles[i].t += dt;
}

static void to_screen(float wx, float wy, bool screen, float *sx, float *sy) {
  if (screen) { *sx = wx; *sy = wy; return; }
  *sx = (wx - cam_x) * PX;
  *sy = 240 - (GROUND_OFFSET + wy - cam_y) * PX;
}

static void draw_particles(bool screen_pass) {
  for (int i = 0; i < nparts; i++) {
    const Particle *q = &parts[i];
    bool scr = (q->flags & PF_SCREEN) != 0;
    if (scr != screen_pass) continue;
    float t = q->age / q->life;
    float wx = q->x, wy = q->y;
    if (q->flags & PF_RADIAL) {
      const PDef *d = &pdefs[q->def];
      float r = q->vy + (d->r_min - q->vy) * t, a = q->vx + d->rot * 3.14159265f / 180.f * q->age;
      wx += cosf(a) * r;
      wy += sinf(a) * r;
    }
    float sx, sy;
    to_screen(wx, wy, scr, &sx, &sy);
    float size = (q->s0 + (q->s1 - q->s0) * t) / 4.f * (scr ? 1 : PX);
    if (size < 0.6f) continue;
    int s = (int)(size + .5f), x = (int)(sx - size / 2), y = (int)(sy - size / 2);
    if (y + s < gfx_y0 || y > gfx_y1) continue;
    color_t c = c_mix(q->c0, q->c1, (unsigned)(t * 256));
    unsigned a = (unsigned)(q->a0 + (q->a1 - q->a0) * t);
    gfx_add(x, y, s < 1 ? 1 : s, s < 1 ? 1 : s, c, a);
  }
}

static void draw_circles(bool screen_pass) {
  for (int i = 0; i < MAX_CIRCLES; i++) {
    const Circle *c = &circles[i];
    if (!c->def || (c->screen != 0) != screen_pass) continue;
    const CDef *d = &cdefs[c->def - 1];
    if (c->t >= d->dur) continue;
    float t = c->t / d->dur, a, r;
    if (d->tri) {
      a = t < .5f ? d->a0 + (d->a1 - d->a0) * ease(d->ea0, t * 2) : d->a1 + (d->a0 - d->a1) * ease(d->ea1, (t - .5f) * 2);
    } else if (d->ea0 == d->ea1) {
      a = d->a0 + (d->a1 - d->a0) * ease(d->ea0, t);
    } else {
      float mid = (d->a0 + d->a1) / 2;
      a = t < .5f ? d->a0 + (mid - d->a0) * ease(d->ea0, t * 2) : mid + (d->a1 - mid) * ease(d->ea1, (t - .5f) * 2);
    }
    if (d->er0 == d->er1) r = d->r0 + (d->r1 - d->r0) * ease(d->er0, t);
    else {
      float mid = (d->r0 + d->r1) / 2;
      r = t < .5f ? d->r0 + (mid - d->r0) * ease(d->er0, t * 2) : mid + (d->r1 - mid) * ease(d->er1, (t - .5f) * 2);
    }
    float sx, sy;
    to_screen(c->x, c->y, c->screen, &sx, &sy);
    float scale = c->screen ? 1 : PX;
    if (sy + r * scale < gfx_y0 - 2 || sy - r * scale > gfx_y1 + 2) continue;
    unsigned al = (unsigned)(a * 256);
    if (al > 256) al = 256;
    if (d->hollow)
      gfx_ring((int)(sx * 16), (int)(sy * 16), (int)(r * scale * 16), (int)(d->thick * 16 * (scale < 1 ? 1.2f : 1)), c->color, al, BLEND_ADD);
    else
      gfx_disc((int)(sx * 16), (int)(sy * 16), (int)(r * scale * 16), c->color, al, BLEND_ADD);
  }
}

void fx_draw_player_trail(void) {
  if (!G) return;
  /* afterimages */
  for (int i = 0; i < GHOST_N; i++) {
    const Ghost *gh = &ghosts[i];
    if (gh->life <= 0) continue;
    float sx, sy;
    to_screen(gh->x, gh->y, false, &sx, &sy);
    float k = gh->life / 0.45f;
    scene_draw_player_icon(gh->mode == MODE_SHIP ? MODE_CUBE : MODE_CUBE, sx, sy, gh->rot, gh->mode == MODE_SHIP ? 0.5f + 0.4f * k : 0.8f + 0.2f * k,
                           col_p1, col_p2, gh->upside, (unsigned)(k * 0.7f * 256));
  }
  /* streak: a tapered ribbon through the recent positions */
  for (int i = trail_len - 1; i > 0; i--) {
    float x0, y0, x1, y1;
    to_screen(trail_x[i], trail_y[i], false, &x0, &y0);
    to_screen(trail_x[i - 1], trail_y[i - 1], false, &x1, &y1);
    if ((y0 < gfx_y0 - 8 && y1 < gfx_y0 - 8) || (y0 > gfx_y1 + 8 && y1 > gfx_y1 + 8)) continue;
    float k0 = 1 - (float)i / TRAIL_N, k1 = 1 - (float)(i - 1) / TRAIL_N;
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    int steps = (int)(len / 1.5f) + 1;
    for (int s = 0; s < steps; s++) {
      float t = (float)s / steps, k = k0 + (k1 - k0) * t;
      gfx_disc((int)((x0 + dx * t) * 16), (int)((y0 + dy * t) * 16), (int)(k * 4.0f * 16), col_p1, (unsigned)(k * 38), BLEND_ADD);
    }
  }
}

void fx_draw(void) {
  draw_circles(false);
  draw_particles(false);
}

void fx_draw_screen(void) {
  draw_circles(true);
  draw_particles(true);
}
