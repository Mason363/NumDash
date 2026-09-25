#ifndef NUMDASH_PLATFORM_H
#define NUMDASH_PLATFORM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { K_LEFT = 1, K_RIGHT = 2, K_UP = 4, K_DOWN = 8, K_OK = 16, K_EXE = 32,
       K_BACK = 64, K_HOME = 128, K_TOOL = 256, K_SHIFT = 512, K_ERASE = 1024,
       K_SAVE = 2048, K_UNDO = 4096, K_CHECK = 8192, K_PLUS = 16384, K_MINUS = 32768,
       K_COPY = 65536, K_PROPS = 131072 };

bool platform_init(void);
void platform_close(void);
/* Keys held now, plus keys pressed and released since the previous call. */
uint32_t platform_keys(void);
uint32_t platform_millis(void);
void platform_sleep(unsigned ms);
uint32_t platform_random(void);

/* A frame is sent as horizontal strips (top to bottom) between begin/end.
 * vsync asks the device to start on the LCD's vertical blank. */
void platform_frame_begin(bool vsync);
void platform_strip(int y, int h, const uint16_t *pixels);
void platform_frame_end(void);

/* Epsilon's record file system (magic words excluded), or NULL. Writes go
 * through save.c's move-safe helpers; commit persists them where needed. */
uint8_t *platform_storage(size_t *size);
void platform_storage_commit(void);
#endif
