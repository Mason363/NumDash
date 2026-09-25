/* Host tests: level data, physics, saves and storage safety, and the app
 * driven through its real input path. Extra modes:
 *   tests --replay <level> <file>   complete a level with a recorded input
 *   tests --shots <dir>             render every screen to PPM files */
#include "app.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ platform */

static uint8_t store[42 * 1024];
static uint16_t fb[GFX_W * GFX_H];
static unsigned frame_ticks;
uint8_t *platform_storage(size_t *size) { *size = sizeof(store); return store; }
void platform_storage_commit(void) {}
bool platform_init(void) { return true; }
void platform_close(void) {}
uint32_t platform_keys(void) { return 0; }
uint32_t platform_millis(void) { return 0; }
void platform_sleep(unsigned ms) { (void)ms; }
uint32_t platform_random(void) { return 12345; }
void platform_frame_begin(bool vsync) { (void)vsync; }
void platform_strip(int y, int h, const uint16_t *px) { memcpy(fb + y * GFX_W, px, (size_t)h * GFX_W * 2); }
void platform_frame_end(void) {}

static void render(void) {
  for (int s = 0; s < STRIPS; s++) {
    gfx_begin_strip(s);
    app_draw();
    platform_strip(s * STRIP_H, STRIP_H, gfx_strip);
  }
}
static void save_ppm(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
  for (int i = 0; i < GFX_W * GFX_H; i++) {
    uint8_t c[3] = {(uint8_t)c_r(fb[i]), (uint8_t)c_g(fb[i]), (uint8_t)c_b(fb[i])};
    fwrite(c, 1, 3, f);
  }
  fclose(f);
}
/* Runs the app for n physics ticks with keys held, 40 frames per second. */
static void run(uint32_t keys, int n) {
  for (int i = 0; i < n; i++) {
    app_tick(keys);
    if (++frame_ticks % 6 == 0) app_frame(6.0f / ND_HZ);
  }
}
static void tap(uint32_t k) { run(k, 6); run(0, 6); }
static void settle(void) { run(0, 240); }
static void fresh_app(void) {
  memset(store, 0, sizeof(store));
  app_init();
  settle();
}

/* ------------------------------------------------------------ unit tests */

static void test_levels(void) {
  static Level L;
  static const uint16_t counts[LEVEL_COUNT] = {2272, 1530, 1509, 1402, 2126, 1691, 2767};
  for (unsigned i = 0; i < LEVEL_COUNT; i++) {
    assert(level_load_builtin(&L, i));
    assert(L.count == counts[i]);
    assert(level_valid(&L));
    unsigned coins = 0;
    for (unsigned k = 0; k < L.count; k++) coins += L.objs[k].type == OT_COIN;
    assert(coins <= 3);
  }
  assert(!level_load_builtin(&L, LEVEL_COUNT));
  uint8_t out[8];
  static const uint8_t bad[] = {0xff, 0xff, 0xff};
  assert(inflate_raw(out, sizeof(out), bad, sizeof(bad)) < 0);
}

static void test_physics(void) {
  static Level L;
  static Game a, b;
  memset(&L, 0, sizeof(L));
  L.objs = level_objs;
  L.end_x = L.wall_x = 60000;
  game_start(&a, &L, false);
  float peak = 0;
  for (int i = 0; i < 240; i++) {
    game_step(&a, i < 4);
    if (a.p.y > peak) peak = a.p.y;
  }
  /* A GD cube jump peaks a little over two blocks above the ground. */
  assert(peak > 15 + 60 && peak < 15 + 90);
  assert(a.p.on_ground && !a.dead && a.jumps == 1);
  game_start(&a, &L, false);
  game_start(&b, &L, false);
  for (int i = 0; i < 2000; i++) {
    bool h = (i / 37) % 3 == 0;
    game_step(&a, h);
    game_step(&b, h);
  }
  assert(!memcmp(&a, &b, sizeof(a)));
  Checkpoint c;
  game_save_checkpoint(&a, &c);
  for (int i = 0; i < 100; i++) game_step(&a, i % 20 < 5);
  game_load_checkpoint(&a, &c);
  game_load_checkpoint(&b, &c);
  for (int i = 0; i < 300; i++) { game_step(&a, i % 30 < 8); game_step(&b, i % 30 < 8); }
  assert(!memcmp(&a.p, &b.p, sizeof(a.p)));
  /* a spike on the ground kills */
  level_objs[0] = (LObj){300, 15, OT_SPIKE, 0};
  L.count = 1;
  game_start(&a, &L, false);
  for (int i = 0; i < 400 && !a.dead; i++) game_step(&a, false);
  assert(a.dead && a.death_obj == 0);
  /* landing on a block is safe */
  level_objs[0] = (LObj){300, 15, OT_BLOCK, 0};
  game_start(&a, &L, false);
  for (int i = 0; i < 400 && !a.dead; i++) game_step(&a, i >= 150 && i < 154);
  assert(!a.dead && a.p.x > 400);
}

