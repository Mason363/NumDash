#ifndef NUMDASH_GAME_H
#define NUMDASH_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "level.h"

#define ND_HZ 240
#define ND_DT (1.0f / ND_HZ)
enum { MODE_CUBE = 0, MODE_SHIP = 1 };
enum { BUF_NONE = 0, BUF_READY = 1, BUF_END = 2 };
/* Visible world: 480 x 360 GD units (20 px blocks); ground sits 90 units
 * above the bottom edge when the camera is at rest, as in GD. */
#define VIEW_W 480.0f
#define VIEW_H 360.0f
#define GROUND_OFFSET 90.0f
#define PLAYER_SCREEN_X 150.0f

typedef struct {
  float x, y, vy, gravity, rot, target_rot, time_since_ground, ceil_inv;
  float ground_y, ceiling_y, snap_diff;
  int32_t snap_frame, frame, coyote;
  int16_t snap_obj;
  uint8_t mode, speed, buffer;
  bool upside, on_ground, on_ceiling, vel_override, snap_rot, left_ground, inverse_rot, jumped, rot_dir_neg;
} Player;

typedef struct { uint8_t cur[3], from[3], to[3]; uint16_t t, dur; } Channel; /* t, dur in steps */

enum { FX_JUMP, FX_LAND, FX_PAD, FX_ORB, FX_PORTAL, FX_COIN, FX_DEATH, FX_GRAVITY, FX_ORB_TOUCH, FX_WALL };
typedef struct { uint8_t kind, arg; int16_t obj; float x, y; } FxEvent;

typedef struct {
  Player p;
  const Level *L;
  float cam_x, cam_y, ground_x, bg_x, ground_gfx, cam_intended_y, wall_y, end_t, end_x0, end_y0, cam_wall_t;
  float cam_wall_y0, bg_wall_x0, ground_wall_x0, shake_t, shake_amp;
  Channel ch[CH_COUNT];
  uint32_t tick;
  uint16_t next_event, jumps, death_obj;
  uint8_t fade_effect, coins, attempt_camera, touch_done[8];
  bool trail, dead, complete, ending, hold_prev, pressed_prev, old_on_ground, orb_touching;
  bool menu_camera;   /* fixed camera, ground scrolling on its own (main menu) */
  uint8_t used[MAX_OBJECTS / 8];
  FxEvent fx[24];
  uint8_t fx_count;
} Game;

/* Practice checkpoint: everything needed to resume from a point. */
typedef struct {
  Player p;
  float cam_x, cam_y, ground_x, bg_x, ground_gfx, cam_intended_y;
  Channel ch[CH_COUNT];
  uint32_t tick;
  uint16_t next_event, jumps;
  uint8_t fade_effect, touch_done[8];
  bool trail;
} Checkpoint;

void game_start(Game *g, const Level *L, bool first_attempt);
void game_step(Game *g, bool hold);
float game_progress(const Game *g);
bool game_used(const Game *g, unsigned i);
void game_save_checkpoint(const Game *g, Checkpoint *c);
void game_load_checkpoint(Game *g, const Checkpoint *c);
unsigned game_coin_index(const Level *L, unsigned obj);
/* World -> screen helpers (floating point pixels). */
static inline float wx_to_sx(const Game *g, float x) { return (x - g->cam_x) * (2.0f / 3.0f); }
static inline float wy_to_sy(const Game *g, float y) { return 240.0f - (GROUND_OFFSET + y - g->cam_y) * (2.0f / 3.0f); }
#endif
