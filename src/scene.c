#include "scene.h"
#include "fx.h"
#include <math.h>
#include <string.h>

#define PX (2.0f / 3.0f)       /* screen pixels per GD unit */
#define FADE_W 75.0f

/* ------------------------------------------------------------ background */

void scene_background(color_t bg, float bg_x, float cam_y) {
  const float tex = 0.9f, tile = 512 * tex;
  float off = fmodf(bg_x * 0.1f, tile);
  if (off < 0) off += tile;
  float top = cam_y * 0.111f - 200;
  int y0 = gfx_y0, y1 = gfx_y1;
  for (int y = y0; y < y1; y++) {
    float ty = (y + .5f - top) / tex;
    if (ty < 0) ty = 0;
    if (ty > 511) ty = 511;
    float g = ty / 384.f;
    if (g > 1) g = 1;
    unsigned gi = (unsigned)(g * 256);
    color_t gap = c_scale(bg, gi), sq = c_scale(bg, gi * 216 / 256), sh = c_scale(bg, gi * 182 / 256);
    gfx_fill(0, y, GFX_W, 1, gap);
    int ity = (int)ty;
    for (float base = -off; base < GFX_W; base += tile) {
      for (int k = 0; k < BG_RECTS; k++) {
        const uint16_t *r = bg_rects[k];
        if (ity < r[1] || ity >= r[1] + r[3]) continue;
        int a = (int)floorf(base + r[0] * tex + .5f), b = (int)floorf(base + (r[0] + r[2]) * tex + .5f);
        if (b <= 0 || a >= GFX_W) continue;
        gfx_fill(a, y, b - a - 1, 1, sq);
        gfx_fill(b - 1, y, 1, 1, sh);
      }
    }
  }
}

static const uint8_t line_profile[23] = {3, 44, 87, 128, 170, 195, 204, 214, 223, 232, 241, 250, 243, 233, 224, 215, 206, 197, 176, 135, 92, 51, 9};

/* Ground band whose top edge (or bottom edge for a ceiling) is at top_y. */
void scene_ground(color_t g1, color_t line, float ground_x, int top_y, bool ceiling, bool shadows) {
  const float tile = 128 * PX;
  float off = fmodf(ground_x * PX, tile);
  if (off < 0) off += tile;
  int ya = ceiling ? 0 : top_y, yb = ceiling ? top_y : GFX_H;
  if (ya < gfx_y0) ya = gfx_y0;
  if (yb > gfx_y1) yb = gfx_y1;
  for (int y = ya; y < yb; y++) {
    float d = ceiling ? (top_y - 1 - y) : (y - top_y);   /* depth in px */
    float t = d * 1.5f;                                   /* depth in units */
    float in = t < 2 ? 1.0f : 0.85f - (t - 2) / 126.f * 0.65f;
    if (in < 0.2f) in = 0.2f;
    color_t base = c_scale(g1, (unsigned)(in * 256)), seam = c_scale(g1, (unsigned)((in + 0.13f > 1 ? 1 : in + 0.13f) * 256));
    gfx_fill(0, y, GFX_W, 1, base);
    for (float x = -off; x < GFX_W + tile; x += tile) {
      int s = (int)floorf(x + .5f);
      gfx_fill(s - 4, y, 8, 1, seam);
    }
  }
  if (shadows) {
    int h = ceiling ? top_y : GFX_H - top_y;
    int y = ceiling ? 0 : top_y;
    gfx_hgrad_alpha(0, y, 86, h, 0, 100, 0, BLEND_NORMAL);
    gfx_hgrad_alpha(GFX_W - 86, y, 86, h, 0, 0, 100, BLEND_NORMAL);
  }
  /* floor line: additive, brightest in the middle of the screen */
  int ly = ceiling ? top_y - 1 : top_y;
  if (ly >= gfx_y0 - 1 && ly < gfx_y1 + 1) {
    const int len = 296, x0 = (GFX_W - len) / 2;
    for (int i = 0; i < len; i++) {
      float f = i * 22.f / (len - 1);
      int k = (int)f;
      float fr = f - k;
      unsigned a = (unsigned)(line_profile[k] * (1 - fr) + line_profile[k < 22 ? k + 1 : 22] * fr);
      gfx_add(x0 + i, ly, 1, 1, line, a);
      gfx_add(x0 + i, ceiling ? ly - 1 : ly + 1, 1, 1, line, a / 3);
    }
  }
}

