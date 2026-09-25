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
static Object *editor_at_cursor(CustomLevel *c) {
  for(unsigned i=0;i<c->count;i++)if(c->objects[i].x==app.cursor_x &&
      (c->objects[i].y==app.cursor_y||c->objects[i].y==app.cursor_y-13))return &c->objects[i];
  return NULL;
}
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
      if(accept){if(app.menu==0){app.selection=0;screen(SELECT);}else if(app.menu==1)screen(SLOTS);else if(app.menu==2){app.settings_return=HOME;screen(SETTINGS);}else {app.help_return=HOME;screen(HELP);}}
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
      else if(hit&K_OK){app.cursor_x=15;app.cursor_y=15;app.edit_camera=0;app.undo_count=0;app.edit_mode=0;screen(EDITOR);}
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
      if(hit&K_OK){
        if(app.edit_mode==0){undo_snapshot();Object o={(int16_t)app.cursor_x,(int16_t)app.cursor_y,brushes[app.brush],app.rotate,0,0,0};if(o.id==35||o.id==67||o.id==9)o.y-=13;if(editor_put(c,o)){edit_reset_progress();}else notify("LEVEL FULL - DELETE AN OBJECT");}
        else if(app.edit_mode==1){Object *o=editor_at_cursor(c);if(o){undo_snapshot();o->rot=(o->rot+1)%4;edit_reset_progress();}}
        else {Object *o=editor_at_cursor(c);if(o){int y=o->y;undo_snapshot();editor_remove(c,app.cursor_x,y);edit_reset_progress();}}
      }
      if(hit&K_ERASE){undo_snapshot();bool removed=editor_remove(c,app.cursor_x,app.cursor_y);removed|=editor_remove(c,app.cursor_x,app.cursor_y-13);if(removed)edit_reset_progress();}
      if(hit&K_UNDO){if(app.undo_count){*c=app.undo;app.undo_count=0;edit_reset_progress();notify("UNDONE");}else notify("NOTHING TO UNDO");}
      if(hit&K_COPY){Object *o=editor_at_cursor(c);if(o){for(unsigned j=0;j<BRUSHES;j++)if(brushes[j]==o->id)app.brush=(uint8_t)j;app.rotate=o->rot;}}
      if(hit&K_SAVE){app_save();notify(app.save_ok?"LEVEL SAVED":"SAVE FAILED - STORAGE UNAVAILABLE");}
      if(hit&K_PROPS)screen(PROPERTIES);
      if(hit&K_CHECK){if(keys&K_SHIFT){app.help_return=EDITOR;screen(HELP);}else app.edit_mode=(app.edit_mode+1)%3;}
      break;
    }
    case PROPERTIES: {
      CustomLevel *c=&app.save.custom[app.slot];
      if(hit&K_UP)app.menu=(app.menu+2)%3;
      if(hit&K_DOWN)app.menu=(app.menu+1)%3;
      if(hit&(K_LEFT|K_RIGHT)){
        int delta=(hit&K_RIGHT)?1:-1;
        if(app.menu==0)c->theme=(uint8_t)((c->theme+delta+6)%6);
        else if(app.menu==1){int bpm=c->bpm+delta*5;if(bpm>=60&&bpm<=200)c->bpm=(uint8_t)bpm;}
        else {int length=c->length+delta*150;int min=300;if(c->count&&min<c->objects[c->count-1].x+300)min=c->objects[c->count-1].x+300;if(length>=min&&length<=32000)c->length=(uint16_t)length;}
        app.dirty=true;
      }
      if(hit&K_BACK||accept)screen(EDITOR);
      break;
    }
    case PLAY:
      if(hit&K_BACK){remember_progress();screen(PAUSE);app.menu=2;break;}
      if(app.player.dead){app.death_timer++;if(app.death_timer>=180 || (app.death_timer>48&&accept))restart();break;}
      if(app.practice&&hit&K_CHECK){app.checkpoint=app.player;app.has_checkpoint=true;notify("CHECKPOINT SET");}
      if(app.practice&&hit&K_ERASE){app.has_checkpoint=false;notify("CHECKPOINT REMOVED");}
      player_step(&app.player,&app.level,(keys&(K_OK|K_EXE|K_UP))!=0);
      if(app.player.dead){remember_progress();app.death_timer=0;}
      if(app.player.complete){remember_progress();if(!app.testing)app_save();screen(COMPLETE);}
      break;
    case PAUSE:
      if(hit&(K_UP|K_LEFT))app.menu=(app.menu+4)%5;
      if(hit&(K_DOWN|K_RIGHT))app.menu=(app.menu+1)%5;
      if(hit&K_BACK){screen(PLAY);break;}
      if(accept){if(app.menu==0){app.settings_return=PAUSE;screen(SETTINGS);}else if(app.menu==1){app.practice=!app.practice;app.has_checkpoint=false;restart();screen(PLAY);}else if(app.menu==2)screen(PLAY);else if(app.menu==3)leave_play();else {app.has_checkpoint=false;restart();screen(PLAY);}}
      break;
    case COMPLETE:
      if(app.time-app.screen_time<120)break;
      if(hit&(K_LEFT|K_RIGHT))app.menu^=1;
      if(hit&K_BACK)leave_play();
      else if(accept){if(app.menu==0)app_start(app.selection,false,app.testing);else leave_play();}
      break;
    case SETTINGS:
      if(hit&K_UP)app.menu=(app.menu+2)%3;
      if(hit&K_DOWN)app.menu=(app.menu+1)%3;
      if(accept||hit&(K_LEFT|K_RIGHT)){if(app.menu==0)app.save.effects^=1;else if(app.menu==1)app.save.percent^=1;else app.save.fps^=1;app.dirty=true;}
      if(hit&K_BACK){app_save();screen(app.settings_return);if(app.screen==PAUSE)app.menu=2;}
      break;
    case HELP:if(accept||hit&K_BACK)screen(app.help_return);break;
  }
}

