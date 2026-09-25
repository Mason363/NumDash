#ifndef NUMDASH_APP_H
#define NUMDASH_APP_H
#include "fx.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "save.h"
#include "scene.h"

typedef enum { SCR_MENU, SCR_SELECT, SCR_PLAY, SCR_GARAGE, SCR_CREATOR, SCR_EDITOR, SCR_LOADING } Screen;
enum { DLG_NONE, DLG_QUIT, DLG_INFO };
#define MAX_CHECKPOINTS 40

typedef struct {
  Screen screen, next, back_to;
  float fade;                 /* 0..1 screen transition to black */
  bool fading_out;
  float t;                    /* seconds on this screen */
  uint32_t keys, prev_keys, hit, repeat_at, ticks;
  int sel, row;               /* focused widget */
  float press_t;              /* button press animation */
  int press_id;
  int dialog;
  const char *dialog_title, *dialog_text;
  char notice[48];
  float notice_t;
  unsigned fps;
  bool running, vsync;

  /* level being played */
  int level;                  /* 0..LEVEL_COUNT-1 built-in, then custom slots */
  Level L;
  Game g;
  bool practice, testing, paused, has_level;
  unsigned attempt, session_jumps;
  uint32_t session_ticks;     /* time played in this session (complete screen) */
  float death_t, complete_t, end_menu_t, newbest_t, pause_t;
  int newbest_percent;
  bool newbest, end_menu, first_attempt;
  uint8_t coins_before, coins_got;
  Checkpoint checkpoints[MAX_CHECKPOINTS];
  unsigned checkpoint_count;
  float auto_check_t;
  float attempt_x, attempt_y;
  const char *complete_msg;

  /* menus */
  int select_page;
  float select_scroll;        /* animated page position */
  float menu_bg_x, menu_ground_x;
  int menu_color;
  float menu_color_t;
  uint8_t menu_from[3], menu_to[3];
  int garage_tab;

  /* editor */
  int slot;
  int cur_x, cur_y, brush, rotate, edit_mode, edit_page;
  float edit_cam_x, edit_cam_y;
  bool dirty;
} App;
extern App app;

extern Progress progress;
extern CustomMeta custom_meta[CUSTOM_SLOTS];

void app_init(void);
void app_tick(uint32_t keys);
void app_frame(float dt);
void app_draw(void);

/* app.c helpers shared with the screens */
void app_go(Screen s);
void app_notice(const char *s);
bool app_save_progress(void);
color_t icon_color(int index);
color_t app_p1(void);
color_t app_p2(void);
float frand(void);
uint32_t app_hit(uint32_t mask);
bool app_accept(void);
void app_begin_press(int id);
void app_dialog_quit(void);
void app_dialog_info(const char *title, const char *text);

/* play.c */
bool play_start(int level, bool practice, bool testing);
void play_tick(void);
void play_frame(float dt);
void play_draw(void);
void play_flush(void);

/* menu.c */
void menu_enter(Screen s);
void menu_tick(void);
void menu_frame(float dt);
void menu_draw(void);

/* editor.c */
void editor_open(int slot);
void editor_tick(void);
void editor_frame(float dt);
void editor_draw(void);
void creator_tick(void);
void creator_draw(void);
bool editor_load_slot(int slot, Level *L);
void editor_flush(void);
unsigned editor_object_count(void);
#endif
