#include "app.h"
#include "draw.h"
#include <string.h>

App app;
static uint8_t save_buffer[15000];
static const uint16_t brushes[]={1,8,9,40,35,36,13,12,11,10,67,1329};
static const char *brush_names[]={"BLOCK","SPIKE","FLOOR SPIKES","HALF BLOCK","JUMP PAD","JUMP ORB","SHIP PORTAL","CUBE PORTAL","GRAVITY UP","GRAVITY DOWN","GRAVITY PAD","COIN"};
#define BRUSHES (sizeof(brushes)/sizeof(brushes[0]))
static void screen(Screen s){app.screen=s;app.screen_time=app.time;app.menu=0;}
static void notify(const char *s){strncpy(app.notice,s,sizeof(app.notice)-1);app.notice[sizeof(app.notice)-1]=0;app.notice_until=app.time+720;}
void app_save(void) {
  size_t n=save_encode(&app.save,save_buffer,sizeof(save_buffer));
  app.save_ok=n && platform_save(save_buffer,n);
  if(app.save_ok) app.dirty=false;
}
void app_init(void) {
  memset(&app,0,sizeof(app)); save_defaults(&app.save);app.running=true;
  size_t n=0;
  if(platform_load(save_buffer,sizeof(save_buffer),&n)) app.loaded=save_decode(&app.save,save_buffer,n);
  app.save_ok=app.loaded;
  if(n && !app.loaded) notify("SAVE INVALID - DEFAULTS LOADED");
  app.level=nd_levels[0];
}
void app_start(unsigned index,bool practice,bool testing) {
  if(index>=ND_BUILTINS+ND_SLOTS) return;
  app.selection=(uint8_t)index;
  app.level=index<ND_BUILTINS?nd_levels[index]:custom_level(&app.save.custom[index-ND_BUILTINS],index-ND_BUILTINS);
  if(!level_valid(&app.level)) {notify("LEVEL DATA INVALID");return;}
  app.practice=practice;app.testing=testing;app.has_checkpoint=false;app.death_timer=0;
  player_start(&app.player,&app.level);app.deaths=1;
  if(!testing){app.save.attempts[index]++;app.dirty=true;}
  screen(PLAY);
}
static void remember_progress(void) {
  if(app.testing)return;
  unsigned index=app.selection;
  int percent=player_progress(&app.player,&app.level);
  uint8_t *best=app.practice?app.save.practice:app.save.best;
  if(percent>best[index]) {best[index]=(uint8_t)percent;app.dirty=true;}
  if(app.player.complete&&!app.practice&&app.player.coins>app.save.coins[index]){app.save.coins[index]=app.player.coins;app.dirty=true;}
}
static void restart(void) {
  if(app.practice&&app.has_checkpoint) {app.player=app.checkpoint;app.player.held=false;app.player.orb_armed=false;}
  else player_start(&app.player,&app.level);
  app.death_timer=0;app.deaths++;
  if(!app.testing){app.save.attempts[app.selection]++;app.dirty=true;}
}
static void leave_play(void) {
  remember_progress();
  if(app.testing)screen(EDITOR);
  else {app_save();screen(app.selection<ND_BUILTINS?SELECT:SLOTS);}
}
static void undo_snapshot(void) {app.undo=app.save.custom[app.slot];app.undo_count=1;}
static void edit_reset_progress(void) {unsigned i=ND_BUILTINS+app.slot;app.save.best[i]=app.save.practice[i]=app.save.coins[i]=0;app.dirty=true;}
void app_tick(uint32_t keys) {
  app.time++;
  uint32_t hit=keys&~app.previous_keys;
  uint32_t arrows=K_LEFT|K_RIGHT|K_UP|K_DOWN;
  if(keys&arrows) {if((keys&arrows)!=(app.previous_keys&arrows))app.repeat_time=app.time+72;else if(app.time>=app.repeat_time){hit|=keys&arrows;app.repeat_time=app.time+20;}}
  app.previous_keys=keys;
  bool accept=(hit&(K_OK|K_EXE))!=0;
  if(hit&K_HOME){remember_progress();app_save();app.running=false;return;}
  switch(app.screen) {
    case HOME:
      if(hit&K_LEFT)app.menu=(app.menu+3)%4;
      if(hit&K_RIGHT)app.menu=(app.menu+1)%4;
      if(hit&K_BACK){app_save();app.running=false;}
      if(accept){if(app.menu==0){app.selection=0;screen(SELECT);}else if(app.menu==1)screen(SLOTS);else if(app.menu==2)screen(SETTINGS);else {app.help_return=HOME;screen(HELP);}}
      break;
    case SELECT:
      if(hit&K_LEFT)app.selection=(app.selection+ND_BUILTINS-1)%ND_BUILTINS;
      if(hit&K_RIGHT)app.selection=(app.selection+1)%ND_BUILTINS;
      if(hit&K_BACK)screen(HOME);
      else if(accept)app_start(app.selection,false,false);
      else if(hit&K_TOOL)app_start(app.selection,true,false);
      break;
    case SLOTS:
      if(hit&K_UP)app.slot=(app.slot+ND_SLOTS-1)%ND_SLOTS;
      if(hit&K_DOWN)app.slot=(app.slot+1)%ND_SLOTS;
      if(hit&K_BACK)screen(HOME);
      else if(hit&K_EXE)app_start(ND_BUILTINS+app.slot,false,false);
      else if(hit&K_OK){app.cursor_x=15;app.cursor_y=15;app.edit_camera=0;app.undo_count=0;screen(EDITOR);}
      break;
    case EDITOR: {
      CustomLevel *c=&app.save.custom[app.slot];
      if(hit&K_BACK){app_save();if(!app.save_ok)notify("SAVE FAILED - KEPT IN SESSION");screen(SLOTS);break;}
      if(hit&K_EXE){app_start(ND_BUILTINS+app.slot,false,true);break;}
      if(hit&K_LEFT)app.cursor_x-=30;
      if(hit&K_RIGHT)app.cursor_x+=30;
      if(hit&K_UP)app.cursor_y+=30;
      if(hit&K_DOWN)app.cursor_y-=30;
      if(app.cursor_x<15)app.cursor_x=15;if(app.cursor_x>30975)app.cursor_x=30975;
      if(app.cursor_y<15)app.cursor_y=15;if(app.cursor_y>1185)app.cursor_y=1185;
      if(hit&(K_TOOL|K_PLUS))app.brush=(app.brush+1)%BRUSHES;
      if(hit&K_MINUS)app.brush=(app.brush+BRUSHES-1)%BRUSHES;
      if(hit&K_SHIFT)app.rotate=(app.rotate+1)%4;
      if(hit&K_OK){undo_snapshot();Object o={(int16_t)app.cursor_x,(int16_t)app.cursor_y,brushes[app.brush],app.rotate,0,0,0};if(o.id==35||o.id==67)o.y-=13;if(o.id==9)o.y-=13;if(editor_put(c,o)){edit_reset_progress();}else notify("LEVEL FULL - DELETE AN OBJECT");}
      if(hit&K_ERASE){undo_snapshot();bool removed=editor_remove(c,app.cursor_x,app.cursor_y);removed|=editor_remove(c,app.cursor_x,app.cursor_y-13);if(removed)edit_reset_progress();}
      if(hit&K_UNDO){if(app.undo_count){*c=app.undo;app.undo_count=0;edit_reset_progress();notify("UNDONE");}else notify("NOTHING TO UNDO");}
      if(hit&K_COPY){for(unsigned i=0;i<c->count;i++)if(c->objects[i].x==app.cursor_x && (c->objects[i].y==app.cursor_y||c->objects[i].y==app.cursor_y-13)){for(unsigned j=0;j<BRUSHES;j++)if(brushes[j]==c->objects[i].id)app.brush=(uint8_t)j;app.rotate=c->objects[i].rot;}}
      if(hit&K_SAVE){app_save();notify(app.save_ok?"LEVEL SAVED":"SAVE FAILED - STORAGE UNAVAILABLE");}
      if(hit&K_PROPS)screen(PROPERTIES);
      if(hit&K_CHECK){app.help_return=EDITOR;screen(HELP);}
      break;
    }
    case PROPERTIES: {
      CustomLevel *c=&app.save.custom[app.slot];
      if(hit&(K_UP|K_DOWN))app.menu^=1;
      if(hit&(K_LEFT|K_RIGHT)){
        int delta=(hit&K_RIGHT)?1:-1;
        if(app.menu==0)c->theme=(uint8_t)((c->theme+delta+6)%6);
        else {int bpm=c->bpm+delta*5;if(bpm>=60&&bpm<=200)c->bpm=(uint8_t)bpm;}
        app.dirty=true;
      }
      if(hit&K_BACK||accept)screen(EDITOR);
      break;
    }
    case PLAY:
      if(hit&K_BACK){remember_progress();screen(PAUSE);break;}
      if(app.player.dead){app.death_timer++;if(app.death_timer>=180 || (app.death_timer>48&&accept))restart();break;}
      if(app.practice&&hit&K_CHECK){app.checkpoint=app.player;app.has_checkpoint=true;notify("CHECKPOINT SET");}
      if(app.practice&&hit&K_ERASE){app.has_checkpoint=false;notify("CHECKPOINT REMOVED");}
      player_step(&app.player,&app.level,(keys&(K_OK|K_EXE|K_UP))!=0);
      if(app.player.dead){remember_progress();app.death_timer=0;}
      if(app.player.complete){remember_progress();if(!app.testing)app_save();screen(COMPLETE);}
      break;
    case PAUSE:
      if(hit&K_UP)app.menu=(app.menu+3)%4;
      if(hit&K_DOWN)app.menu=(app.menu+1)%4;
      if(hit&K_BACK){screen(PLAY);break;}
      if(accept){if(app.menu==0)screen(PLAY);else if(app.menu==1){app.has_checkpoint=false;restart();screen(PLAY);}else if(app.menu==2){app.practice=!app.practice;app.has_checkpoint=false;restart();screen(PLAY);}else leave_play();}
      break;
    case COMPLETE:
      if(app.time-app.screen_time<120)break;
      if(hit&K_BACK)leave_play();
      else if(accept){if(!app.testing&&app.selection<ND_BUILTINS-1)app_start(app.selection+1,false,false);else leave_play();}
      break;
    case SETTINGS:
      if(hit&K_UP)app.menu=(app.menu+2)%3;
      if(hit&K_DOWN)app.menu=(app.menu+1)%3;
      if(accept||hit&(K_LEFT|K_RIGHT)){if(app.menu==0)app.save.effects^=1;else if(app.menu==1)app.save.color=(app.save.color+1)%6;else app.save.fps^=1;app.dirty=true;}
      if(hit&K_BACK){app_save();screen(HOME);}
      break;
    case HELP:if(accept||hit&K_BACK)screen(app.help_return);break;
  }
}

