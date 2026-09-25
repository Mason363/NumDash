#ifndef NUMDASH_PLATFORM_H
#define NUMDASH_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
enum { K_LEFT=1, K_RIGHT=2, K_UP=4, K_DOWN=8, K_OK=16, K_EXE=32,
 K_BACK=64, K_HOME=128, K_TOOL=256, K_SHIFT=512, K_ERASE=1024,
 K_SAVE=2048, K_UNDO=4096, K_CHECK=8192, K_PLUS=16384, K_MINUS=32768,
 K_COPY=65536, K_PROPS=131072 };
uint32_t platform_keys(void);
uint32_t platform_millis(void);
void platform_sleep(unsigned ms);
void platform_present(const uint8_t *pixels,const uint16_t *palette);
bool platform_load(uint8_t *data,size_t cap,size_t *size);
bool platform_save(const uint8_t *data,size_t size);
bool platform_init(void);
void platform_close(void);
#endif