/* ------------------------------------------------------------ objects */

typedef struct {
  int16_t x, y;          /* anchor position, px */
  uint8_t spr, xf, ct, alpha, layer, kind;
  uint16_t scale;        /* 256 = 1 */
} Item;
enum { K_SPRITE, K_SCALED, K_PLAYER };
#define MAX_ITEMS 1100
static Item items[MAX_ITEMS];
static uint16_t order[MAX_ITEMS];
static int nitems;
static color_t tint_obj, tint_p1, tint_p2;
static const Game *G;
static SceneOpts O;
static float player_sx, player_sy;
static color_t bg_col, g1_col, line_col;
static int ground_top, ceil_bottom, floor_bottom;
static bool draw_gfx_grounds;

color_t scene_channel(const Game *g, int ch) { return rgb(g->ch[ch].cur[0], g->ch[ch].cur[1], g->ch[ch].cur[2]); }

static int fade_at(const Level *L, float x) {
  int eff = 0;
  for (unsigned k = 0; k < L->event_count; k++) {
    const LevelEvent *e = &L->events[k];
    if (e->x >= x) break;
    if (e->kind == EV_FADE) eff = e->arg;
  }
  return eff;
}

static void push(int x, int y, int spr, int xf, int ct, int alpha, int layer, int kind, int scale) {
  if (nitems >= MAX_ITEMS || spr < 0) return;
  items[nitems++] = (Item){(int16_t)x, (int16_t)y, (uint8_t)spr, (uint8_t)xf, (uint8_t)ct, (uint8_t)(alpha > 255 ? 255 : alpha),
                           (uint8_t)layer, (uint8_t)kind, (uint16_t)scale};
}

static float pulse_amount(void) {
  unsigned bpm = G && G->L->bpm ? G->L->bpm : 120;
  float beat = O.time * bpm / 60.f;
  float ph = beat - floorf(beat);
  return ph < 0.25f ? 1 - ph * 4 : 0;
}

/* Rotate a part offset (units) by the object's transform. */
static void part_offset(int xf, float dx, float dy, float *ox, float *oy) {
  if (xf & XF_FLIPX) dx = -dx;
  if (xf & XF_FLIPY) dy = -dy;
  switch (xf & 3) {
    case 1: *ox = dy; *oy = -dx; break;       /* clockwise 90: (x,y) -> (y,-x) in y-up space */
    case 2: *ox = -dx; *oy = -dy; break;
    case 3: *ox = -dy; *oy = dx; break;
    default: *ox = dx; *oy = dy; break;
  }
}