static void label(int x,int y,const char *s,int scale,int c){text(x+1,y+2,s,scale,C_BLACK);text(x,y,s,scale,c);}
static void title(const char *s){centered(20,s,2,C_BLACK);centered(18,s,2,C_WHITE);}
static void panel(int x,int y,int w,int h){rect(x+3,y+4,w,h,C_BLACK);rect(x,y,w,h,C_INK);outline(x,y,w,h,C_GLOW);outline(x+2,y+2,w-4,h-4,C_BG3);}
static void footer(const char *s){rect(0,222,320,18,C_INK);centered(228,s,1,C_MUTED);}
static void button(int x,int y,int w,const char *s,bool active){rect(x+2,y+3,w,24,C_BLACK);rect(x,y,w,24,active?C_LIME:C_BG3);outline(x,y,w,24,active?C_WHITE:C_GLOW);text(x+(w-(int)strlen(s)*6)/2,y+9,s,1,active?C_BLACK:C_WHITE);}
static void bar(int x,int y,int width,int percent,int color){rect(x,y,width,12,C_BLACK);outline(x,y,width,12,C_WHITE);if(percent>0)rect(x+2,y+2,(width-4)*percent/100,8,color);}
static void world(bool editing) {
  const Level *l=&app.level; Level custom;
  float camera,cy;
  if(editing){custom=custom_level(&app.save.custom[app.slot],app.slot);l=&custom;camera=app.cursor_x-180;if(camera<0)camera=0;cy=app.cursor_y>240?app.cursor_y-240:0;}
  else {camera=app.player.x-115;if(camera<0)camera=0;cy=app.player.camera_y;}
  gfx_palette(editing?l->background:app.player.bg,editing?l->ground:app.player.ground,app.save.color);
  backdrop((int)(camera*.6f),app.time,false);
  int floor_y=208+(int)(cy*.6f);
  if(editing){for(int x=-(int)(camera*.6f)%18;x<320;x+=18)line(x,28,x,211,C_GRID);for(int y=floor_y%18;y<212;y+=18)line(0,y,319,y,C_GRID);}
  unsigned begin=level_lower_bound(l,camera-90);
  for(unsigned i=begin;i<l->count&&l->objects[i].x<camera+650;i++) {
    const Object *o=&l->objects[i];int x=(int)((o->x-camera)*.6f),y=floor_y-(int)(o->y*.6f);
    if(y>=-40&&y<260)object_draw(o,x,y,!editing&&player_used(&app.player,i));
  }
  if(!editing&&app.player.mode){int y=floor_y-(int)(app.player.ceiling*.6f);rect(0,0,320,y,C_GROUND2);rect(0,y,320,2,C_WHITE);floor_y-= (int)(app.player.floor*.6f);}
  rect(0,floor_y,320,240-floor_y,C_GROUND);rect(0,floor_y,320,2,C_WHITE);rect(0,floor_y+3,320,2,C_GLOW);
  unsigned beat_ticks=14400u/(l->bpm?l->bpm:120);
  unsigned beat_phase=(editing?app.time:app.player.tick)%beat_ticks;
  if(app.save.effects&&beat_phase<12)rect(0,floor_y-2,320,2,C_GLOW);
  for(int x=-((int)(camera*.6f)%24);x<320;x+=24){outline(x,floor_y+9,19,19,C_GROUND2);line(x,floor_y+9,x+19,floor_y+28,C_GROUND2);}
  int finish=(int)((l->length-camera)*.6f);if(finish<320){rect(finish,28,3,180,C_WHITE);for(int y=30;y<208;y+=12)rect(finish+3,y,6,6,C_LIME);}
  if(editing){
    int x=(int)((app.cursor_x-camera)*.6f),y=208+(int)(cy*.6f)-(int)(app.cursor_y*.6f);
    outline(x-11,y-11,23,23,C_YELLOW);line(x-15,y,x-11,y,C_YELLOW);line(x+11,y,x+15,y,C_YELLOW);
    Object ghost={(int16_t)app.cursor_x,(int16_t)app.cursor_y,brushes[app.brush],app.rotate,0,0,0};object_draw(&ghost,x,y,false);
    rect(0,0,320,26,C_INK);text(7,5,brush_names[app.brush],1,C_YELLOW);text(7,16,"X",1,C_MUTED);number(20,16,app.cursor_x/30,1,C_WHITE);text(66,16,"Y",1,C_MUTED);number(80,16,app.cursor_y/30,1,C_WHITE);text(125,16,"ROT",1,C_MUTED);number(150,16,app.rotate*90,1,C_WHITE);number(232,16,l->count,1,C_WHITE);text(256,16,"/384",1,C_MUTED);
    footer("OK PLACE  EXE TEST  0 HELP");return;
  }
  int px=(int)((app.player.x-camera)*.6f),py=208+(int)(cy*.6f)-(int)(app.player.y*.6f);
  if(app.player.dead){
    if(app.save.effects)for(int i=0;i<24;i++){int dx=(i*17%19)-9,dy=(i*11%23)-11;int t=app.death_timer/4;rect(px+dx*t/5,py+dy*t/5+t*t/180,3,3,i%2?C_PLAYER:C_WHITE);}
    if(app.death_timer<110){text(111,90,"CRASH!",3,C_BLACK);text(109,88,"CRASH!",3,C_WHITE);}
  }else{
    if(app.save.effects){for(int i=1;i<=4;i++){int off=i*6;rect(px-off-8,py+6,3,3,i%2?C_GLOW:C_PLAYER);}if(app.player.mode){triangle(px-13,py,px-25-(int)(app.time%7),py+4,px-13,py+6,C_ORANGE);}}
    player_icon(px,py,app.player.rotation,app.player.mode,C_PLAYER);
  }
  rect(0,0,320,20,C_INK);bar(60,5,195,player_progress(&app.player,l),app.practice?C_CYAN:C_LIME);number(264,7,player_progress(&app.player,l),1,C_WHITE);text(283,7,"%",1,C_WHITE);
  text(7,7,app.practice?"PRAC":"RUN",1,app.practice?C_CYAN:C_WHITE);
  if(app.player.tick<450){text(87,52,"ATTEMPT",2,C_WHITE);number(184,52,app.deaths,2,C_WHITE);}
  if(app.practice){text(8,229,"0 CHECKPOINT  DEL REMOVE",1,C_CYAN);}
  for(unsigned i=0;i<3;i++){circle(282+(int)i*13,231,4,i<app.player.coins?C_YELLOW:C_GLOW,i<app.player.coins);}
}
void app_render(void) {
  gfx_palette(0x2199,0x0153,app.save.color);backdrop((int)app.time/6,app.time,false);
  switch(app.screen) {
    case HOME: {
      text(57,35,"NUMDASH",5,C_BLACK);text(55,31,"NUMDASH",5,C_LIME);centered(76,"GEOMETRY IN MOTION",1,C_WHITE);
      line(70,92,250,92,C_GLOW);
      circle(160,136,31,C_BLACK,true);circle(158,133,31,C_LIME,true);circle(158,133,28,C_WHITE,false);triangle(150,116,150,150,173,133,C_BLACK);triangle(153,121,153,145,169,133,C_WHITE);
      player_icon(55,152,app.time/4,false,C_PLAYER);player_icon(264,122,app.time/5,true,C_CYAN);
      const char *items[]={"PLAY","CREATE","OPTIONS","HELP"};for(int i=0;i<4;i++)button(7+i*79,183,70,items[i],app.menu==i);
      footer("LEFT / RIGHT SELECT   OK OPEN");break;
    }
    case SELECT: {
      const Level *l=&nd_levels[app.selection];gfx_palette(l->background,l->ground,app.save.color);backdrop((int)app.time/8,app.time,false);
      title("SELECT LEVEL");panel(32,52,256,144);
      circle(70,83,17,app.selection<2?C_CYAN:app.selection<4?C_YELLOW:C_LIME,true);circle(70,83,17,C_WHITE,false);rect(61,79,4,4,C_BLACK);rect(75,79,4,4,C_BLACK);line(62,89,77,89,C_BLACK);
      text(99,67,app.selection<4?"CLASSIC":"NUMDASH ORIGINAL",1,C_MUTED);label(99,83,l->name,2,C_WHITE);number(257,61,app.selection+1,1,C_YELLOW);
      text(48,113,"NORMAL",1,C_WHITE);number(235,113,app.save.best[app.selection],1,C_LIME);text(260,113,"%",1,C_WHITE);bar(48,125,224,app.save.best[app.selection],C_LIME);
      text(48,148,"PRACTICE",1,C_WHITE);number(235,148,app.save.practice[app.selection],1,C_CYAN);text(260,148,"%",1,C_WHITE);bar(48,160,224,app.save.practice[app.selection],C_CYAN);
      for(int i=0;i<3;i++)circle(147+i*13,184,4,app.save.coins[app.selection]>i?C_YELLOW:C_GLOW,app.save.coins[app.selection]>i);
      text(10,120,"<",2,C_WHITE);text(300,120,">",2,C_WHITE);
      for(int i=0;i<7;i++)circle(124+i*12,208,3,i==app.selection?C_WHITE:C_GLOW,i==app.selection);
      footer("OK PLAY  TOOLBOX PRACTICE  BACK MENU");break;
    }
    case SLOTS:
      title("MY LEVELS");
      for(int i=0;i<3;i++){int y=56+i*45;panel(30,y,260,36);if(app.slot==i)outline(29,y-1,262,38,C_LIME);text(43,y+8,"MY LEVEL",1,C_WHITE);number(99,y+8,i+1,1,C_WHITE);number(43,y+22,app.save.custom[i].count,1,C_MUTED);text(69,y+22,"OBJECTS",1,C_MUTED);text(202,y+14,app.slot==i?"< EDIT >":"",1,C_LIME);}
      centered(202,app.save_ok?"SAVED ON DEVICE":"SAVE: VAR IN EDITOR",1,C_MUTED);footer("UP / DOWN  OK EDIT  EXE PLAY");break;
    case EDITOR:world(true);break;
    case PLAY:world(false);break;
    case PAUSE:
      world(false);panel(61,38,198,174);centered(51,"PAUSED",2,C_WHITE);
      {const char *items[]={"RESUME","RESTART",app.practice?"NORMAL MODE":"PRACTICE MODE",app.testing?"BACK TO EDITOR":"LEVEL SELECT"};for(int i=0;i<4;i++)button(83,79+i*30,154,items[i],app.menu==i);}
      break;
    case COMPLETE:
      world(false);panel(34,50,252,147);centered(67,app.practice?"PRACTICE DONE!":"LEVEL COMPLETE!",2,C_LIME);
      text(62,103,"ATTEMPTS",1,C_WHITE);number(216,103,app.deaths,1,C_YELLOW);text(62,124,"COINS",1,C_WHITE);number(216,124,app.player.coins,1,C_YELLOW);
      bar(63,147,194,100,app.practice?C_CYAN:C_LIME);centered(176,app.testing?"OK RETURN TO EDITOR":"OK CONTINUE   BACK LEVELS",1,C_WHITE);break;
    case SETTINGS:
      title("OPTIONS");panel(35,56,250,142);
      button(54,72,212,app.save.effects?"EFFECTS ON":"EFFECTS OFF",app.menu==0);
      button(54,109,212,"ICON COLOR",app.menu==1);player_icon(246,121,0,false,C_PLAYER);
      button(54,146,212,app.save.fps?"FPS DISPLAY ON":"FPS DISPLAY OFF",app.menu==2);
      footer("UP / DOWN SELECT  OK CHANGE  BACK SAVE");break;
    case PROPERTIES:
      title("LEVEL SETTINGS");panel(40,65,240,127);button(60,83,200,"COLOR THEME",app.menu==0);number(233,91,app.save.custom[app.slot].theme+1,1,C_BLACK);button(60,122,200,"PULSE BPM",app.menu==1);number(224,130,app.save.custom[app.slot].bpm,1,C_BLACK);
      footer("LEFT / RIGHT CHANGE  BACK EDITOR");break;
    case HELP:
      title(app.help_return==EDITOR?"EDITOR CONTROLS":"HOW TO PLAY");panel(14,49,292,163);
      if(app.help_return==EDITOR){const char *rows[]={"ARROWS     MOVE GRID CURSOR","OK         PLACE OBJECT","TOOLBOX +/- CHANGE OBJECT","SHIFT      ROTATE 90 DEGREES","BACKSPACE  ERASE OBJECT","ALPHA      UNDO LAST EDIT","XNT        PICK OBJECT","VAR        SAVE LEVEL","LN         THEME / PULSE BPM","EXE        PLAYTEST   0 HELP","BACK       SAVE AND LEAVE"};for(int i=0;i<11;i++)text(24,58+i*13,rows[i],1,C_WHITE);}
      else {const char *rows[]={"OK / EXE / UP: JUMP OR FLY","HOLD TO REPEAT CUBE JUMPS","HOLD TO RISE IN SHIP MODE","TAP AGAIN ON A YELLOW ORB","YELLOW PADS JUMP AUTOMATICALLY","PORTALS CHANGE MODE / GRAVITY","BACK PAUSES THE GAME","PRACTICE: 0 SETS A CHECKPOINT","BACKSPACE REMOVES CHECKPOINT","PROGRESS SAVES ON EXIT","HOME SAVES AND EXITS THE APP"};for(int i=0;i<11;i++)text(24,58+i*13,rows[i],1,C_WHITE);}
      footer("OK / BACK RETURN");break;
  }
  if(app.notice_until>app.time){rect(0,212,320,10,C_BLACK);centered(213,app.notice,1,C_YELLOW);}
  if(app.save.fps){rect(0,22,48,10,C_INK);number(3,24,app.fps,1,C_WHITE);text(22,24,"FPS",1,C_WHITE);}
  /* Short palette fade on screen changes; no extra framebuffer or allocation. */
  unsigned age=app.time-app.screen_time;
  if(age<32&&app.screen!=PLAY)for(int i=0;i<16;i++)palette[i]=color_mix(0,palette[i],age+16,48);
}