static void test_storage(void) {
  static uint8_t arena[4096];
  memset(arena, 0, sizeof(arena));
  const uint8_t small[3] = {7, 8, 9};
  uint8_t prog[PROGRESS_BYTES];
  Progress p;
  progress_defaults(&p);
  p.lv[2].normal = 57;
  p.lv[2].attempts = 123456;
  p.jumps = 99;
  p.options = OPT_FPS | OPT_BAR;
  progress_encode(&p, prog);
  assert(storage_put(arena, sizeof(arena), "a.py", small, 3, 1));
  assert(storage_put(arena, sizeof(arena), "numdash.nds", prog, sizeof(prog), 1));
  assert(storage_put(arena, sizeof(arena), "b.py", small, 3, 1));
  size_t len;
  const uint8_t *bpy = storage_read(arena, sizeof(arena), "b.py", &len);
  assert(bpy && len == 3 && !memcmp(bpy, small, 3));
  /* rewriting with the same size leaves every other record in place */
  p.lv[2].normal = 58;
  progress_encode(&p, prog);
  assert(storage_put(arena, sizeof(arena), "numdash.nds", prog, sizeof(prog), 1));
  assert(storage_read(arena, sizeof(arena), "b.py", &len) == bpy);
  Progress q;
  const uint8_t *d = storage_read(arena, sizeof(arena), "numdash.nds", &len);
  assert(d && progress_decode(&q, d, len) && q.lv[2].normal == 58 && q.lv[2].attempts == 123456 && q.options == (OPT_FPS | OPT_BAR));
  /* a record that is not the last one never grows */
  uint32_t crc = nd_crc32(arena, sizeof(arena));
  uint8_t big[400] = {1};
  assert(!storage_put(arena, sizeof(arena), "numdash.nds", big, sizeof(big), 1));
  assert(crc == nd_crc32(arena, sizeof(arena)));
  /* but it can shrink in place (padded) */
  assert(storage_put(arena, sizeof(arena), "numdash.nds", small, 3, 1));
  assert(storage_read(arena, sizeof(arena), "b.py", &len) == bpy);
  d = storage_read(arena, sizeof(arena), "numdash.nds", &len);
  assert(d && len == PROGRESS_BYTES && d[0] == 7 && d[3] == 0);
  assert(!storage_remove_last(arena, sizeof(arena), "numdash.nds"));
  /* the last record is resized freely */
  assert(storage_put(arena, sizeof(arena), "c.ndl", big, sizeof(big), 512));
  assert(storage_put(arena, sizeof(arena), "c.ndl", big, 1000 > sizeof(big) ? sizeof(big) : 0, 512));
  static uint8_t huge[1500];
  assert(storage_put(arena, sizeof(arena), "c.ndl", huge, sizeof(huge), 512));
  assert(storage_read(arena, sizeof(arena), "b.py", &len) == bpy);
  assert(storage_read(arena, sizeof(arena), "c.ndl", &len) && len >= sizeof(huge));
  assert(storage_remove_last(arena, sizeof(arena), "c.ndl"));
  assert(!storage_read(arena, sizeof(arena), "c.ndl", &len));
  assert(storage_read(arena, sizeof(arena), "b.py", &len) == bpy);
  /* full storage */
  static uint8_t giant[4000];
  assert(!storage_put(arena, sizeof(arena), "d.ndl", giant, sizeof(giant), 1));
  /* malformed storage is rejected, never written */
  memset(arena, 0xff, 64);
  assert(!storage_put(arena, sizeof(arena), "numdash.nds", prog, sizeof(prog), 1));
  for (int j = 0; j < 3000; j++) {
    size_t size = (unsigned)(j * 7) % sizeof(arena);
    for (size_t i = 0; i < size; i++) arena[i] = (uint8_t)(i * 31 + j * 17 + (i >> 3));
    storage_read(arena, size, "numdash.nds", &len);
    storage_put(arena, size, "numdash.nds", prog, sizeof(prog), 1);
  }
  /* corrupted progress is rejected */
  progress_encode(&p, prog);
  for (int i = 0; i < PROGRESS_BYTES; i += 7) {
    prog[i] ^= 0x10;
    assert(!progress_decode(&q, prog, sizeof(prog)));
    prog[i] ^= 0x10;
  }
  assert(progress_decode(&q, prog, sizeof(prog)));
}

