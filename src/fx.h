#ifndef NUMDASH_FX_H
#define NUMDASH_FX_H
#include "game.h"
#include "gfx.h"

void fx_reset(void);
/* Consume the physics event queue and advance effects by dt seconds. */
void fx_update(Game *g, float dt, color_t p1, color_t p2, bool practice);
void fx_draw(void);
void fx_draw_player_trail(void);
/* Screen-space effects used by menus and the level-complete sequence. */
void fx_circle_screen(float x, float y, int def, color_t c);
void fx_burst_screen(float x, float y, int def, color_t c, int count);
void fx_circle_world(float x, float y, int def, color_t c);
void fx_burst_world(float x, float y, int def, color_t c, int count);
void fx_update_screen(float dt);
void fx_draw_screen(void);
enum { CE_PAD, CE_ORB, CE_PORTAL, CE_ORB_TOUCH, CE_DEATH, CE_COIN, CE_COIN_RING, CE_WALL1, CE_WALL2, CE_TITLE, CE_FIREWORK, CE_RING_BIG, CE_RESPAWN };
enum { PE_DRAG, PE_SHIP_FIRE, PE_SHIP_SMOKE, PE_LAND, PE_EXPLODE, PE_BUMP, PE_RING, PE_PORTAL, PE_COIN, PE_COIN_PICKUP, PE_COMPLETE, PE_FIREWORK };
#endif
