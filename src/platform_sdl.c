/* Desktop build: SDL2 window, keyboard mapping and a file-backed copy of
 * the calculator's record storage. */
#include "platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint16_t frame[320 * 240];
static bool quit;
static uint8_t storage[42 * 1024];

static const char *store_path(void) {
  const char *p = getenv("NUMDASH_STORE");
  return p ? p : "numdash.store";
}

bool platform_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) return false;
  window = SDL_CreateWindow("NumDash  (Space/Enter/Up: jump  Esc: back  0: checkpoint)", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 960, 720, SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) return false;
  SDL_RenderSetLogicalSize(renderer, 320, 240);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 320, 240);
  FILE *f = fopen(store_path(), "rb");
  if (f) {
    size_t n = fread(storage, 1, sizeof(storage), f);
    (void)n;
    fclose(f);
  }
  return texture != NULL;
}
void platform_close(void) {
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}
uint32_t platform_millis(void) { return SDL_GetTicks(); }
void platform_sleep(unsigned ms) { SDL_Delay(ms); }
uint32_t platform_random(void) { return (uint32_t)rand() ^ ((uint32_t)rand() << 15) ^ ((uint32_t)rand() << 30); }

static uint32_t captured, held;
static const struct { int key; uint32_t bit; } keymap[] = {
  {SDL_SCANCODE_LEFT, K_LEFT}, {SDL_SCANCODE_RIGHT, K_RIGHT}, {SDL_SCANCODE_UP, K_UP}, {SDL_SCANCODE_DOWN, K_DOWN},
  {SDL_SCANCODE_SPACE, K_OK}, {SDL_SCANCODE_RETURN, K_EXE}, {SDL_SCANCODE_ESCAPE, K_BACK}, {SDL_SCANCODE_HOME, K_HOME},
  {SDL_SCANCODE_TAB, K_TOOL}, {SDL_SCANCODE_LSHIFT, K_SHIFT}, {SDL_SCANCODE_RSHIFT, K_SHIFT},
  {SDL_SCANCODE_BACKSPACE, K_ERASE}, {SDL_SCANCODE_S, K_SAVE}, {SDL_SCANCODE_Z, K_UNDO}, {SDL_SCANCODE_0, K_CHECK},
  {SDL_SCANCODE_EQUALS, K_PLUS}, {SDL_SCANCODE_MINUS, K_MINUS}, {SDL_SCANCODE_X, K_COPY}, {SDL_SCANCODE_L, K_PROPS}};
uint32_t platform_keys(void) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) quit = true;
    if (e.type == SDL_KEYDOWN && !e.key.repeat)
      for (unsigned i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++)
        if ((int)e.key.keysym.scancode == keymap[i].key) captured |= keymap[i].bit;
  }
  const uint8_t *k = SDL_GetKeyboardState(NULL);
  held = quit ? K_HOME : 0;
  for (unsigned i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++)
    if (k[keymap[i].key]) held |= keymap[i].bit;
  uint32_t out = held | captured;
  captured = 0;
  return out;
}

void platform_frame_begin(bool vsync) { (void)vsync; }
void platform_strip(int y, int h, const uint16_t *pixels) { memcpy(frame + y * 320, pixels, (size_t)h * 320 * 2); }
void platform_frame_end(void) {
  SDL_UpdateTexture(texture, NULL, frame, 320 * 2);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

uint8_t *platform_storage(size_t *size) {
  *size = sizeof(storage);
  return storage;
}
void platform_storage_commit(void) {
  char tmp[1024];
  if (snprintf(tmp, sizeof(tmp), "%s.tmp", store_path()) >= (int)sizeof(tmp)) return;
  FILE *f = fopen(tmp, "wb");
  if (!f) return;
  bool good = fwrite(storage, 1, sizeof(storage), f) == sizeof(storage);
  if (fclose(f) == 0 && good) rename(tmp, store_path());
}
