#include "present.h"
#include <string.h>

size_t present_plan(PresentCache *cache,const uint8_t *pixels,const uint16_t *colors,
                    PresentRegion emit,void *context) {
  uint16_t changed=0;
  for(unsigned i=0;i<16;i++)if(cache->colors[i]!=colors[i])changed|=(uint16_t)(1u<<i);
  size_t regions=0;
  for(int row=0;row<ND_TILE_ROWS;row++) {
    bool dirty[ND_TILE_COLS];
    for(int col=0;col<ND_TILE_COLS;col++) {
      uint32_t hash=2166136261u;uint16_t used=0;
      const uint8_t *p=pixels+row*ND_TILE_H*(ND_SCREEN_W/2)+col*(ND_TILE_W/2);
      for(int y=0;y<ND_TILE_H;y++,p+=ND_SCREEN_W/2)
        for(int x=0;x<ND_TILE_W/2;x++){
          uint8_t b=p[x];hash=(hash^b)*16777619u;
          if(changed)used|=(uint16_t)((1u<<(b&15))|(1u<<(b>>4)));
        }
      unsigned index=(unsigned)(row*ND_TILE_COLS+col);
      dirty[col]=!cache->initialized||hash!=cache->hashes[index]||(used&changed);
      cache->hashes[index]=hash;
    }
    for(int col=0;col<ND_TILE_COLS;) {
      if(!dirty[col]){col++;continue;}
      int start=col++;
      while(col<ND_TILE_COLS&&dirty[col])col++;
      emit(start*ND_TILE_W,row*ND_TILE_H,(col-start)*ND_TILE_W,ND_TILE_H,
           pixels,colors,context);
      regions++;
    }
  }
  memcpy(cache->colors,colors,sizeof(cache->colors));
  cache->initialized=true;
  return regions;
}

void present_expand_region(const uint8_t *pixels,const uint16_t *colors,
                           int x,int y,int w,int h,uint16_t *out) {
  for(int row=0;row<h;row++) {
    const uint8_t *p=pixels+(y+row)*(ND_SCREEN_W/2)+x/2;
    uint16_t *dst=out+row*w;
    for(int col=0;col<w/2;col++) {
      uint8_t b=p[col];dst[col*2]=colors[b&15];dst[col*2+1]=colors[b>>4];
    }
  }
}
