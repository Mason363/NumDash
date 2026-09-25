/* Desktop-only reachability search. Produces real input replays through the
 * unmodified physics (no collision bypass) to prove each shipped course can
 * be completed. Usage: solve <level index> [output]. */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 3000
#define LAYERS 9000
#define STEP 4
#define HASH 65536
typedef struct { uint16_t parent; uint8_t down; } Edge;
static Game *cur, *next;
static Edge *parents;
static uint64_t keys[HASH];
static uint8_t occupied[HASH];
static uint64_t key(const Game *g) {
  const Player *p = &g->p;
  uint64_t y = (uint64_t)(int64_t)(p->y * 4), v = (uint64_t)(int64_t)(p->vy * 2);
  uint64_t k = (y & 0xffff) | ((v & 0xffff) << 16) | ((uint64_t)p->mode << 32) | ((uint64_t)p->upside << 33) |
               ((uint64_t)p->on_ground << 34) | ((uint64_t)g->hold_prev << 35) | ((uint64_t)p->buffer << 36);
  unsigned first = level_lower_bound(g->L, (int)p->x - 120) / 8;
  for (unsigned i = first; i < first + 6 && i < MAX_OBJECTS / 8; i++) k = (k ^ g->used[i]) * 1099511628211ull;
  return k;
}
static int insert(uint64_t k) {
  unsigned i = (unsigned)((k ^ (k >> 29)) * 2654435761u) & (HASH - 1);
  while (occupied[i]) { if (keys[i] == k) return 0; i = (i + 1) & (HASH - 1); }
  occupied[i] = 1; keys[i] = k; return 1;
}
int main(int argc, char **argv) {
  int index = argc > 1 ? atoi(argv[1]) : 0;
  static Level L;
  if (index < 0 || index >= LEVEL_COUNT || !level_load_builtin(&L, (unsigned)index)) return 2;
  cur = calloc(WIDTH, sizeof(Game)); next = calloc(WIDTH * 2, sizeof(Game));
  parents = calloc((size_t)LAYERS * WIDTH, sizeof(Edge));
  Edge *pending = calloc(WIDTH * 2, sizeof(Edge));
  if (!cur || !next || !parents || !pending) return 2;
  game_start(&cur[0], &L, false);
  int count = 1, finish = -1, layer = 0;
  for (; layer < LAYERS; layer++) {
    memset(occupied, 0, sizeof(occupied));
    int n = 0;
    for (int i = 0; i < count && finish < 0; i++)
      for (int down = 0; down < 2; down++) {
        Game *g = &next[n];
        *g = cur[i];
        for (int t = 0; t < STEP; t++) { g->fx_count = 0; game_step(g, down != 0); }
        if (g->dead || !insert(key(g))) continue;
        pending[n] = (Edge){(uint16_t)i, (uint8_t)down};
        if (g->complete) { finish = n; n++; break; }
        n++;
      }
    if (!n) {
      fprintf(stderr, "NO PATH: %s at x=%.1f layer=%d\n", L.name, cur[0].p.x, layer);
      return 1;
    }
    if (finish >= 0) { parents[(size_t)layer * WIDTH] = pending[finish]; finish = 0; break; }
    int keep = n > WIDTH ? WIDTH : n;
    for (int i = 0; i < keep; i++) {
      int chosen = (int)((int64_t)i * n / keep);
      cur[i] = next[chosen];
      parents[(size_t)layer * WIDTH + i] = pending[chosen];
    }
    count = keep;
    if (layer % 500 == 0) fprintf(stderr, "%s: %.0f%%, %d states\n", L.name, game_progress(&cur[0]), count);
  }
  if (finish < 0) return 1;
  static uint8_t inputs[LAYERS];
  int node = finish;
  for (int j = layer; j >= 0; j--) { Edge e = parents[(size_t)j * WIDTH + node]; inputs[j] = e.down; node = e.parent; }
  char path[256];
  if (argc > 2) snprintf(path, sizeof(path), "%s", argv[2]);
  else snprintf(path, sizeof(path), "tests/replays/level%d.txt", index + 1);
  FILE *f = fopen(path, "w");
  if (!f) return 2;
  int prev = 0;
  for (int j = 0; j <= layer; j++)
    if (inputs[j] != prev) { fprintf(f, "input=%d,%d\n", j * STEP + 1, inputs[j]); prev = inputs[j]; }
  fclose(f);
  printf("COMPLETE: %s, %d ticks -> %s\n", L.name, (layer + 1) * STEP, path);
  return 0;
}
