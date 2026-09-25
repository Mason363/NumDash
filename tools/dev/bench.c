/* Development helper: runs the app headless for instruction counting under
 * qemu-arm (no libc start-up; exits with a raw Linux syscall).
 *   bench <scenario> <frames>   scenario: 0 menu, 1 gameplay, 2 complete */
#include "app.h"
#include <string.h>

static uint8_t store[4096];
uint8_t *platform_storage(size_t *size) { *size = sizeof(store); return store; }
void platform_storage_commit(void) {}
bool platform_init(void) { return true; }
void platform_close(void) {}
uint32_t platform_keys(void) { return 0; }
uint32_t platform_millis(void) { return 0; }
void platform_sleep(unsigned ms) { (void)ms; }
uint32_t platform_random(void) { return 12345; }
void platform_frame_begin(bool vsync) { (void)vsync; }
void platform_strip(int y, int h, const uint16_t *px) { (void)y; (void)h; (void)px; }
void platform_frame_end(void) {}

static unsigned ticks;
static void run(uint32_t keys, int n) {
  for (int i = 0; i < n; i++) {
    app_tick(keys);
    if (++ticks % 6 == 0) app_frame(6.0f / ND_HZ);
  }
}
static void frame(void) {
  run(app.keys, 6);
  for (int s = 0; s < STRIPS; s++) { gfx_begin_strip(s); app_draw(); platform_strip(s * STRIP_H, STRIP_H, gfx_strip); }
}
static int atoi_(const char *s) { int v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return v; }

/* inputs of tests/replays/level1.txt up to the ship section, compiled in */
extern const int bench_ticks[], bench_vals[], bench_n;

static int bench(int scenario, int frames) {
  app_init();
  run(0, 240);
  if (scenario >= 1) {
    app.level = 0; app.practice = false; app.testing = false;
    app_go(SCR_PLAY);
    run(0, 120);
    int pos = 0, stop = scenario == 1 ? 7000 : 30000;
    bool held = false;
    for (int i = 1; i < stop && !app.g.complete; i++) {
      while (pos < bench_n && bench_ticks[pos] <= i) held = bench_vals[pos++] != 0;
      run(held ? K_OK : 0, 1);
      if (app.g.dead) return 3;
    }
    if (scenario == 2) run(0, 240 * 2 + 60);
  }
  for (int f = 0; f < frames; f++) frame();
  return 0;
}

__attribute__((used)) static void cstart(uint32_t *sp) {
  char **argv = (char **)(sp + 1);
  int code = bench(atoi_(argv[1]), atoi_(argv[2]));
  register int r0 __asm__("r0") = code;
  register int r7 __asm__("r7") = 1;
  __asm__ volatile("svc 0" ::"r"(r0), "r"(r7));
  for (;;) {}
}
__attribute__((naked)) void _start(void) { __asm__ volatile("mov r0, sp\n bl cstart\n"); }