static void title(const char *s){centered(20,s,2,C_BLACK);centered(18,s,2,C_WHITE);}
static void panel(int x,int y,int w,int h){rect(x+3,y+4,w,h,C_BLACK);rect(x,y,w,h,C_INK);outline(x,y,w,h,C_GLOW);outline(x+2,y+2,w-4,h-4,C_BG3);}
static void footer(const char *s){rect(0,222,320,18,C_INK);centered(228,s,1,C_MUTED);}
static void button(int x,int y,int w,const char *s,bool active){rect(x+2,y+3,w,24,C_BLACK);rect(x,y,w,24,C_LIME);outline(x,y,w,24,active?C_YELLOW:C_WHITE);text(x+(w-(int)strlen(s)*6)/2,y+9,s,1,C_BLACK);}
static void chunky(int x,int y,const char *s,int scale,int c){
  text(x+2,y+3,s,scale,C_BLACK);
  text(x-1,y,s,scale,C_BLACK);text(x+1,y,s,scale,C_BLACK);
  text(x,y-1,s,scale,C_BLACK);text(x,y+1,s,scale,C_BLACK);
  text(x,y,s,scale,c);
}
static void chunky_centered(int y,const char *s,int scale,int c){chunky((320-(int)strlen(s)*6*scale+scale)/2,y,s,scale,c);}
static void gd_bar(int x,int y,int w,int h,int percent,int color){
  rect(x+2,y+2,w,h,C_BLACK);rect(x,y,w,h,C_INK);outline(x,y,w,h,C_WHITE);
  if(percent>0){int fill=(w-4)*percent/100;if(fill<1)fill=1;rect(x+2,y+2,fill,h-4,color);}
}
static void gd_round(int x,int y,int r,bool selected){
  circle(x+2,y+3,r+2,C_BLACK,true);circle(x,y,r+2,C_WHITE,true);
  circle(x,y,r,C_BLACK,true);circle(x,y,r-2,C_LIME,true);
  circle(x-r/3,y-r/3,r/3,C_GLOW,true);
  if(selected)circle(x,y,r+3,C_YELLOW,false);
}
static void gd_cross(int x,int y,int r,bool selected){
  int b=r/2;
  rect(x-b+2,y-r+3,b*2,r*2,C_BLACK);rect(x-r+2,y-b+3,r*2,b*2,C_BLACK);
  rect(x-b-2,y-r-2,b*2+4,r*2+4,C_WHITE);rect(x-r-2,y-b-2,r*2+4,b*2+4,C_WHITE);
  rect(x-b,y-r,b*2,r*2,C_BLACK);rect(x-r,y-b,r*2,b*2,C_BLACK);
  rect(x-b+2,y-r+2,b*2-4,r*2-4,C_LIME);
  rect(x-r+2,y-b+2,r*2-4,b*2-4,C_LIME);
  if(selected)outline(x-r-3,y-r-3,r*2+6,r*2+6,C_YELLOW);
  rect(x-b+3,y-r+3,b*2-6,3,C_GLOW);rect(x-r+3,y-b+3,r*2-6,3,C_GLOW);
  rect(x-r+3,y-b+4,r/2,5,C_CYAN);rect(x+r-r/2-3,y-b+4,r/2,5,C_CYAN);
}
static void gd_arrow(int x,int y,bool right,int color){
  int d=right?1:-1;triangle(x-8*d,y-15,x-8*d,y+15,x+12*d,y,C_BLACK);
  triangle(x-8*d,y-13,x-8*d,y+12,x+10*d,y,C_WHITE);
  triangle(x-6*d,y-10,x-6*d,y+9,x+7*d,y,color);
}
static void gd_icon(int x,int y,int kind,int r,bool selected){
  gd_round(x,y,r,selected);
  switch(kind){
    case 0:
      for(int i=0;i<9;i++){int dx=(i%3-1)*7,dy=(i/3-1)*7;if(i==4)continue;rect(x+dx-2,y+dy-2,5,5,C_BLACK);}
      circle(x,y,9,C_BLACK,true);circle(x,y,6,C_CYAN,true);circle(x,y,3,C_BLACK,true);break;
    case 1:
      triangle(x,y-13,x-9,y,x+9,y,C_WHITE);triangle(x,y+13,x-9,y,x+9,y,C_WHITE);
      triangle(x,y-10,x-6,y,x+6,y,C_CYAN);triangle(x,y+10,x-6,y,x+6,y,C_CYAN);break;
    case 2:
      triangle(x-8,y-12,x-8,y+12,x+12,y,C_BLACK);
      triangle(x-6,y-10,x-6,y+10,x+10,y,C_YELLOW);break;
    case 3:
      for(int i=-1;i<=1;i++){circle(x-8,y+i*7,2,C_CYAN,true);rect(x-3,y+i*7-2,13,4,C_BLACK);rect(x-2,y+i*7-1,11,2,C_CYAN);}break;
    case 4:
      circle(x,y,10,C_CYAN,false);rect(x-13,y-6,7,9,selected?C_LIME:C_BG3);
      triangle(x-10,y-7,x-14,y+2,x-4,y+1,C_CYAN);break;
  }
}
static void dim_scene(void){
  static const uint8_t map[16]={0,1,2,3,4,5,6,2,8,2,2,2,2,2,14,2};
  for(unsigned i=0;i<sizeof(frame);i++){uint8_t b=frame[i];frame[i]=(uint8_t)(map[b&15]|(map[b>>4]<<4));}
  int dark[]={C_BG,C_BG2,C_BG3,C_GROUND,C_GROUND2,C_GLOW,C_INK};
  for(unsigned i=0;i<sizeof(dark)/sizeof(dark[0]);i++)palette[dark[i]]=color_mix(palette[dark[i]],0,2,3);
}
static void world(bool editing) {
  const Level *l=&app.level; Level custom;
  float camera,cy;
  if(editing){custom=custom_level(&app.save.custom[app.slot],app.slot);l=&custom;camera=app.cursor_x-180;if(camera<0)camera=0;cy=app.cursor_y>210?app.cursor_y-210:0;}
  else {camera=app.player.x-115;if(camera<-70)camera=-70;cy=app.player.camera_y;}
  gfx_palette(editing?l->background:app.player.bg,editing?l->ground:app.player.ground,0);
  backdrop((int)(camera*.6f),app.time,false);
  int base_floor=editing?158:184;
  int floor_y=base_floor+(int)(cy*.6f);
  if(editing){for(int x=-((int)(camera*.6f)%18+18)%18;x<320;x+=18)line(x,0,x,177,C_INK);for(int y=((floor_y%18)+18)%18;y<178;y+=18)line(0,y,319,y,C_INK);}
  unsigned begin=level_lower_bound(l,camera-90);
  for(unsigned i=begin;i<l->count&&l->objects[i].x<camera+650;i++) {
    const Object *o=&l->objects[i];int x=(int)((o->x-camera)*.6f),y=floor_y-(int)(o->y*.6f);
    if(y>=-40&&y<260)object_draw(o,x,y,!editing&&player_used(&app.player,i));
  }
  if(!editing&&app.player.mode){int y=floor_y-(int)(app.player.ceiling*.6f);rect(0,0,320,y,C_GROUND2);rect(0,y,320,2,C_WHITE);floor_y-= (int)(app.player.floor*.6f);}
  rect(0,floor_y,320,240-floor_y,C_GROUND);rect(0,floor_y+24,320,240-floor_y-24,C_GROUND2);
  rect(0,floor_y,320,2,C_WHITE);rect(0,floor_y+3,320,2,C_GLOW);
  unsigned beat_ticks=14400u/(l->bpm?l->bpm:120);
  unsigned beat_phase=(editing?app.time:app.player.tick)%beat_ticks;
  if(app.save.effects&&beat_phase<12)rect(0,floor_y-2,320,2,C_GLOW);
  for(int x=-((int)(camera*.6f)%38);x<320;x+=38)outline(x,floor_y+8,37,40,C_GROUND2);
  int finish=(int)((l->length-camera)*.6f);if(finish<320){rect(finish,28,3,180,C_WHITE);for(int y=30;y<208;y+=12)rect(finish+3,y,6,6,C_LIME);}
  if(editing){
    int x=(int)((app.cursor_x-camera)*.6f),y=base_floor+(int)(cy*.6f)-(int)(app.cursor_y*.6f);
    outline(x-11,y-11,23,23,C_YELLOW);line(x-15,y,x-11,y,C_YELLOW);line(x+11,y,x+15,y,C_YELLOW);
    if(app.edit_mode==0){Object ghost={(int16_t)app.cursor_x,(int16_t)app.cursor_y,brushes[app.brush],app.rotate,0,0,0};object_draw(&ghost,x,y,false);}
    rect(0,0,320,20,C_INK);const char *mode_names[]={"BUILD","EDIT","DELETE"};text(6,4,mode_names[app.edit_mode],1,C_LIME);text(60,4,brush_names[app.brush],1,C_WHITE);
    text(7,13,"X",1,C_MUTED);number(19,13,app.cursor_x/30,1,C_WHITE);text(65,13,"Y",1,C_MUTED);number(77,13,app.cursor_y/30,1,C_WHITE);text(124,13,"ROT",1,C_MUTED);number(148,13,app.rotate*90,1,C_WHITE);number(244,13,l->count,1,C_WHITE);text(268,13,"/384",1,C_MUTED);
    rect(0,176,320,64,C_INK);rect(0,176,320,2,C_WHITE);
    const char *tabs[]={"B","E","D"};for(int i=0;i<3;i++){rect(4,182+i*17,26,15,i==app.edit_mode?C_LIME:C_BG3);outline(4,182+i*17,26,15,C_WHITE);text(14,186+i*17,tabs[i],1,C_BLACK);}
    for(int i=0;i<6;i++){
      unsigned j=(app.brush/6)*6+i;int bx=35+i*47;
      rect(bx+2,183,42,34,C_BLACK);rect(bx,181,42,34,j==app.brush?C_BG3:C_GROUND2);outline(bx,181,42,34,j==app.brush?C_YELLOW:C_MUTED);
      Object icon={0,0,brushes[j],0,0,0,0};object_draw(&icon,bx+21,198,false);
      number(bx+17,217,j+1,1,j==app.brush?C_YELLOW:C_MUTED);
    }
    const char *tips[]={"OK PLACE  TOOL OBJECT  0 MODE","OK ROTATE  XNT PICK  0 MODE","OK DELETE  ALPHA UNDO  0 MODE"};text(40,231,tips[app.edit_mode],1,C_WHITE);return;
  }
  int px=(int)((app.player.x-camera)*.6f),py=base_floor+(int)(cy*.6f)-(int)(app.player.y*.6f);
  if(app.player.dead){
    if(app.save.effects)for(int i=0;i<24;i++){int dx=(i*17%19)-9,dy=(i*11%23)-11;int t=app.death_timer/4;rect(px+dx*t/5,py+dy*t/5+t*t/180,3,3,i%2?C_PLAYER:C_WHITE);}
    /* The burst is the feedback; the original game immediately retries. */
  }else{
    if(app.save.effects){for(int i=1;i<=4;i++){int off=i*6;rect(px-off-8,py+6,3,3,i%2?C_GLOW:C_PLAYER);}if(app.player.mode){triangle(px-13,py,px-25-(int)(app.time%7),py+4,px-13,py+6,C_ORANGE);}}
    player_icon(px,py,app.player.rotation,app.player.mode,C_PLAYER);
  }
  gd_bar(79,5,159,8,player_progress(&app.player,l),app.practice?C_CYAN:C_LIME);
  if(app.save.percent){number(244,5,player_progress(&app.player,l),1,C_WHITE);text(262,5,"%",1,C_WHITE);}
  circle(304,13,10,C_WHITE,false);rect(301,8,2,10,C_WHITE);rect(306,8,2,10,C_WHITE);
  if(app.player.tick<430){chunky(10,57,"ATTEMPT",2,C_WHITE);number(113,57,app.deaths,2,C_WHITE);}
  if(app.practice)text(4,228,"0 CHECKPOINT    DEL REMOVE",1,C_CYAN);
}
void app_render(void) {
  gfx_palette(0x2c78,0x1454,0);backdrop((int)app.time/6,app.time,false);
  switch(app.screen) {
    case HOME: {
      rect(0,198,320,42,C_GROUND);rect(0,198,320,2,C_WHITE);
      for(int x=-(int)(app.time/7)%56;x<320;x+=56)outline(x,205,55,35,C_GROUND2);
      chunky_centered(26,"GEOMETRY DASH",3,C_LIME);
      gd_cross(160,123,31,app.menu==0);triangle(149,105,149,141,180,123,C_BLACK);triangle(151,108,151,138,177,123,C_YELLOW);
      gd_cross(66,124,23,false);rect(52,111,27,27,C_BLACK);rect(55,114,21,21,C_YELLOW);rect(59,120,4,4,C_BLACK);rect(68,120,4,4,C_BLACK);rect(61,129,11,3,C_BLACK);
      gd_cross(254,124,23,app.menu==1);for(int k=-1;k<=1;k++){line(241+k,135,267+k,113,C_BLACK);line(241+k,113,267+k,135,C_BLACK);}line(241,135,267,113,C_YELLOW);line(241,113,267,135,C_YELLOW);
      gd_icon(118,179,0,17,app.menu==2);gd_icon(203,179,3,17,app.menu==3);
      player_icon(31,188,app.time/3,false,C_PLAYER);
      text(8,225,"< > SELECT      OK OPEN",1,C_WHITE);break;
    }
    case SELECT: {
      const Level *l=&nd_levels[app.selection];gfx_palette(0x095f,0x0153,0);backdrop((int)app.time/9,app.time,false);
      rect(0,208,320,32,C_BG2);rect(0,207,320,2,C_WHITE);
      circle(18,23,17,C_BLACK,true);circle(17,22,16,C_LIME,true);circle(17,22,16,C_WHITE,false);
      gd_arrow(17,22,false,C_WHITE);gd_arrow(18,108,false,C_WHITE);gd_arrow(302,108,true,C_WHITE);
      rect(43,54,237,78,C_INK);
      circle(73,83,18,C_BLACK,true);circle(72,82,17,l->difficulty<3?C_CYAN:C_LIME,true);circle(72,82,17,C_WHITE,false);
      rect(63,76,5,5,C_BLACK);rect(78,76,5,5,C_BLACK);
      if(l->difficulty<3){rect(65,89,17,3,C_BLACK);rect(68,92,11,2,C_PINK);}
      else {rect(65,92,17,3,C_BLACK);rect(67,89,3,3,C_BLACK);rect(78,89,3,3,C_BLACK);}
      chunky(99,72,l->name,2,C_WHITE);
      triangle(263,55,257,67,269,67,C_YELLOW);triangle(257,59,269,59,263,71,C_YELLOW);number(248,55,l->difficulty,1,C_WHITE);
      for(int i=0;i<3;i++){int x=216+i*18;circle(x,110,7,i<app.save.coins[app.selection]?C_YELLOW:C_MUTED,true);circle(x,110,7,C_WHITE,false);}
      chunky_centered(137,"NORMAL MODE",1,C_WHITE);
      gd_bar(50,151,200,13,app.save.best[app.selection],C_LIME);
      number(259,154,app.save.best[app.selection],1,C_WHITE);text(277,154,"%",1,C_WHITE);
      chunky_centered(174,"PRACTICE MODE",1,C_WHITE);
      gd_bar(50,188,200,13,app.save.practice[app.selection],C_CYAN);
      number(259,191,app.save.practice[app.selection],1,C_WHITE);text(277,191,"%",1,C_WHITE);
      for(int i=0;i<7;i++)circle(124+i*12,217,2,i==app.selection?C_WHITE:C_MUTED,true);
      centered(227,"OK PLAY    TOOLBOX PRACTICE",1,C_WHITE);break;
    }
    case SLOTS:
      gfx_palette(0x095f,0x8a43,0);backdrop((int)app.time/9,app.time,false);
      chunky_centered(16,"MY LEVELS",2,C_WHITE);
      rect(23,48,274,151,C_BLACK);rect(25,46,270,151,C_LIME);rect(30,51,260,140,C_GROUND);
      for(int i=0;i<3;i++){int y=58+i*42;rect(39,y+2,244,36,C_BLACK);rect(37,y,244,36,app.slot==i?C_BG3:C_INK);outline(37,y,244,36,app.slot==i?C_YELLOW:C_WHITE);
        text(48,y+7,"MY LEVEL",1,C_WHITE);number(102,y+7,i+1,1,C_WHITE);number(49,y+22,app.save.custom[i].count,1,C_MUTED);text(72,y+22,"OBJECTS",1,C_MUTED);
        gd_icon(251,y+17,2,13,app.slot==i);}
      centered(202,app.save_ok?"SAVED":"VAR SAVES IN EDITOR",1,C_WHITE);footer("UP DOWN SELECT  OK EDIT  EXE PLAY");break;
    case EDITOR:world(true);break;
    case PLAY:world(false);break;
    case PAUSE: {
      world(false);dim_scene();
      chunky_centered(27,app.level.name,2,C_WHITE);
      chunky_centered(65,"NORMAL MODE",1,C_WHITE);gd_bar(60,80,200,12,app.save.best[app.selection],C_LIME);
      number(260,82,app.save.best[app.selection],1,C_WHITE);text(278,82,"%",1,C_WHITE);
      chunky_centered(104,"PRACTICE MODE",1,C_WHITE);gd_bar(60,119,200,12,app.save.practice[app.selection],C_CYAN);
      number(260,121,app.save.practice[app.selection],1,C_WHITE);text(278,121,"%",1,C_WHITE);
      static const int positions[5]={38,99,160,221,282};for(int i=0;i<5;i++)gd_icon(positions[i],171,i,i==2?25:21,app.menu==i);
      const char *items[]={"SETTINGS",app.practice?"NORMAL MODE":"PRACTICE MODE","RESUME",app.testing?"BACK TO EDITOR":"LEVEL SELECT","RESTART"};
      chunky_centered(210,items[app.menu],1,C_WHITE);centered(229,"< > SELECT   OK OPEN   BACK RESUME",1,C_MUTED);break;
    }
    case COMPLETE: {
      world(false);
      unsigned age=app.time-app.screen_time;
      if(age<120){
        for(int i=0;i<48;i++){int x=(int)((i*47+age*(i%3+1))%320),y=(int)((i*73+age*(i%5+1))%210);rect(x,y,2+i%3,2+i%3,i%3?C_CYAN:C_WHITE);}
        chunky_centered(99,"LEVEL COMPLETE!",2,C_LIME);
      }else{
        dim_scene();rect(43,36,234,166,C_BLACK);rect(47,36,226,166,C_LIME);rect(51,40,218,158,C_INK);
        rect(44,37,16,8,C_CYAN);rect(260,37,16,8,C_CYAN);rect(44,195,16,8,C_CYAN);rect(260,195,16,8,C_CYAN);
        chunky_centered(53,app.practice?"PRACTICE COMPLETE!":"LEVEL COMPLETE!",2,C_LIME);
        text(78,90,"ATTEMPTS",1,C_YELLOW);number(206,90,app.deaths,1,C_WHITE);
        text(78,109,"JUMPS",1,C_YELLOW);number(206,109,app.player.jumps,1,C_WHITE);
        text(78,128,"TIME",1,C_YELLOW);number(198,128,app.player.tick/14400,1,C_WHITE);text(210,128,":",1,C_WHITE);
        if((app.player.tick/240)%60<10)text(216,128,"0",1,C_WHITE);number(222,128,(app.player.tick/240)%60,1,C_WHITE);
        for(int i=0;i<3;i++){circle(135+i*24,161,8,i<app.player.coins?C_YELLOW:C_MUTED,true);circle(135+i*24,161,8,C_WHITE,false);}
        gd_icon(104,205,4,19,app.menu==0);gd_icon(216,205,3,19,app.menu==1);
        text(83,229,"REPLAY",1,C_WHITE);text(195,229,"LEVELS",1,C_WHITE);
      }break;
    }
    case SETTINGS: {
      chunky_centered(19,"OPTIONS",2,C_WHITE);panel(38,55,244,145);
      static const char *options[]={"PULSE EFFECTS","SHOW PERCENT","SHOW FPS"};
      const int values[]={app.save.effects,app.save.percent,app.save.fps};
      for(int i=0;i<3;i++){
        int y=78+i*39;rect(53,y-8,214,30,app.menu==i?C_BG3:C_INK);
        if(app.menu==i)outline(53,y-8,214,30,C_YELLOW);
        text(63,y,options[i],1,C_WHITE);
        rect(232,y-3,19,19,C_BLACK);outline(232,y-3,19,19,C_WHITE);
        if(values[i]){rect(235,y,13,13,C_LIME);line(237,y+6,240,y+9,C_BLACK);line(240,y+9,246,y+2,C_BLACK);}
      }
      footer("UP DOWN SELECT  OK TOGGLE  BACK SAVE");break;
    }
    case PROPERTIES:
      title("LEVEL SETTINGS");panel(40,60,240,145);button(60,74,200,"COLOR THEME",app.menu==0);number(233,82,app.save.custom[app.slot].theme+1,1,C_BLACK);button(60,111,200,"PULSE BPM",app.menu==1);number(224,119,app.save.custom[app.slot].bpm,1,C_BLACK);button(60,148,200,"LENGTH",app.menu==2);number(220,156,app.save.custom[app.slot].length/30,1,C_BLACK);
      footer("LEFT RIGHT CHANGE  BACK EDITOR");break;
    case HELP:
      title(app.help_return==EDITOR?"EDITOR CONTROLS":"HOW TO PLAY");panel(14,49,292,163);
      if(app.help_return==EDITOR){const char *rows[]={"ARROWS     MOVE GRID CURSOR","0          BUILD EDIT DELETE","OK         USE CURRENT TOOL","TOOLBOX +/- CHANGE OBJECT","SHIFT      ROTATE 90 DEGREES","BACKSPACE  ERASE OBJECT","ALPHA      UNDO LAST EDIT","XNT        PICK OBJECT","VAR        SAVE LEVEL","LN         THEME BPM LENGTH","EXE        PLAYTEST  BACK EXIT"};for(int i=0;i<11;i++)text(24,58+i*13,rows[i],1,C_WHITE);}
      else {const char *rows[]={"OK / EXE / UP: JUMP OR FLY","HOLD TO REPEAT CUBE JUMPS","HOLD TO RISE IN SHIP MODE","TAP AGAIN ON A YELLOW ORB","YELLOW PADS JUMP AUTOMATICALLY","PORTALS CHANGE MODE / GRAVITY","BACK PAUSES THE GAME","PRACTICE: 0 SETS A CHECKPOINT","BACKSPACE REMOVES CHECKPOINT","PROGRESS SAVES ON EXIT","HOME SAVES AND EXITS THE APP"};for(int i=0;i<11;i++)text(24,58+i*13,rows[i],1,C_WHITE);}
      footer("OK / BACK RETURN");break;
  }
  if(app.notice_until>app.time){rect(0,212,320,10,C_BLACK);centered(213,app.notice,1,C_YELLOW);}
  if(app.save.fps){rect(0,22,48,10,C_INK);number(3,24,app.fps,1,C_WHITE);text(22,24,"FPS",1,C_WHITE);}
  /* Short palette fade on screen changes; no extra framebuffer or allocation. */
  unsigned age=app.time-app.screen_time;
  if(age<32&&app.screen!=PLAY)for(int i=0;i<16;i++)palette[i]=color_mix(0,palette[i],age+16,48);
}
