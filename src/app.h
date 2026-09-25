#ifndef NUMDASH_APP_H
#define NUMDASH_APP_H
#include "game.h"
#include "platform.h"
typedef enum { HOME, SELECT, SLOTS, EDITOR, PLAY, PAUSE, COMPLETE, SETTINGS, HELP, PROPERTIES } Screen;
typedef struct {
  SaveData save;
  Player player, checkpoint;
  Level level;
  Screen screen, help_return;
  uint32_t time, previous_keys, repeat_time, screen_time, deaths, fps;
  uint16_t death_timer;
  uint8_t selection, menu, slot, brush, rotate;
  bool running, practice, has_checkpoint, testing, dirty, save_ok, loaded;
  int cursor_x,cursor_y,edit_camera,undo_count;
  CustomLevel undo;
  char notice[40];
  uint32_t notice_until;
} App;
extern App app;
void app_init(void);
void app_tick(uint32_t keys);
void app_render(void);
void app_save(void);
void app_start(unsigned level,bool practice,bool testing);
#endif