static void test_custom(void) {
  static LObj a[CUSTOM_MAX], b[CUSTOM_MAX];
  static uint8_t buf[16 + CUSTOM_MAX * 8];
  unsigned n = 0, x = 15;
  for (unsigned i = 0; i < 600; i++) {
    x += (i * 7) % 3 == 0 ? 30 : 0;
    a[n++] = (LObj){(uint16_t)x, (int16_t)(15 + (i % 9) * 30 - (i % 4 == 0 ? 13 : 0)), (uint8_t)(1 + i % (OT_COUNT - 1)), (uint8_t)(i & 3)};
  }
  CustomMeta m = {(uint16_t)n, 9000, 3, 1, true}, m2;
  size_t len = custom_encode(a, n, &m, buf, sizeof(buf));
  assert(len > 16 && len < n * 5 + 16);
  assert(custom_decode(buf, len, b, CUSTOM_MAX, &m2));
  assert(m2.count == n && m2.theme == 3 && m2.flags == 1 && m2.end_x == 9000);
  assert(!memcmp(a, b, n * sizeof(LObj)));
  for (size_t i = 0; i < len; i += 5) {
    buf[i] ^= 4;
    assert(!custom_decode(buf, len, b, CUSTOM_MAX, &m2));
    buf[i] ^= 4;
  }
  assert(!custom_encode(a, n, &m, buf, 64));
}

/* Builds a save of the previous NumDash release (format NDASH003). */
static size_t legacy_save(uint8_t *out) {
  static const struct { int16_t x, y; uint16_t id; uint8_t rot; } objs[3] = {{15, 15, 1, 0}, {45, 15, 8, 0}, {75, 2, 35, 0}};
  size_t n = 16;
  memcpy(out, "NDASH003", 8);
  for (int i = 0; i < 10; i++) {
    out[n++] = (uint8_t)(i == 1 ? 64 : 0);
    out[n++] = (uint8_t)(i == 1 ? 100 : 0);
    out[n++] = (uint8_t)(i == 1 ? 2 : 0);
    uint32_t att = i == 1 ? 321 : 0;
    memcpy(out + n, &att, 4);
    n += 4;
  }
  out[n++] = 1; out[n++] = 1; out[n++] = 0; out[n++] = 0;
  for (int s = 0; s < 3; s++) {
    unsigned c = s == 0 ? 3 : 0;
    out[n] = (uint8_t)c; out[n + 1] = 0; out[n + 2] = 0x08; out[n + 3] = 0x07; out[n + 4] = 1; out[n + 5] = 128;
    n += 6;
    for (unsigned j = 0; j < c; j++) {
      memcpy(out + n, &objs[j].x, 2); memcpy(out + n + 2, &objs[j].y, 2); memcpy(out + n + 4, &objs[j].id, 2);
      out[n + 6] = objs[j].rot; out[n + 7] = 0;
      n += 8;
    }
  }
  uint32_t size = (uint32_t)n, crc = nd_crc32(out + 16, n - 16);
  memcpy(out + 8, &size, 4);
  memcpy(out + 12, &crc, 4);
  return n;
}

static void test_legacy(void) {
  static uint8_t old[512];
  size_t n = legacy_save(old);
  memset(store, 0, sizeof(store));
  assert(storage_put(store, sizeof(store), "numdash.ndd", old, n, 1));
  Progress p;
  CustomMeta metas[CUSTOM_SLOTS];
  assert(save_load_all(&p, metas));
  assert(p.lv[1].normal == 64 && p.lv[1].practice == 100 && p.lv[1].coins == 3 && p.lv[1].attempts == 321);
  assert(metas[0].exists && metas[0].count == 3 && !metas[1].exists);
  size_t len;
  assert(storage_read(store, sizeof(store), "numdash.nds", &len));
  assert(!storage_read(store, sizeof(store), "numdash.ndd", &len));   /* it was last: removed */
  static LObj objs[8];
  CustomMeta m;
  assert(save_read_custom(0, objs, 8, &m) && m.count == 3 && objs[1].type == OT_SPIKE && objs[2].type == OT_PAD_Y);
  /* the second load uses the new records */
  assert(save_load_all(&p, metas) && p.lv[1].attempts == 321);
}

