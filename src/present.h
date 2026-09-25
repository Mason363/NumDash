#ifndef NUMDASH_PRESENT_H
#define NUMDASH_PRESENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { ND_SCREEN_W=320, ND_SCREEN_H=240, ND_TILE_W=32, ND_TILE_H=8,
       ND_TILE_COLS=ND_SCREEN_W/ND_TILE_W, ND_TILE_ROWS=ND_SCREEN_H/ND_TILE_H };
typedef struct {
  uint32_t hashes[ND_TILE_COLS*ND_TILE_ROWS];
  uint16_t colors[16];
  bool initialized;
} PresentCache;
typedef void (*PresentRegion)(int x,int y,int w,int h,const uint8_t *pixels,
                              const uint16_t *colors,void *context);
/* Emits horizontal runs of changed tiles, including tiles using changed colors. */
size_t present_plan(PresentCache *cache,const uint8_t *pixels,const uint16_t *colors,
                    PresentRegion emit,void *context);
/* Expand a rectangular run into the packed RGB565 layout expected by EADK. */
void present_expand_region(const uint8_t *pixels,const uint16_t *colors,
                           int x,int y,int w,int h,uint16_t *out);
#endif
