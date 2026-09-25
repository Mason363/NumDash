#ifndef NUMDASH_DRAW_H
#define NUMDASH_DRAW_H
#include "game.h"
enum { C_BG,C_BG2,C_BG3,C_GROUND,C_GROUND2,C_GLOW,C_BLACK,C_WHITE,C_MUTED,C_LIME,C_CYAN,C_YELLOW,C_PINK,C_RED,C_INK,C_PLAYER,
 C_GRID=C_BG2,C_BLUE=C_CYAN,C_ORANGE=C_YELLOW,C_DETAIL=C_CYAN };
extern uint8_t frame[320*240/2];
extern uint16_t palette[16];
static inline uint8_t gfx_index(const uint8_t *p,unsigned i){uint8_t b=p[i>>1];return (i&1)?b>>4:b&15;}
void gfx_palette(uint16_t bg,uint16_t ground,unsigned player);
void rect(int x,int y,int w,int h,int c);
void outline(int x,int y,int w,int h,int c);
void line(int x,int y,int xx,int yy,int c);
void triangle(int x,int y,int xx,int yy,int xxx,int yyy,int c);
void circle(int x,int y,int r,int c,bool fill);
void text(int x,int y,const char *s,int scale,int c);
void centered(int y,const char *s,int scale,int c);
void number(int x,int y,unsigned v,int scale,int c);
void player_icon(int x,int y,unsigned rotation,bool ship,int color);
void object_draw(const Object *o,int x,int y,bool used);
void backdrop(int scroll,unsigned time,bool grid);
#endif