/* ------------------------------------------------------------ app flow */

static void test_app(void) {
  fresh_app();
  assert(app.screen == SCR_MENU && app.sel == 1);
  tap(K_OK);
  settle();
  assert(app.screen == SCR_SELECT);
  tap(K_RIGHT);
  tap(K_LEFT);
  tap(K_EXE);
  settle();
  assert(app.screen == SCR_PLAY && app.level == 0 && app.attempt == 1);
  run(0, 240 * 8);   /* no input: the first spike kills */
  assert(app.attempt >= 2 && progress.lv[0].attempts >= 2 && progress.lv[0].normal > 0);
  tap(K_BACK);
  assert(app.paused);
  run(0, 100);
  tap(K_BACK);
  assert(!app.paused);
  /* practice: checkpoint then restart from it */
  tap(K_BACK);
  app.sel = 0;
  tap(K_OK);
  assert(app.practice && !app.paused);
  run(0, 200);
  tap(K_CHECK);
  assert(app.checkpoint_count == 1);
  tap(K_ERASE);
  assert(app.checkpoint_count == 0);
  tap(K_BACK);
  app.sel = 2;   /* menu */
  tap(K_OK);
  settle();
  assert(app.screen == SCR_SELECT);
  size_t len;
  const uint8_t *d = storage_read(store, sizeof(store), "numdash.nds", &len);
  Progress p;
  assert(d && progress_decode(&p, d, len) && p.lv[0].attempts == progress.lv[0].attempts);
  tap(K_BACK);
  settle();
  assert(app.screen == SCR_MENU);
  /* quit dialog */
  tap(K_BACK);
  assert(app.dialog == DLG_QUIT);
  tap(K_BACK);
  assert(!app.dialog && app.running);
  /* settings popup toggles and saves */
  app.sel = 4;
  tap(K_OK);
  run(0, 60);
  uint8_t before = progress.options;
  tap(K_OK);
  assert(progress.options == (before ^ OPT_PERCENT));
  tap(K_BACK);
  d = storage_read(store, sizeof(store), "numdash.nds", &len);
  assert(d && progress_decode(&p, d, len) && p.options == progress.options);
}

static void test_editor(void) {
  fresh_app();
  app.sel = 2;
  tap(K_OK);
  settle();
  assert(app.screen == SCR_CREATOR);
  tap(K_OK);
  settle();
  assert(app.screen == SCR_EDITOR);
  for (int i = 0; i < 5; i++) { tap(K_RIGHT); tap(K_OK); }
  tap(K_PLUS);
  for (int i = 0; i < 6; i++) tap(K_PLUS);
  tap(K_RIGHT); tap(K_RIGHT); tap(K_OK);
  assert(editor_object_count() == 6);
  tap(K_UNDO);
  assert(editor_object_count() == 5);
  tap(K_ERASE);   /* nothing under the cursor now */
  tap(K_LEFT); tap(K_LEFT);
  tap(K_ERASE);
  assert(editor_object_count() == 4);
  tap(K_SAVE);
  CustomMeta m;
  static LObj objs[16];
  assert(save_read_custom(0, objs, 16, &m) && m.count == 4);
  tap(K_EXE);
  settle();
  assert(app.screen == SCR_PLAY && app.testing);
  run(K_OK, 240);
  tap(K_BACK);
  app.sel = 2;
  tap(K_OK);
  settle();
  assert(app.screen == SCR_EDITOR && editor_object_count() == 4);
  tap(K_BACK);
  settle();
  assert(app.screen == SCR_CREATOR && custom_meta[0].count == 4);
}

/* ------------------------------------------------------------ replay */

static int replay(int index, const char *path) {
  static Level L;
  static Game g;
  if (!level_load_builtin(&L, (unsigned)index)) return 2;
  FILE *f = fopen(path, "r");
  if (!f) return 2;
  static int ticks[8192], vals[8192];
  int n = 0, t, v;
  while (n < 8192 && fscanf(f, "input=%d,%d\n", &t, &v) == 2) { ticks[n] = t; vals[n++] = v; }
  fclose(f);
  game_start(&g, &L, false);
  bool held = false;
  for (int i = 1, pos = 0; i < 200 * ND_HZ && !g.dead && !g.complete; i++) {
    while (pos < n && ticks[pos] <= i) held = vals[pos++] != 0;
    game_step(&g, held);
    g.fx_count = 0;
  }
  printf("%s: %s at %.0f%% (x=%.0f)\n", L.name, g.complete ? "COMPLETE" : "FAILED", game_progress(&g), g.p.x);
  return g.complete ? 0 : 1;
}

