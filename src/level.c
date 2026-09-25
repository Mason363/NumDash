#include "level.h"
#include <string.h>

LObj level_objs[MAX_OBJECTS];

bool level_load_builtin(Level *L, unsigned index) {
  if (index >= LEVEL_COUNT) return false;
  const LevelDef *d = &level_defs[index];
  if (d->count > MAX_OBJECTS) return false;
  int n = inflate_raw((uint8_t *)level_objs, sizeof(level_objs), d->data, d->data_len);
  if (n != (int)(d->count * sizeof(LObj))) return false;
  unsigned x = 0;
  for (unsigned i = 0; i < d->count; i++) {
    x += level_objs[i].x;
    if (x > 0xffff) return false;
    level_objs[i].x = (uint16_t)x;
  }
  memset(L, 0, sizeof(*L));
  L->name = d->name;
  L->objs = level_objs;
  L->count = d->count;
  L->events = d->events;
  L->event_count = d->event_count;
  L->end_x = d->end_x;
  L->wall_x = d->wall_x;
  memcpy(L->colors[CH_BG], d->bg, 3);
  memcpy(L->colors[CH_G1], d->g1, 3);
  memcpy(L->colors[CH_LINE], d->line, 3);
  memcpy(L->colors[CH_OBJ], d->obj, 3);
  L->bpm = d->bpm;
  L->start_mode = d->start_mode;
  L->difficulty = d->difficulty;
  L->stars = d->stars;
  level_index_coins(L);
  return level_valid(L);
}

void level_index_coins(Level *L) {
  L->coin_count = 0;
  for (unsigned i = 0; i < L->count && L->coin_count < 3; i++)
    if (L->objs[i].type == OT_COIN) L->coin_obj[L->coin_count++] = (uint16_t)i;
}

unsigned level_lower_bound(const Level *L, int x) {
  unsigned a = 0, b = L->count;
  while (a < b) {
    unsigned m = (a + b) / 2;
    if (L->objs[m].x < x) a = m + 1; else b = m;
  }
  return a;
}

bool level_valid(const Level *L) {
  if (!L || !L->objs || L->count > MAX_OBJECTS || L->end_x < 300 || L->wall_x < L->end_x - 30) return false;
  for (unsigned i = 0; i < L->count; i++) {
    const LObj *o = &L->objs[i];
    if (!o->type || o->type >= OT_COUNT || o->y < -600 || o->y > 3000 || (o->xf & 0xf0) || (i && o->x < L->objs[i - 1].x))
      return false;
  }
  for (unsigned i = 0; i < L->event_count; i++)
    if (!L->events[i].kind || L->events[i].kind > EV_TRAIL) return false;
  return true;
}

void obj_hitbox(const LObj *o, float *hw, float *hh) {
  const ObjDef *d = &objdefs[o->type];
  float w = d->w10 * 0.05f, h = d->h10 * 0.05f;
  if (o->xf & 1) { *hw = h; *hh = w; } else { *hw = w; *hh = h; }
}
