#include "game.h"
#include <string.h>

uint32_t nd_crc32(const uint8_t *p,size_t n) {
  uint32_t c=~0u;
  while(n--) { c^=*p++; for(int j=0;j<8;j++) c=(c>>1)^(0xedb88320u & (0u-(c&1))); }
  return ~c;
}
static unsigned rd16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p)|(uint32_t)rd16(p+2)<<16; }
static void wr16(uint8_t *p,unsigned v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void wr32(uint8_t *p,uint32_t v) { wr16(p,v); wr16(p+2,v>>16); }
void save_defaults(SaveData *s) {
  memset(s,0,sizeof(*s)); s->effects=1;s->percent=1;
  for(int i=0;i<ND_SLOTS;i++) { s->custom[i].length=1800; s->custom[i].bpm=128; s->custom[i].theme=(uint8_t)i; }
}
size_t save_encode(const SaveData *s,uint8_t *out,size_t cap) {
  size_t need=16+10*7+4+ND_SLOTS*6;
  for(int i=0;i<ND_SLOTS;i++) { if(s->custom[i].count>ND_CUSTOM_MAX) return 0; need+=s->custom[i].count*8; }
  if(cap<need) return 0;
  memcpy(out,"NDASH003",8); wr32(out+8,(uint32_t)need);
  size_t n=16;
  for(int i=0;i<10;i++) { out[n++]=s->best[i]; out[n++]=s->practice[i]; out[n++]=s->coins[i]; wr32(out+n,s->attempts[i]); n+=4; }
  out[n++]=s->effects; out[n++]=s->percent; out[n++]=s->fps; out[n++]=0;
  for(int i=0;i<ND_SLOTS;i++) {
    const CustomLevel *c=&s->custom[i]; wr16(out+n,c->count); wr16(out+n+2,c->length); out[n+4]=c->theme; out[n+5]=c->bpm; n+=6;
    for(unsigned j=0;j<c->count;j++) {
      const Object *o=&c->objects[j]; wr16(out+n,(uint16_t)o->x); wr16(out+n+2,(uint16_t)o->y); wr16(out+n+4,o->id);
      out[n+6]=o->rot; out[n+7]=o->flags; n+=8;
    }
  }
  wr32(out+12,nd_crc32(out+16,n-16)); return n;
}
bool save_decode(SaveData *s,const uint8_t *in,size_t size) {
  if(size<108)return false;
  bool legacy=!memcmp(in,"NDASH002",8);
  if(!legacy&&memcmp(in,"NDASH003",8))return false;
  if(rd32(in+8)!=size || nd_crc32(in+16,size-16)!=rd32(in+12)) return false;
  unsigned stride=legacy?12:8;
  /* Validate before touching the live save; avoids a second 14 KB copy. */
  for(size_t n=16;n<86;n+=7) if(in[n]>100||in[n+1]>100||in[n+2]>3) return false;
  if(in[86]>1||in[87]>(legacy?5:1)||in[88]>1) return false;
  size_t n=90;
  for(int i=0;i<ND_SLOTS;i++) {
    if(n+6>size) return false;
    unsigned count=rd16(in+n),length=rd16(in+n+2);
    if(count>ND_CUSTOM_MAX||length<300||length>32000||in[n+4]>5||in[n+5]<60||in[n+5]>200) return false;
    n+=6; int last=-1;
    for(unsigned j=0;j<count;j++,n+=stride) {
      if(n+stride>size) return false;
      int x=(int16_t)rd16(in+n),y=(int16_t)rd16(in+n+2);
      if(x<last||x<0||x>(int)length||y<0||y>1200||in[n+6]>3) return false;
      last=x;
    }
  }
  if(n!=size) return false;
  save_defaults(s); n=16;
  for(int i=0;i<10;i++) { s->best[i]=in[n++]; s->practice[i]=in[n++]; s->coins[i]=in[n++]; s->attempts[i]=rd32(in+n); n+=4; }
  s->effects=in[n++]; s->percent=legacy?1:in[n];n++; s->fps=in[n++]; n++;
  for(int i=0;i<ND_SLOTS;i++) {
    CustomLevel *c=&s->custom[i]; c->count=(uint16_t)rd16(in+n); c->length=(uint16_t)rd16(in+n+2); c->theme=in[n+4]; c->bpm=in[n+5]; n+=6;
    for(unsigned j=0;j<c->count;j++,n+=stride) c->objects[j]=(Object){(int16_t)rd16(in+n),(int16_t)rd16(in+n+2),(uint16_t)rd16(in+n+4),in[n+6],in[n+7],legacy?(uint16_t)rd16(in+n+8):0,legacy?(uint16_t)rd16(in+n+10):0};
  }
  return true;
}

/* The arena is Epsilon's m_buffer, excluding both magic words. No unaligned
 * halfword accesses; reject malformed records, duplicates, and missing EOF. */
static bool scan(const uint8_t *a,size_t size,const char *name,size_t *found,size_t *end) {
  if(!a||!name||size<2) return false;
  *found=SIZE_MAX;
  size_t p=0;
  while(p+2<=size) {
    size_t len=rd16(a+p);
    if(!len) { *end=p; return true; }
    if(len<4||len>size-p-2) return false;
    const uint8_t *zero=memchr(a+p+2,0,len-2);
    if(!zero) return false;
    if(!strcmp((const char *)a+p+2,name)) { if(*found!=SIZE_MAX) return false; *found=p; }
    p+=len;
  }
  return false;
}
bool storage_read(const uint8_t *a,size_t size,const char *name,const uint8_t **data,size_t *len) {
  size_t found,end;
  if(!data||!len||!scan(a,size,name,&found,&end)||found==SIZE_MAX) return false;
  size_t offset=2+strlen(name)+1;
  *data=a+found+offset; *len=rd16(a+found)-offset; return true;
}
bool storage_write(uint8_t *a,size_t size,const char *name,const uint8_t *data,size_t len) {
  size_t found,end;
  if(!data||!name||strlen(name)>32||!scan(a,size,name,&found,&end)) return false;
  size_t old=found==SIZE_MAX?0:rd16(a+found), total=2+strlen(name)+1+len;
  if(total>UINT16_MAX||total< len||end-old+total+2>size) return false;
  if(found==SIZE_MAX) found=end;
  memmove(a+found+total,a+found+old,end-found-old+2);
  wr16(a+found,(unsigned)total); memcpy(a+found+2,name,strlen(name)+1);
  memcpy(a+found+2+strlen(name)+1,data,len);
  return true;
}
