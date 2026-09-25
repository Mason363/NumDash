#ifndef NUMDASH_SCENE_H
#define NUMDASH_SCENE_H
#include "game.h"
#include "gfx.h"

typedef struct {
  color_t p1, p2;          /* icon colours */
  uint8_t icon_cube, icon_ship;
  bool practice, show_percent, show_bar, hide_player, paused, editor, low_detail;
  unsigned attempt;
  float attempt_x, attempt_y; /* world position of the "Attempt N" label */
  uint8_t coins_saved;     /* coins collected on an earlier run: drawn faded */
  float time;              /* seconds since level start (pulses) */
  float fade;              /* 0..1 whole-screen fade to black */
  const Checkpoint *checkpoints;
  unsigned checkpoint_count;
} SceneOpts;

/* Background and ground used by gameplay and menus. */
void scene_background(color_t bg, float bg_x, float cam_y);
void scene_ground(color_t g1, color_t line, float ground_x, int top_y, bool ceiling, bool shadows);

void scene_prepare(const Game *g, const SceneOpts *o);
void scene_draw(void);
void scene_draw_player_icon(int mode, float cx, float cy, float rot, float scale, color_t p1, color_t p2, bool upside, unsigned alpha);
color_t scene_channel(const Game *g, int ch);
#endif