void scene_prepare(const Game *g, const SceneOpts *o) {
  G = g;
  O = *o;
  nitems = 0;
  const Level *L = g->L;
  bg_col = scene_channel(g, CH_BG);
  g1_col = scene_channel(g, CH_G1);
  line_col = scene_channel(g, CH_LINE);
  tint_obj = scene_channel(g, CH_OBJ);
  tint_p1 = o->p1;
  tint_p2 = o->p2;
  float cam_x = g->cam_x;
  unsigned i = level_lower_bound(L, (int)(cam_x - 90));
  float pulse = pulse_amount();
  unsigned coin_frame = (unsigned)(o->time * 8) & 3;
  for (; i < L->count && L->objs[i].x < cam_x + VIEW_W + 90; i++) {
    const LObj *ob = &L->objs[i];
    const ObjDef *d = &objdefs[ob->type];
    if (d->special == SP_COIN && game_used(g, i)) continue;
    if (o->low_detail && d->hit == HIT_NONE && !o->editor) continue;
    float rel = ob->x - cam_x;
    float fade = o->editor ? 1 : rel < 0 || rel > VIEW_W ? 0 : rel < FADE_W ? rel / FADE_W : rel > VIEW_W - FADE_W ? (VIEW_W - rel) / FADE_W : 1;
    if (fade <= 0) continue;
    float offx = 0, offy = 0, sc = 1;
    if (fade < 1) {
      int eff = fade_at(L, rel < VIEW_W / 2 ? ob->x + 75 : ob->x - (VIEW_W - PLAYER_SCREEN_X) + 75);
      float off = (1 - fade) * 127.5f;
      switch (eff) {
        case 1: offy = -off; break;          /* rises into place from below */
        case 2: offy = off; break;
        case 3: offx = -off; break;
        case 4: offx = off; break;
        case 5: sc = fade; break;
        case 6: sc = 1 + (1 - fade) / 2; break;
        default: break;
      }
    }
    float wx = ob->x + offx, wy = ob->y + offy;
    int alpha = (int)(fade * 255 + .5f);
    if (d->special == SP_COIN && (o->coins_saved >> game_coin_index(L, i) & 1)) alpha = alpha * 2 / 5;
    for (int k = 0; k < d->nparts; k++) {
      const ObjPart *pt = &d->parts[k];
      if (o->low_detail && pt->ctype >= CT_GLOW) continue;
      int spr = pt->sprite;
      if (pt->flags & PF_RANDOM3) spr += (int)(i % 3);
      if (pt->flags & PF_COIN) spr += (int)coin_frame;
      float ox, oy;
      part_offset(ob->xf, pt->dx4 / 4.f * sc, pt->dy4 / 4.f * sc, &ox, &oy);
      float sx = (wx + ox - cam_x) * PX, sy = 240 - (GROUND_OFFSET + wy + oy - g->cam_y) * PX;
      int psc = (int)(sc * 256);
      if (pt->flags & PF_PULSE) psc = (int)(psc * (1 + 0.22f * pulse));
      int kind = psc != 256 ? K_SCALED : K_SPRITE;
      push((int)floorf(sx + .5f), (int)floorf(sy + .5f), spr, ob->xf, pt->ctype, alpha, pt->layer, kind, psc);
    }
  }
  /* the player */
  player_sx = wx_to_sx(g, g->p.x);
  player_sy = wy_to_sy(g, g->p.y);
  if (!g->dead && !o->hide_player) push(0, 0, 0, 0, 0, 255, LAYER_PLAYER, K_PLAYER, 256);
  /* sort by layer, stable */
  uint16_t count[LAYER_COUNT + 1] = {0};
  for (int k = 0; k < nitems; k++) count[items[k].layer + 1]++;
  for (int k = 1; k <= LAYER_COUNT; k++) count[k] += count[k - 1];
  for (int k = 0; k < nitems; k++) order[count[items[k].layer]++] = (uint16_t)k;
  /* ground positions */
  ground_top = (int)floorf(wy_to_sy(g, 0) + .5f);
  draw_gfx_grounds = g->ground_gfx > 2;
  floor_bottom = (int)floorf(240 - g->ground_gfx * PX + .5f);
  ceil_bottom = (int)floorf(g->ground_gfx * PX + .5f);
}

static void tint_for(int ct, color_t *c, int *mode) {
  *mode = BLEND_NORMAL;
  switch (ct) {
    case CT_OBJ: *c = tint_obj; break;
    case CT_BLACK: *c = 0; break;
    case CT_WHITE: *c = 0xffff; break;
    case CT_P1ADD: *c = tint_p1; *mode = BLEND_ADD; break;
    case CT_P2ADD: *c = tint_p2; *mode = BLEND_ADD; break;
    case CT_GLOW: *c = tint_obj; *mode = BLEND_ADD; break;
    case CT_GLOW_Y: *c = rgb(255, 255, 0); *mode = BLEND_ADD; break;
    case CT_GLOW_B: *c = rgb(0, 255, 255); *mode = BLEND_ADD; break;
    default: *c = rgb(255, 0, 255); *mode = BLEND_ADD; break;
  }
}