/* ------------------------------------------------------------ screenshots */

static int shots(const char *dir) {
  fresh_app();
  progress.lv[0].normal = 64;
  progress.lv[0].practice = 100;
  progress.lv[0].coins = 5;
  progress.lv[1].normal = 12;
  run(0, 240 * 3);
  render(); save_ppm(dir, "01_menu");
  app.sel = 4; tap(K_OK); run(0, 120);
  render(); save_ppm(dir, "02_settings");
  tap(K_BACK); app.sel = 5; tap(K_OK); run(0, 120);
  render(); save_ppm(dir, "03_stats");
  tap(K_BACK); app.sel = 3; tap(K_OK); run(0, 120);
  render(); save_ppm(dir, "04_help");
  tap(K_BACK); app.sel = 1; tap(K_OK); settle();
  render(); save_ppm(dir, "05_select");
  tap(K_RIGHT); run(0, 30);
  render(); save_ppm(dir, "06_select_scroll");
  settle();
  tap(K_LEFT); settle();
  tap(K_OK); settle();
  render(); save_ppm(dir, "07_play_start");
  run(0, 240 * 2 + 150);
  render(); save_ppm(dir, "08_play_death");
  run(0, 240);
  render(); save_ppm(dir, "09_play_attempt2");
  tap(K_BACK); run(0, 60);
  render(); save_ppm(dir, "10_pause");
  tap(K_BACK);
  /* level complete: replay level 1 */
  FILE *f = fopen("tests/replays/level1.txt", "r");
  static int ticks[8192], vals[8192];
  int n = 0, t, v;
  while (f && n < 8192 && fscanf(f, "input=%d,%d\n", &t, &v) == 2) { ticks[n] = t; vals[n++] = v; }
  if (f) fclose(f);
  app.checkpoint_count = 0;
  app.practice = false;
  app.attempt = 0;
  app.g.dead = true;
  app.death_t = 10;
  run(0, 6);
  bool held = false;
  int pos = 0;
  for (int i = 1; i < 200 * ND_HZ && !app.g.complete; i++) {
    while (pos < n && ticks[pos] <= i) held = vals[pos++] != 0;
    run(held ? K_OK : 0, 1);
    if (i == 3000) { render(); save_ppm(dir, "11_play_mid"); }
    if (i == 12000) { render(); save_ppm(dir, "12_play_ship"); }
  }
  run(0, 120);
  render(); save_ppm(dir, "13_complete_rays");
  run(0, 240 * 2 - 120 + 60);
  render(); save_ppm(dir, "14_complete_title");
  run(0, 240 * 3);
  render(); save_ppm(dir, "15_complete_window");
  tap(K_RIGHT); tap(K_OK); settle(); settle();
  tap(K_BACK); settle();
  app.sel = 0; tap(K_OK); settle();
  render(); save_ppm(dir, "16_garage");
  tap(K_BACK); settle();
  app.sel = 2; tap(K_OK); settle();
  render(); save_ppm(dir, "17_creator");
  tap(K_OK); settle();
  for (int i = 0; i < 6; i++) { tap(K_RIGHT); tap(K_OK); }
  tap(K_UP); tap(K_PLUS); tap(K_PLUS); tap(K_PLUS); tap(K_PLUS); tap(K_PLUS); tap(K_PLUS); tap(K_PLUS);
  tap(K_RIGHT); tap(K_OK); tap(K_RIGHT);
  render(); save_ppm(dir, "18_editor");
  tap(K_PROPS); run(0, 30);
  render(); save_ppm(dir, "19_editor_settings");
  puts("shots written");
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1], "--replay")) return replay(atoi(argv[2]), argv[3]);
  if (argc == 3 && !strcmp(argv[1], "--shots")) return shots(argv[2]);
  test_levels();
  test_physics();
  test_storage();
  test_custom();
  test_legacy();
  test_app();
  test_editor();
  puts("PASS: levels, physics, checkpoints, storage safety, saves, legacy migration, menus, gameplay, editor");
  return 0;
}
