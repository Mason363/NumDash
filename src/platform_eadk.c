#include "platform.h"
#include "game.h"
#include <eadk.h>
#include <string.h>

bool platform_init(void){return true;}
void platform_close(void){}
uint32_t platform_millis(void){return (uint32_t)eadk_timing_millis();}
void platform_sleep(unsigned ms){eadk_timing_msleep(ms);}
static uint32_t scan_keys(void) {
  eadk_keyboard_state_t k=eadk_keyboard_scan();uint32_t out=0;
  static const struct {eadk_key_t key;uint32_t bit;} map[]={
    {eadk_key_left,K_LEFT},{eadk_key_right,K_RIGHT},{eadk_key_up,K_UP},{eadk_key_down,K_DOWN},
    {eadk_key_ok,K_OK},{eadk_key_exe,K_EXE},{eadk_key_back,K_BACK},{eadk_key_home,K_HOME},{eadk_key_on_off,K_HOME},
    {eadk_key_toolbox,K_TOOL},{eadk_key_shift,K_SHIFT},{eadk_key_backspace,K_ERASE},{eadk_key_var,K_SAVE},
    {eadk_key_alpha,K_UNDO},{eadk_key_zero,K_CHECK},{eadk_key_plus,K_PLUS},{eadk_key_minus,K_MINUS},
    {eadk_key_xnt,K_COPY},{eadk_key_ln,K_PROPS}};
  for(unsigned i=0;i<sizeof(map)/sizeof(map[0]);i++)if(eadk_keyboard_key_down(k,map[i].key))out|=map[i].bit;
  return out;
}
static uint32_t last_scan,captured;
static void capture_keys(void){uint32_t now=scan_keys();captured|=now&~last_scan;last_scan=now;}
uint32_t platform_keys(void){capture_keys();uint32_t out=last_scan|captured;captured=0;return out;}
void platform_present(const uint8_t *pixels,const uint16_t *colors) {
  static uint16_t strip[320*8];
  static uint32_t old_hash[30];
  static uint16_t old_palette[16];
  bool palette_changed=memcmp(old_palette,colors,sizeof(old_palette))!=0;
  for(int y=0;y<240;y+=8){
    if((y&31)==0)capture_keys();
    uint32_t hash=2166136261u;
    const uint8_t *p=pixels+y*160;
    for(int i=0;i<160*8;i++){uint8_t b=p[i];strip[i*2]=colors[b&15];strip[i*2+1]=colors[b>>4];hash=(hash^b)*16777619u;}
    if(palette_changed||hash!=old_hash[y/8])eadk_display_push_rect((eadk_rect_t){0,(uint16_t)y,320,8},strip);
    old_hash[y/8]=hash;
  }
  memcpy(old_palette,colors,sizeof(old_palette));
}

/* Only the documented Epsilon record buffer is writable here. A SlotInfo
 * pointer is available after USB transfer but can be zero after a cold boot,
 * so accept a unique validated arena from either firmware slot as fallback. */
static uint8_t *arena_at(uint32_t address,size_t *size) {
#if PLATFORM_DEVICE
  /* The optional extra-data sector shifts the userland header by 64 KiB. */
  if(address!=0x90010000&&address!=0x90020000&&
     address!=0x90410000&&address!=0x90420000)return NULL;
  const uint32_t *header=(const uint32_t *)(uintptr_t)address;
  if(header[0]!=0xDEC0EDFE||header[11]!=0xDEC0EDFE)return NULL;
  uint32_t ram=header[3],length=header[4];
  if(length<1024||length>65536||ram<0x24000010||ram>0x24040000-length-8||ram%4)return NULL;
  const uint32_t *magic=(const uint32_t *)(uintptr_t)ram;
  if(*magic!=0xEE0BDDBA)return NULL;
  uint32_t footer;memcpy(&footer,(const uint8_t *)magic+4+length,4);
  if(footer!=0xEE0BDDBA)return NULL;
  *size=length;return (uint8_t *)(uintptr_t)(ram+4);
#else
  (void)address;(void)size;return NULL;
#endif
}
static uint8_t *arena(size_t *size) {
#if PLATFORM_DEVICE
  const volatile uint32_t *slot=(const volatile uint32_t *)0x24000000;
  if(slot[0]==0xEFEEDBBA&&slot[3]==0xEFEEDBBA){
    uint8_t *a=arena_at(slot[2],size);if(a)return a;
  }
  static const uint32_t headers[]={0x90010000,0x90020000,0x90410000,0x90420000};
  uint8_t *match=NULL;size_t matched_size=0;
  for(unsigned i=0;i<4;i++){
    size_t bytes=0;uint8_t *a=arena_at(headers[i],&bytes);
    if(!a)continue;
    if(match&&(match!=a||matched_size!=bytes))return NULL;
    match=a;matched_size=bytes;
  }
  if(match)*size=matched_size;
  return match;
#else
  (void)size;return NULL;
#endif
}
bool platform_load(uint8_t *data,size_t cap,size_t *size) {
  size_t bytes=0,len=0;uint8_t *a=arena(&bytes);const uint8_t *p;
  if(!a||!storage_read(a,bytes,"numdash.ndd",&p,&len)||len>cap)return false;
  memcpy(data,p,len);*size=len;return true;
}
bool platform_save(const uint8_t *data,size_t size) {
  size_t bytes=0;uint8_t *a=arena(&bytes);
  return a&&storage_write(a,bytes,"numdash.ndd",data,size);
}
