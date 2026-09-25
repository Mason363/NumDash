#ifndef NUMDASH_GAME_H
#define NUMDASH_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ND_HZ 240
#define ND_BUILTINS 7
#define ND_SLOTS 3
#define ND_CUSTOM_MAX 384
#define ND_MAX_OBJECTS 4096
#define ND_USED_BYTES (ND_MAX_OBJECTS / 8)

/* Coordinates are original Geometry Dash units, 30 units per block. */
typedef struct {
  int16_t x, y;
  uint16_t id;
  uint8_t rot, flags;
  uint16_t color, duration;
} Object;
typedef struct {
  const char *name, *credit;
  const Object *objects;
  uint16_t count, length, background, ground;
  uint8_t difficulty, bpm;
} Level;
extern const Level nd_levels[ND_BUILTINS];

typedef enum { DECOR, SOLID, HAZARD, PAD, ORB, GRAVITY, PORTAL, COIN, COLOR } Kind;
typedef struct { float w, h; Kind kind; } Shape;
Shape object_shape(const Object *o);
bool level_valid(const Level *level);
unsigned level_lower_bound(const Level *l, float x);

typedef struct {
  float x, y, vy, camera_y, floor, ceiling;
  uint32_t tick;
  uint16_t jumps;
  uint16_t first, death_object, bg, ground, bg_from, ground_from;
  uint16_t bg_target, ground_target, bg_time, ground_time, bg_elapsed, ground_elapsed;
  uint8_t used[ND_USED_BYTES];
  uint8_t mode, coins, rotation;
  bool inverted, grounded, dead, complete, held, orb_armed;
} Player;
void player_start(Player *p, const Level *l);
void player_step(Player *p, const Level *l, bool down);
int player_progress(const Player *p, const Level *l);
bool player_used(const Player *p, unsigned i);
uint16_t color_mix(uint16_t a, uint16_t b, unsigned n, unsigned d);

typedef struct {
  Object objects[ND_CUSTOM_MAX];
  uint16_t count, length;
  uint8_t theme, bpm;
} CustomLevel;
typedef struct {
  uint8_t best[ND_BUILTINS + ND_SLOTS], practice[ND_BUILTINS + ND_SLOTS];
  uint8_t coins[ND_BUILTINS + ND_SLOTS], effects, percent, fps;
  uint32_t attempts[ND_BUILTINS + ND_SLOTS];
  CustomLevel custom[ND_SLOTS];
} SaveData;
void save_defaults(SaveData *s);
size_t save_encode(const SaveData *s, uint8_t *out, size_t cap);
bool save_decode(SaveData *s, const uint8_t *data, size_t size);
uint32_t nd_crc32(const uint8_t *data, size_t size);
Level custom_level(const CustomLevel *c, unsigned slot);
bool editor_put(CustomLevel *c, Object o);
bool editor_remove(CustomLevel *c, int x, int y);

/* Validates the entire record arena before reading or modifying it. */
bool storage_read(const uint8_t *arena, size_t size, const char *name,
                  const uint8_t **data, size_t *len);
bool storage_write(uint8_t *arena, size_t size, const char *name,
                   const uint8_t *data, size_t len);
#endif
