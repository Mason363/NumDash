#include "platform.h"
#include <eadk.h>
#include <string.h>

bool platform_init(void) { return true; }
void platform_close(void) {}
uint32_t platform_millis(void) { return (uint32_t)eadk_timing_millis(); }
void platform_sleep(unsigned ms) { eadk_timing_msleep(ms); }
uint32_t platform_random(void) { return eadk_random(); }

static uint32_t scan_keys(void) {
  eadk_keyboard_state_t k = eadk_keyboard_scan();
  uint32_t out = 0;
  static const struct { uint8_t key; uint32_t bit; } map[] = {
    {eadk_key_left, K_LEFT}, {eadk_key_right, K_RIGHT}, {eadk_key_up, K_UP}, {eadk_key_down, K_DOWN},
    {eadk_key_ok, K_OK}, {eadk_key_exe, K_EXE}, {eadk_key_back, K_BACK}, {eadk_key_home, K_HOME},
    {eadk_key_on_off, K_HOME}, {eadk_key_toolbox, K_TOOL}, {eadk_key_shift, K_SHIFT},
    {eadk_key_backspace, K_ERASE}, {eadk_key_var, K_SAVE}, {eadk_key_alpha, K_UNDO},
    {eadk_key_zero, K_CHECK}, {eadk_key_plus, K_PLUS}, {eadk_key_minus, K_MINUS},
    {eadk_key_xnt, K_COPY}, {eadk_key_ln, K_PROPS}};
  for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
    if (eadk_keyboard_key_down(k, (eadk_key_t)map[i].key)) out |= map[i].bit;
  return out;
}

/* Keys are also sampled between strip transfers so that a quick tap made
 * while a frame is being sent is not lost. */
static uint32_t last_scan, captured;
static void capture_keys(void) {
  uint32_t now = scan_keys();
  captured |= now & ~last_scan;
  last_scan = now;
}
uint32_t platform_keys(void) {
  capture_keys();
  uint32_t out = last_scan | captured;
  captured = 0;
  return out;
}

/* Strips whose pixels did not change since the last frame are skipped. */
static uint32_t strip_hash[32];
void platform_frame_begin(bool vsync) {
  capture_keys();
  if (vsync) eadk_display_wait_for_vblank();
}
void platform_strip(int y, int h, const uint16_t *pixels) {
  const uint32_t *w = (const uint32_t *)pixels;
  uint32_t hash = 2166136261u;
  for (int i = 0, n = 320 * h / 2; i < n; i++) hash = (hash ^ w[i]) * 16777619u;
  int slot = (y / 8) & 31;
  if (strip_hash[slot] != hash) {
    strip_hash[slot] = hash;
    eadk_display_push_rect((eadk_rect_t){0, (uint16_t)y, 320, (uint16_t)h}, pixels);
  }
  capture_keys();
}
void platform_frame_end(void) { capture_keys(); }

/* Only Epsilon's documented record buffer is used. A SlotInfo pointer is
 * available after a USB transfer but can be zero after a cold boot, so a
 * unique validated storage from either firmware slot is accepted too. */
static uint8_t *storage_at(uint32_t address, size_t *size) {
#if PLATFORM_DEVICE
  /* The optional extra-data sector shifts the userland header by 64 KiB. */
  if (address != 0x90010000 && address != 0x90020000 && address != 0x90410000 && address != 0x90420000) return NULL;
  const uint32_t *header = (const uint32_t *)(uintptr_t)address;
  if (header[0] != 0xDEC0EDFE || header[11] != 0xDEC0EDFE) return NULL;
  uint32_t ram = header[3], length = header[4];
  if (length < 1024 || length > 65536 || ram < 0x24000010 || ram > 0x24040000 - length - 8 || ram % 4) return NULL;
  const uint32_t *magic = (const uint32_t *)(uintptr_t)ram;
  if (*magic != 0xEE0BDDBA) return NULL;
  uint32_t footer;
  memcpy(&footer, (const uint8_t *)magic + 4 + length, 4);
  if (footer != 0xEE0BDDBA) return NULL;
  *size = length;
  return (uint8_t *)(uintptr_t)(ram + 4);
#else
  (void)address; (void)size;
  return NULL;
#endif
}
uint8_t *platform_storage(size_t *size) {
#if !PLATFORM_DEVICE
  (void)size;
  return NULL;
#endif
  const volatile uint32_t *slot = (const volatile uint32_t *)0x24000000;
  if (slot[0] == 0xEFEEDBBA && slot[3] == 0xEFEEDBBA) {
    uint8_t *a = storage_at(slot[2], size);
    if (a) return a;
  }
  static const uint32_t headers[] = {0x90010000, 0x90020000, 0x90410000, 0x90420000};
  uint8_t *match = NULL;
  size_t matched = 0;
  for (unsigned i = 0; i < 4; i++) {
    size_t bytes = 0;
    uint8_t *a = storage_at(headers[i], &bytes);
    if (!a) continue;
    if (match && (match != a || matched != bytes)) return NULL;
    match = a;
    matched = bytes;
  }
  if (match) *size = matched;
  return match;
}
void platform_storage_commit(void) {}
