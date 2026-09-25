#ifndef NUMDASH_LEVEL_H
#define NUMDASH_LEVEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "objdefs.h"

#define LEVEL_COUNT 7
#define MAX_OBJECTS 4096

/* Runtime object: GD units, sorted by x. xf: rotation (90 degree steps,
 * clockwise) in bits 0-1, flip x bit 2, flip y bit 3. Matches the 6-byte
 * records produced by tools/levels.py, so levels inflate in place. */
typedef struct { uint16_t x; int16_t y; uint8_t type, xf; } LObj;
typedef struct { uint16_t x; int16_t y; uint8_t kind, arg, r, g, b, flags; uint16_t dur; } LevelEvent;
enum { EVF_TOUCH = 1, EVF_TINT_GROUND = 2, EVF_BLEND = 4 };
enum { CH_BG = 0, CH_G1 = 1, CH_LINE = 2, CH_OBJ = 3, CH_COUNT = 4 };

typedef struct {
  const char *name;
  const uint8_t *data;
  const LevelEvent *events;
  uint16_t data_len, count, event_count, end_x, wall_x, reserved;
  uint8_t bg[3], g1[3], line[3], obj[3];
  uint8_t difficulty, stars, bpm, start_mode;
} LevelDef;
extern const LevelDef level_defs[LEVEL_COUNT];

typedef struct {
  const char *name;
  const LObj *objs;
  const LevelEvent *events;
  uint16_t count, event_count, end_x, wall_x;
  uint8_t colors[CH_COUNT][3];
  uint8_t bpm, start_mode, difficulty, stars;
  uint16_t coin_obj[3];     /* object indices of the secret coins, in order */
  uint8_t coin_count;
} Level;
/* Fills coin_obj / coin_count from the object list. */
void level_index_coins(Level *L);

extern LObj level_objs[MAX_OBJECTS];
int inflate_raw(uint8_t *out, size_t cap, const uint8_t *in, size_t len);
bool level_load_builtin(Level *L, unsigned index);
unsigned level_lower_bound(const Level *L, int x);
bool level_valid(const Level *L);
/* Rotated hitbox half extents in GD units. */
void obj_hitbox(const LObj *o, float *hw, float *hh);
#endif