void scene_draw_player_icon(int mode, float cx, float cy, float rot, float scale, color_t p1, color_t p2, bool upside, unsigned alpha) {
  int x16 = (int)(cx * 16), y16 = (int)(cy * 16), a16 = (int)(rot * 16);
  if (mode == MODE_SHIP) {
    float r = rot * 3.14159265f / 180.f, s = upside ? -1.f : 1.f;
    /* the pilot sits in the cockpit, scaled down with the ship */
    float lx = 0, ly = -6.5f * PX * s * scale;
    float px_ = cx + lx * cosf(r) - ly * sinf(r), py_ = cy + lx * sinf(r) + ly * cosf(r);
    int fl = upside ? XF_FLIPY : 0;
    gfx_sprite_ex(SPR_CUBE1_S, (int)(px_ * 16), (int)(py_ * 16), a16, (int)(128 * scale), fl, p2, alpha, BLEND_NORMAL);
    gfx_sprite_ex(SPR_CUBE1_P, (int)(px_ * 16), (int)(py_ * 16), a16, (int)(128 * scale), fl, p1, alpha, BLEND_NORMAL);
    gfx_sprite_ex(SPR_SHIP1_S, x16, y16 + (int)(2 * 16 * s * scale), a16, (int)(256 * scale), fl, p2, alpha, BLEND_NORMAL);
    gfx_sprite_ex(SPR_SHIP1_P, x16, y16 + (int)(2 * 16 * s * scale), a16, (int)(256 * scale), fl, p1, alpha, BLEND_NORMAL);
    return;
  }
  gfx_sprite_ex(SPR_CUBE1_S, x16, y16, a16, (int)(256 * scale), 0, p2, alpha, BLEND_NORMAL);
  gfx_sprite_ex(SPR_CUBE1_P, x16, y16, a16, (int)(256 * scale), 0, p1, alpha, BLEND_NORMAL);
}

static void draw_player(void) {
  const Player *p = &G->p;
  fx_draw_player_trail();
  scene_draw_player_icon(p->mode, player_sx, player_sy, p->rot, 1, O.p1, O.p2, p->upside, 256);
}

static void draw_end_wall(void) {
  if (G->wall_y <= 0) return;
  int x = (int)floorf(wx_to_sx(G, G->L->wall_x) + .5f);
  if (x > GFX_W + 20) return;
  float oy = fmodf(G->cam_y, 30);
  for (float wy = -30; wy < VIEW_H + 60; wy += 30) {
    int y = (int)floorf(240 - (wy - oy + 15) * PX + .5f);
    gfx_sprite(SPR_GRID_T, x, y, 3, tint_obj, 255, BLEND_NORMAL);
  }
  gfx_hgrad_alpha(x - 26, 0, 20, GFX_H, O.p1, 0, 170, BLEND_ADD);
}

static void draw_attempt(void) {
  if (O.editor || !O.attempt) return;
  float sx = wx_to_sx(G, O.attempt_x);
  if (sx < -200 || sx > 520) return;
  char buf[24] = "ATTEMPT ";
  gfx_format_uint(buf + 8, O.attempt);
  int base = (int)floorf(wy_to_sy(G, O.attempt_y) + fonts[FONT_HUGE].cap / 2 + .5f);
  gfx_text_center(FONT_HUGE, (int)sx, base, buf, 0xffff, 0xffff, 256);
}

