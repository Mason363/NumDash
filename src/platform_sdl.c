#include "platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static bool quit;
bool platform_init(void) {
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER))return false;
  window=SDL_CreateWindow("NumDash - Enter: EXE | Space: OK | Tab: Toolbox | S: Save | Z: Undo",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,720,SDL_WINDOW_RESIZABLE);
  renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
  if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
  if(!renderer)return false;
  SDL_RenderSetLogicalSize(renderer,320,240);SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
  texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,320,240);
  return texture!=NULL;
}
void platform_close(void){SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();}
uint32_t platform_millis(void){return SDL_GetTicks();}
void platform_sleep(unsigned ms){SDL_Delay(ms);}
uint32_t platform_keys(void) {
  SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)quit=true;
  const uint8_t *k=SDL_GetKeyboardState(NULL);uint32_t out=quit?K_HOME:0;
  static const struct {int key;uint32_t bit;} map[]={
    {SDL_SCANCODE_LEFT,K_LEFT},{SDL_SCANCODE_RIGHT,K_RIGHT},{SDL_SCANCODE_UP,K_UP},{SDL_SCANCODE_DOWN,K_DOWN},
    {SDL_SCANCODE_SPACE,K_OK},{SDL_SCANCODE_RETURN,K_EXE},{SDL_SCANCODE_ESCAPE,K_BACK},{SDL_SCANCODE_HOME,K_HOME},
    {SDL_SCANCODE_TAB,K_TOOL},{SDL_SCANCODE_LSHIFT,K_SHIFT},{SDL_SCANCODE_RSHIFT,K_SHIFT},{SDL_SCANCODE_BACKSPACE,K_ERASE},
    {SDL_SCANCODE_S,K_SAVE},{SDL_SCANCODE_Z,K_UNDO},{SDL_SCANCODE_0,K_CHECK},{SDL_SCANCODE_EQUALS,K_PLUS},{SDL_SCANCODE_MINUS,K_MINUS},
    {SDL_SCANCODE_X,K_COPY},{SDL_SCANCODE_L,K_PROPS}};
  for(unsigned i=0;i<sizeof(map)/sizeof(map[0]);i++)if(k[map[i].key])out|=map[i].bit;
  return out;
}
void platform_present(const uint8_t *p,const uint16_t *colors) {
  uint16_t *pixels;int pitch;
  if(SDL_LockTexture(texture,NULL,(void **)&pixels,&pitch))return;
  for(int y=0;y<240;y++){uint16_t *row=(uint16_t *)((uint8_t *)pixels+y*pitch);for(int x=0;x<320;x++)row[x]=colors[(x&1)?p[y*160+x/2]>>4:p[y*160+x/2]&15];}
  SDL_UnlockTexture(texture);SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);
}
static const char *save_path(void) {const char *p=getenv("NUMDASH_SAVE");return p?p:"numdash.ndsave";}
bool platform_load(uint8_t *data,size_t cap,size_t *size){FILE *f=fopen(save_path(),"rb");if(!f)return false;size_t n=fread(data,1,cap,f);bool good=!ferror(f)&&fgetc(f)==EOF;fclose(f);if(good)*size=n;return good;}
bool platform_save(const uint8_t *data,size_t size){
  char tmp[1024];if(snprintf(tmp,sizeof(tmp),"%s.tmp",save_path())>=(int)sizeof(tmp))return false;
  FILE *f=fopen(tmp,"wb");if(!f)return false;bool good=fwrite(data,1,size,f)==size;int closed=fclose(f);return good&&!closed&&!rename(tmp,save_path());
}