static void draw_hud(void) {
  if (O.editor) return;
  /* progress bar, top centre */
  float prog = game_progress(G);
  const int bw = 160, bh = 8, bx = (GFX_W - bw) / 2, by = 5;
  if (O.show_bar) {
    gfx_round_rect(bx - 2, by - 2, bw + 4, bh + 4, 5, 0xffff, 170);
    gfx_round_rect(bx, by, bw, bh, 4, 0, 200);
    int fw = (int)(prog / 100 * (bw - 2));
    if (fw > 0) {
      color_t c = O.p1 ? O.p1 : 0xffff;   /* the player's colour, as in 2.2 */
      int w = fw < 6 ? 6 : fw;
      gfx_round_rect(bx + 1, by + 1, w, bh - 2, 3, c, 256);
      gfx_add(bx + 3, by + 2, w - 4, 1, 0xffff, 90);
    }
  }
  if (O.show_percent) {
    char buf[8];
    int n = gfx_format_uint(buf, (unsigned long)prog);
    buf[n++] = '%';
    buf[n] = 0;
    if (O.show_bar) gfx_text(FONT_SMALL, bx + bw + 6, by + 8, buf, 0xffff, 0xffff, 256);
    else gfx_text_center(FONT_SMALL, GFX_W / 2, by + 8, buf, 0xffff, 0xffff, 256);
  }
  /* practice checkpoints: green diamonds */
  for (unsigned k = 0; k < O.checkpoint_count; k++) {
    const Checkpoint *c = &O.checkpoints[k];
    float sx = wx_to_sx(G, c->p.x), sy = wy_to_sy(G, c->p.y);
    if (sx < -10 || sx > GFX_W + 10 || sy < gfx_y0 - 12 || sy > gfx_y1 + 12) continue;
    float d0[8] = {sx, sy - 10, sx + 7, sy, sx, sy + 10, sx - 7, sy};
    float d1[8] = {sx, sy - 7.5f, sx + 5, sy, sx, sy + 7.5f, sx - 5, sy};
    float d2[6] = {sx, sy - 7.5f, sx + 5, sy, sx - 5, sy};
    gfx_poly(d0, 4, 0, 256, BLEND_NORMAL);
    gfx_poly(d1, 4, rgb(0, 190, 40), 256, BLEND_NORMAL);
    gfx_poly(d2, 3, rgb(120, 255, 120), 256, BLEND_NORMAL);
  }
}

/* Editor grid: 30 unit cells above the ground. */
static void draw_grid(void) {
  float x0 = fmodf(-G->cam_x, 30);
  if (x0 > 0) x0 -= 30;
  for (float wx = x0; wx < VIEW_W + 30; wx += 30) {
    int sx = (int)floorf(wx * PX + .5f);
    gfx_blend(sx, 0, 1, ground_top, 0, 70);
  }
  for (int row = 0; row < 90; row++) {
    int sy = (int)floorf(wy_to_sy(G, row * 30.0f) + .5f);
    if (sy < gfx_y0 - 1) break;
    if (sy <= gfx_y1) gfx_blend(0, sy, GFX_W, 1, 0, 70);
  }
}

void scene_draw(void) {
  if (!G) return;
  scene_background(bg_col, G->bg_x, G->cam_y);
  if (O.editor) draw_grid();
  for (int k = 0; k < nitems; k++) {
    const Item *it = &items[order[k]];
    if (it->kind == K_PLAYER) { draw_player(); continue; }
    const Sprite *s = &sprites[it->spr];
    int ext = (s->w > s->h ? s->w : s->h) * (it->scale > 256 ? it->scale : 256) / 256 + 2;
    if (it->y + ext < gfx_y0 || it->y - ext > gfx_y1) continue;
    color_t c;
    int mode;
    tint_for(it->ct, &c, &mode);
    if (it->kind == K_SCALED) {
      /* 90 degree transforms are expressed as rotation + flips for the bilinear path */
      gfx_sprite_ex(it->spr, it->x * 16, it->y * 16, (it->xf & 3) * 90 * 16, it->scale, it->xf & (XF_FLIPX | XF_FLIPY), c, it->alpha + 1, mode);
    } else {
      gfx_sprite(it->spr, it->x, it->y, it->xf, c, it->alpha + 1, mode);
    }
  }
  draw_end_wall();
  draw_attempt();
  if (ground_top < GFX_H) scene_ground(g1_col, line_col, G->ground_x, ground_top, false, true);
  if (draw_gfx_grounds) {
    if (floor_bottom < ground_top) scene_ground(g1_col, line_col, G->ground_x, floor_bottom, false, true);
    scene_ground(g1_col, line_col, G->ground_x, ceil_bottom, true, true);
  }
  fx_draw();
  draw_hud();
  if (O.fade > 0) gfx_scale_rect(0, 0, GFX_W, GFX_H, (unsigned)((1 - O.fade) * 256));
}
